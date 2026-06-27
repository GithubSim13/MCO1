#include "ProcessScheduler.h"
#include "ConfigManager.h"
#include "ScreenManager.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

ProcessScheduler* ProcessScheduler::instance = nullptr;

ProcessScheduler* ProcessScheduler::getInstance() {
    return instance;
}

void ProcessScheduler::initialize() {
    instance = new ProcessScheduler();
    ConfigManager* config = ConfigManager::getInstance();

    for (int i = 0; i < config->numCpu; i++) {
        Core* core = new Core(i);

        // Wire the preemption callback so Core can return a process to our queue
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

// -----------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------

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

// Called by Core's onPreempt lambda (RR only)
void ProcessScheduler::requeueProcess(Process* process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    readyQueue.push(process);
}

// -----------------------------------------------------------------------
// Scheduler thread main loop
// -----------------------------------------------------------------------

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
    oss << "(" << (ltm.tm_mon + 1) << "/" << ltm.tm_mday
        << "/" << (1900 + ltm.tm_year)
        << " " << std::setfill('0') << std::setw(2) << h12
        << ":" << std::setw(2) << ltm.tm_min
        << ":" << std::setw(2) << ltm.tm_sec
        << (ltm.tm_hour >= 12 ? "PM" : "AM") << ")";
    return oss.str();
}

void ProcessScheduler::generateBatchProcess() {
    ConfigManager* config = ConfigManager::getInstance();

    int pid = nextPid.fetch_add(1);
    std::ostringstream nameStream;
    nameStream << "p" << std::setfill('0') << std::setw(2) << pid;
    String name = nameStream.str();

    Process* p = new Process(name, pid, config->minIns);
    p->creationTime = makeTimestamp();
    p->generateInstructions(config->minIns, config->maxIns);

    addProcess(p);
}

void ProcessScheduler::run() {
    ConfigManager* config = ConfigManager::getInstance();

    while (true) {
        // Batch process generation: every batchProcessFreq CPU cycles
        if (running) {
            int cycle = cpuCycle.fetch_add(1);
            if (config->batchProcessFreq > 0 && cycle % config->batchProcessFreq == 0) {
                generateBatchProcess();
            }
        }

        // Dispatch to cores
        if (config->scheduler == "\"fcfs\"" || config->scheduler == "fcfs")
            scheduleFCFS();
        else if (config->scheduler == "\"rr\"" || config->scheduler == "rr")
            scheduleRR();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// -----------------------------------------------------------------------
// FCFS: assign any queued process to a free core, run to completion
// -----------------------------------------------------------------------

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

// -----------------------------------------------------------------------
// RR: assign a process for exactly quantumCycles instructions, then preempt
// -----------------------------------------------------------------------

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
