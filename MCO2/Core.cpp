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
    int idleStreak = 0;

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

                // Violation already marked the process SHUTDOWN_VIOLATION; stop, don't requeue.
                if (currentProcess->state == Process::SHUTDOWN_VIOLATION) {
                    violated = true;
                    break;
                }

                // SLEEP hit: hand back to the scheduler's sleeping queue, free this core.
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
                // Free memory; keep the Process object so reporting still works.
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

        // Only sleep when there's genuinely nothing to do. Sleeping
        // unconditionally here (even right after finishing a whole quantum
        // in microseconds) throttles dispatch throughput to ~1000
        // reassignments/sec/core regardless of how fast instructions
        // actually execute - a short, idle-only poll avoids that bottleneck
        // while still not busy-spinning a real host CPU core.
        if (currentProcess == nullptr) {
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
