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

    while (true) {
        // Cycle counter advances regardless of scheduler-stop, so sleeps don't freeze.
        int cycle = cpuCycle.fetch_add(1);

        // Wake any sleeping processes whose sleep period has expired
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            for (auto it = sleepingQueue.begin(); it != sleepingQueue.end(); ) {
                if (cycle >= it->first) {
                    readyQueue.push(it->second);
                    it = sleepingQueue.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // Generate a process every batchProcessFreq cycles.
        if (running && config->batchProcessFreq > 0 && cycle % config->batchProcessFreq == 0) {
            generateBatchProcess();
        }

        // Dispatch to cores
        if (config->scheduler == "\"fcfs\"" || config->scheduler == "fcfs")
            scheduleFCFS();
        else if (config->scheduler == "\"rr\"" || config->scheduler == "rr")
            scheduleRR();

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

// FCFS: assign a queued process to a free core, run to completion.
void ProcessScheduler::scheduleFCFS() {
    std::lock_guard<std::mutex> lock(queueMutex);
    for (Core* core : cores) {
        if (core->isAvailable() && !readyQueue.empty()) {
            Process* next = readyQueue.front();
            readyQueue.pop();
            core->assignProcess(next, -1);   // -1 = no quantum limit
        }
    }
}

// RR: assign a process for exactly quantumCycles instructions, then preempt.
void ProcessScheduler::scheduleRR() {
    ConfigManager* config = ConfigManager::getInstance();
    std::lock_guard<std::mutex> lock(queueMutex);

    for (Core* core : cores) {
        if (core->isAvailable() && !readyQueue.empty()) {
            Process* next = readyQueue.front();
            readyQueue.pop();
            core->assignProcess(next, config->quantumCycles);
        }
    }
}
