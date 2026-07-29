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
    std::function<void(Process*)> onPreempt;

    // Cumulative CPU-tick accounting across ALL cores, used by vmstat.
    // One "tick" = one pass through this core's dispatch loop (~1ms). A core
    // counts as "active" for a tick if it had a process assigned at the top
    // of that pass, "idle" otherwise.
    static long long getTotalActiveTicks();
    static long long getTotalIdleTicks();

private:
    int id;
    Process* currentProcess;
    std::atomic<bool> available;
    int quantumSlice;   // instructions to execute this turn (-1 = unlimited)

    static std::atomic<long long> totalActiveTicks;
    static std::atomic<long long> totalIdleTicks;
};
