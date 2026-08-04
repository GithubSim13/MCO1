#include "ProcessScheduler.h"
#include "ConfigManager.h"
#include "ConsoleManager.h"
#include "ScreenManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <random>

ProcessScheduler* ProcessScheduler::instance = nullptr;

ProcessScheduler* ProcessScheduler::getInstance() {
    return instance;
}

void ProcessScheduler::initialize() {
    instance = new ProcessScheduler();
    ConfigManager* config = ConfigManager::getInstance();

    for (int i = 0; i < config->numCpu; i++) {
        Core* core = new Core(i);

        // Lets Core hand a preempted/sleeping process back to our queue.
        core->onPreempt = [](Process* p) {
            ProcessScheduler::getInstance()->requeueProcess(p);
        };

        instance->cores.push_back(core);
        core->start();
    }
}

void ProcessScheduler::destroy() {
    delete instance;
    instance = nullptr;
}

void ProcessScheduler::addProcess(Process* process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    readyQueue.push(process);
    allProcesses.push_back(process);
}

void ProcessScheduler::startScheduler() {
    running = true;
}

void ProcessScheduler::stopScheduler() {
    running = false;
}

// Called by Core's onPreempt lambda (RR preemption and SLEEP relinquishment)
void ProcessScheduler::requeueProcess(Process* process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (process->sleepTicks > 0) {
        int wakeAt = cpuCycle.load() + process->sleepTicks;
        process->sleepTicks = 0;
        sleepingQueue.push_back({ wakeAt, process });
    } else {
        readyQueue.push(process);
    }
}

static String makeTimestamp() {
    time_t now = time(0);
    tm ltm;
#ifdef _WIN32
    localtime_s(&ltm, &now);
#else
    localtime_r(&now, &ltm);
#endif
    std::ostringstream oss;
    int h12 = (ltm.tm_hour % 12 == 0) ? 12 : ltm.tm_hour % 12;
    oss << std::setfill('0')
        << "(" << std::setw(2) << (ltm.tm_mon + 1) << "/" << std::setw(2) << ltm.tm_mday
        << "/" << (1900 + ltm.tm_year)
        << " " << std::setw(2) << h12
        << ":" << std::setw(2) << ltm.tm_min
        << ":" << std::setw(2) << ltm.tm_sec
        << (ltm.tm_hour >= 12 ? "PM" : "AM") << ")";
    return oss.str();
}

// Rolls a random process size in [min-mem-per-proc, max-mem-per-proc].
static size_t rollProcessMemSize(int minMemPerProc, int maxMemPerProc) {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(minMemPerProc, maxMemPerProc);
    return static_cast<size_t>(dist(rng));
}

void ProcessScheduler::generateBatchProcess() {
    ConfigManager* config = ConfigManager::getInstance();

    int pid = nextPid.fetch_add(1);
    std::ostringstream nameStream;
    nameStream << "p" << std::setfill('0') << std::setw(2) << pid;
    String name = nameStream.str();

    size_t memSize = rollProcessMemSize(config->minMemPerProc, config->maxMemPerProc);

    void* allocatedPtr = ConsoleManager::getInstance()->getMemoryAllocator()->allocate(memSize);
    if (allocatedPtr == nullptr) {
        nextPid.fetch_sub(1); // skip this cycle, don't burn a PID
        return;
    }

    Process* p = new Process(name, pid, config->minIns, memSize);
    p->creationTime = makeTimestamp();
    p->generateInstructions(config->minIns, config->maxIns);
    p->memoryPtr = allocatedPtr;

    addProcess(p);
}

void ProcessScheduler::run() {
    ConfigManager* config = ConfigManager::getInstance();
    int idleStreak = 0;

    // One "CPU cycle" (what batch-process-freq, SLEEP ticks, etc. are counted
    // in) advances at this fixed real-time rate, independent of how fast the
    // dispatch loop below can actually iterate. Without this decoupling,
    // removing the old fixed per-iteration sleep (needed so cores can be
    // reassigned quickly under load, for healthy CPU utilization) would also
    // make "cycle" advance at whatever speed the host machine allows -
    // meaning "batch-process-freq 60" could mean 60 cycles in a fraction of
    // a millisecond instead of a meaningfully observable pause, flooding the
    // system with far more processes than intended.
    const auto CYCLE_DURATION = std::chrono::milliseconds(10);
    auto lastCycleTime = std::chrono::steady_clock::now();

    while (true) {
        bool didWork = false;

        auto now = std::chrono::steady_clock::now();
        if (now - lastCycleTime >= CYCLE_DURATION) {
            lastCycleTime = now;
            int cycle = cpuCycle.fetch_add(1);

            // Wake any sleeping processes whose sleep period has expired
            {
                std::lock_guard<std::mutex> lock(queueMutex);
                for (auto it = sleepingQueue.begin(); it != sleepingQueue.end(); ) {
                    if (cycle >= it->first) {
                        readyQueue.push(it->second);
                        it = sleepingQueue.erase(it);
                        didWork = true;
                    } else {
                        ++it;
                    }
                }
            }

            // Generate a process every batchProcessFreq cycles.
            if (running && config->batchProcessFreq > 0 && cycle % config->batchProcessFreq == 0) {
                generateBatchProcess();
                didWork = true;
            }
        }

        // Dispatch to cores - runs every loop iteration regardless of the
        // cycle-rate gate above, so a core that just freed up gets reassigned
        // as fast as the host machine allows, not throttled to CYCLE_DURATION.
        bool dispatched = false;
        if (config->scheduler == "\"fcfs\"" || config->scheduler == "fcfs")
            dispatched = scheduleFCFS();
        else if (config->scheduler == "\"rr\"" || config->scheduler == "rr")
            dispatched = scheduleRR();
        didWork = didWork || dispatched;

        // Only back off when the cycle genuinely had nothing to do - sleeping
        // unconditionally here would throttle dispatch to this interval even
        // while cores are finishing quanta far faster than that, starving
        // CPU utilization numbers even though real throughput is happening.
        if (!didWork) {
            idleStreak++;
            if (idleStreak < 200) {
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        } else {
            idleStreak = 0;
        }
    }
}

// FCFS: assign a queued process to a free core, run to completion.
bool ProcessScheduler::scheduleFCFS() {
    std::lock_guard<std::mutex> lock(queueMutex);
    bool dispatchedAny = false;
    for (Core* core : cores) {
        if (core->isAvailable() && !readyQueue.empty()) {
            Process* next = readyQueue.front();
            readyQueue.pop();
            core->assignProcess(next, -1);   // -1 = no quantum limit
            dispatchedAny = true;
        }
    }
    return dispatchedAny;
}

// RR: assign a process for exactly quantumCycles instructions, then preempt.
bool ProcessScheduler::scheduleRR() {
    ConfigManager* config = ConfigManager::getInstance();
    std::lock_guard<std::mutex> lock(queueMutex);
    bool dispatchedAny = false;

    for (Core* core : cores) {
        if (core->isAvailable() && !readyQueue.empty()) {
            Process* next = readyQueue.front();
            readyQueue.pop();
            core->assignProcess(next, config->quantumCycles);
            dispatchedAny = true;
        }
    }
    return dispatchedAny;
}
