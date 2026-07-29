#include "Core.h"
#include "ConfigManager.h"
#include "ConsoleManager.h"
#include <chrono>

std::atomic<long long> Core::totalActiveTicks{0};
std::atomic<long long> Core::totalIdleTicks{0};

Core::Core(int id)
    : id(id), currentProcess(nullptr), available(true), quantumSlice(-1) {}

bool Core::isAvailable() {
    return available;
}

int Core::getId() {
    return id;
}

long long Core::getTotalActiveTicks() { return totalActiveTicks.load(); }
long long Core::getTotalIdleTicks()   { return totalIdleTicks.load(); }

void Core::assignProcess(Process* process, int quantum) {
    quantumSlice    = quantum;
    currentProcess  = process;
    available       = false;
}

void Core::run() {
    ConfigManager* config = ConfigManager::getInstance();

    while (true) {
        bool wasBusyThisTick = (currentProcess != nullptr);

        if (currentProcess != nullptr) {
            currentProcess->state       = Process::RUNNING;
            currentProcess->assignedCore = id;

            int instructionsRun = 0;
            bool preempted = false;
            bool violated  = false;

            while (currentProcess->currentLine < currentProcess->totalLines) {

                // delay-per-exec busy wait
                for (int d = 0; d < config->delayPerExec; d++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // Execute current instruction
                if (currentProcess->currentLine < (int)currentProcess->instructions.size()) {
                    currentProcess->instructions[currentProcess->currentLine]->execute(currentProcess);
                }

                // Memory access violation: Process::triggerViolation() already
                // marked the process SHUTDOWN_VIOLATION. Stop immediately -
                // no further instructions run, and it must NOT be requeued.
                if (currentProcess->state == Process::SHUTDOWN_VIOLATION) {
                    violated = true;
                    break;
                }

                // Sleep relinquishment: SLEEP instruction sets pendingSleep; hand process
                // back to the scheduler's sleeping queue and free this core immediately.
                if (currentProcess->pendingSleep) {
                    currentProcess->pendingSleep = false;
                    currentProcess->state        = Process::READY;
                    if (onPreempt) onPreempt(currentProcess);
                    currentProcess = nullptr;
                    available      = true;
                    break;
                }

                instructionsRun++;

                if (config->delayPerExec == 0)
                    std::this_thread::sleep_for(std::chrono::microseconds(1));

                // Quantum preemption check (RR)
                if (quantumSlice > 0 && instructionsRun >= quantumSlice) {
                    preempted = true;
                    break;
                }
            }

            if (violated) {
                // Release the process's memory back to the allocator. The
                // Process object itself is left in allProcesses (state =
                // SHUTDOWN_VIOLATION) so screen -r / screen -ls / process-smi
                // can still report on what happened to it.
                if (currentProcess->memoryPtr != nullptr) {
                    ConsoleManager::getInstance()->getMemoryAllocator()->deallocate(currentProcess->memoryPtr);
                    currentProcess->memoryPtr = nullptr;
                }
                currentProcess = nullptr;
                available = true;
            } else if (preempted && currentProcess != nullptr) {
                // Hand the process back to the scheduler's ready queue
                currentProcess->state = Process::READY;
                if (onPreempt) onPreempt(currentProcess);
                currentProcess = nullptr;
                available = true;
            } else if (currentProcess != nullptr) {
                // release memory
                currentProcess->state = Process::FINISHED;
                if (currentProcess->memoryPtr != nullptr) {
                    ConsoleManager::getInstance()->getMemoryAllocator()->deallocate(currentProcess->memoryPtr);
                    currentProcess->memoryPtr = nullptr;
                }
                currentProcess = nullptr;
                available = true;
            }
        }

        if (wasBusyThisTick) totalActiveTicks.fetch_add(1, std::memory_order_relaxed);
        else                 totalIdleTicks.fetch_add(1, std::memory_order_relaxed);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
