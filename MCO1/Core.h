#pragma once
#include "IETThread.h"
#include "Process.h"
#include <atomic>
#include <functional>

class Core : public IETThread {
public:
    Core(int id);
    void run() override;

    // Assign a process to run for exactly `quantum` instructions (-1 = run to completion, i.e. FCFS)
    void assignProcess(Process* process, int quantum = -1);

    bool isAvailable();
    int getId();

    // Called by ProcessScheduler to give back a preempted process
    // Scheduler sets this callback so Core can hand the process back
    std::function<void(Process*)> onPreempt;

private:
    int id;
    Process* currentProcess;
    std::atomic<bool> available;
    int quantumSlice;   // instructions to execute this turn (-1 = unlimited)
};
