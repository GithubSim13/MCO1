#pragma once
#include "IETThread.h"
#include "Core.h"
#include "Process.h"
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

class ProcessScheduler : public IETThread {
public:
    static ProcessScheduler* getInstance();
    static void initialize();
    static void destroy();

    void run() override;
    void addProcess(Process* process);
    void startScheduler();
    void stopScheduler();

    std::vector<Process*> allProcesses;
    std::mutex queueMutex;

private:
    ProcessScheduler() {}
    static ProcessScheduler* instance;

    std::queue<Process*> readyQueue;
    std::vector<Core*> cores;
    std::atomic<bool> running{ false };

    void scheduleFCFS();
    void scheduleRR();
};