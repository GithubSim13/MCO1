#include "ConfigManager.h"
#include <fstream>
#include <iostream>

ConfigManager* ConfigManager::instance = nullptr;

ConfigManager* ConfigManager::getInstance() {
    return instance;
}

void ConfigManager::initialize() {
    instance = new ConfigManager();
}

void ConfigManager::destroy() {
    delete instance;
    instance = nullptr;
}

bool ConfigManager::loadConfig(const String& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: config.txt not found.\n";
        return false;
    } 

    String key;
    while (file >> key) {
        if (key == "num-cpu") file >> numCpu;
        else if (key == "scheduler") file >> scheduler;
        else if (key == "quantum-cycles") file >> quantumCycles;
        else if (key == "batch-process-freq") file >> batchProcessFreq;
        else if (key == "min-ins") file >> minIns;
        else if (key == "max-ins") file >> maxIns;
        else if (key == "delay-per-exec") file >> delayPerExec;
        else if (key == "max-overall-mem") file >> maxOverallMem;
        else if (key == "mem-per-frame") file >> memPerFrame;
        else if (key == "mem-per-proc") file >> memPerProc;
    }
    return true;
}