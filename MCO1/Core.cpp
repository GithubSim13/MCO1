#include "Core.h"
#include "ConfigManager.h"
#include <iostream>
#include <chrono>
#include <ctime>

Core::Core(int id) : id(id), currentProcess(nullptr), available(true) {}

bool Core::isAvailable() {
    return available;
}

int Core::getId() {
    return id;
}

void Core::assignProcess(Process* process) {
    currentProcess = process;
    available = false;
}

void Core::run() {
    while (true) {
        if (currentProcess != nullptr) {
            ConfigManager* config = ConfigManager::getInstance();
            currentProcess->state = Process::RUNNING;
            currentProcess->assignedCore = id;

            while (currentProcess->currentLine < currentProcess->totalLines) {
                // delay-per-exec busy wait
                for (int d = 0; d < config->delayPerExec; d++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // execute instruction
                if (currentProcess->currentLine < (int)currentProcess->instructions.size()) {
                    currentProcess->instructions[currentProcess->currentLine]->execute(currentProcess);
                }
                currentProcess->currentLine++;
            }

            currentProcess->state = Process::FINISHED;
            currentProcess = nullptr;
            available = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}