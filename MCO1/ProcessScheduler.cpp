#include "ProcessScheduler.h"
#include "ConfigManager.h"
#include <iostream>

ProcessScheduler* ProcessScheduler::instance = nullptr;

ProcessScheduler* ProcessScheduler::getInstance() {
    return instance;
}

void ProcessScheduler::initialize() {
    instance = new ProcessScheduler();
    ConfigManager* config = ConfigManager::getInstance();
    for (int i = 0; i < config->numCpu; i++) {
        Core* core = new Core(i);
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

void ProcessScheduler::run() {
    ConfigManager* config = ConfigManager::getInstance();
    while (true) {
        if (running) {
            if (config->scheduler == "\"fcfs\"" || config->scheduler == "fcfs")
                scheduleFCFS();
            else if (config->scheduler == "\"rr\"" || config->scheduler == "rr")
                scheduleRR();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ProcessScheduler::scheduleFCFS() {
    std::lock_guard<std::mutex> lock(queueMutex);
    for (Core* core : cores) {
        if (core->isAvailable() && !readyQueue.empty()) {
            Process* next = readyQueue.front();
            readyQueue.pop();
            core->assignProcess(next);
        }
    }
}

void ProcessScheduler::scheduleRR() {
    // TODO: implement RR with quantum cycles
    scheduleFCFS(); // placeholder
}