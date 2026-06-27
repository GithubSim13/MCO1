#include "Core.h"
#include "ConfigManager.h"
#include <chrono>

Core::Core(int id)
    : id(id), currentProcess(nullptr), available(true), quantumSlice(-1) {}

bool Core::isAvailable() {
    return available;
}

int Core::getId() {
    return id;
}

void Core::assignProcess(Process* process, int quantum) {
    quantumSlice    = quantum;
    currentProcess  = process;
    available       = false;
}

void Core::run() {
    ConfigManager* config = ConfigManager::getInstance();

    while (true) {
        if (currentProcess != nullptr) {
            currentProcess->state       = Process::RUNNING;
            currentProcess->assignedCore = id;

            int instructionsRun = 0;
            bool preempted = false;

            while (currentProcess->currentLine < currentProcess->totalLines) {

                // delay-per-exec busy wait
                for (int d = 0; d < config->delayPerExec; d++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // Execute current instruction
                if (currentProcess->currentLine < (int)currentProcess->instructions.size()) {
                    currentProcess->instructions[currentProcess->currentLine]->execute(currentProcess);
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

                // Quantum preemption check (RR)
                if (quantumSlice > 0 && instructionsRun >= quantumSlice) {
                    preempted = true;
                    break;
                }
            }

            if (preempted && currentProcess != nullptr) {
                // Hand the process back to the scheduler's ready queue
                currentProcess->state = Process::READY;
                if (onPreempt) onPreempt(currentProcess);
                currentProcess = nullptr;
                available = true;
            } else if (currentProcess != nullptr) {
                // Ran to completion
                currentProcess->state = Process::FINISHED;
                currentProcess = nullptr;
                available = true;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
