#pragma once
#include "IETThread.h"
#include "Process.h"
#include <atomic>

class Core : public IETThread {
public:
    Core(int id);
    void run() override;
    void assignProcess(Process* process);
    bool isAvailable();
    int getId();

private:
    int id;
    Process* currentProcess;
    std::atomic<bool> available;
};