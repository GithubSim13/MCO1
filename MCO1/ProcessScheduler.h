#pragma once
#include "IETThread.h"
#include "Core.h"
#include "Process.h"
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <string>
#include <utility>

typedef std::string String;

class ProcessScheduler : public IETThread {
public:
    static ProcessScheduler* getInstance();
    static void initialize();
    static void destroy();

    void run() override;
    void addProcess(Process* process);
    void startScheduler();   // begins batch generation + dispatching
    void stopScheduler();    // stops batch generation (in-flight processes finish)

    // Called by Core when a process is preempted (RR only)
    void requeueProcess(Process* process);

    std::vector<Process*> allProcesses;
    std::mutex queueMutex;

private:
    ProcessScheduler() {}
    static ProcessScheduler* instance;

    std::queue<Process*>                    readyQueue;
    std::vector<std::pair<int, Process*>>   sleepingQueue;  // {wakeAtCycle, process}
    std::vector<Core*>                      cores;
    std::atomic<bool>                       running{ false };
    std::atomic<int>                        cpuCycle{ 0 };
    std::atomic<int>                        nextPid{ 1 };

    void scheduleFCFS();
    void scheduleRR();
    void generateBatchProcess();  // spawns one pXX process
};
