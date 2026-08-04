#include "ConfigManager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>

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

// Strips surrounding quotes from a config value, e.g. "\"rr\"" -> "rr".
static String stripQuotes(const String& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

bool ConfigManager::loadConfig(const String& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "Error: " << filename << " not found.\n";
        return false;
    }

    String key;
    while (file >> key) {
        if (key == "num-cpu") file >> numCpu;
        else if (key == "scheduler") { String s; file >> s; scheduler = stripQuotes(s); }
        else if (key == "quantum-cycles") file >> quantumCycles;
        else if (key == "batch-process-freq") file >> batchProcessFreq;
        else if (key == "min-ins") file >> minIns;
        else if (key == "max-ins") file >> maxIns;
        else if (key == "delay-per-exec") file >> delayPerExec;
        else if (key == "max-overall-mem") file >> maxOverallMem;
        else if (key == "mem-per-frame") file >> memPerFrame;
        else if (key == "min-mem-per-proc") file >> minMemPerProc;
        else if (key == "max-mem-per-proc") file >> maxMemPerProc;
        else if (key == "mem-per-proc") {
            // Legacy MO1 key: treat as fixed-size (min == max == value).
            int legacy; file >> legacy;
            if (minMemPerProc == 0) minMemPerProc = legacy;
            if (maxMemPerProc == 0) maxMemPerProc = legacy;
        }
        else {
            // Unknown key: consume its value and warn instead of desyncing the stream.
            String ignored; file >> ignored;
            std::cout << "Warning: unrecognized config key '" << key << "' ignored.\n";
        }
    }
    return true;
}

static bool isPowerOfTwo(long long v) {
    return v > 0 && (v & (v - 1)) == 0;
}

bool ConfigManager::validateConfig(String& errorMessage) const {
    std::ostringstream err;

    if (numCpu < 1 || numCpu > 128) {
        err << "num-cpu must be in [1, 128], got " << numCpu;
        errorMessage = err.str(); return false;
    }
    if (scheduler != "fcfs" && scheduler != "rr") {
        err << "scheduler must be \"fcfs\" or \"rr\", got \"" << scheduler << "\"";
        errorMessage = err.str(); return false;
    }
    if (scheduler == "rr" && quantumCycles < 1) {
        err << "quantum-cycles must be >= 1 when scheduler is \"rr\", got " << quantumCycles;
        errorMessage = err.str(); return false;
    }
    if (quantumCycles < 0) {
        err << "quantum-cycles must be >= 0, got " << quantumCycles;
        errorMessage = err.str(); return false;
    }
    if (batchProcessFreq < 1) {
        err << "batch-process-freq must be >= 1, got " << batchProcessFreq;
        errorMessage = err.str(); return false;
    }
    if (minIns < 1 || maxIns < 1 || minIns > maxIns) {
        err << "min-ins/max-ins must satisfy 1 <= min-ins <= max-ins, got ["
            << minIns << ", " << maxIns << "]";
        errorMessage = err.str(); return false;
    }
    if (delayPerExec < 0) {
        err << "delay-per-exec must be >= 0, got " << delayPerExec;
        errorMessage = err.str(); return false;
    }
    if (memPerFrame <= 0) {
        err << "mem-per-frame must be > 0, got " << memPerFrame;
        errorMessage = err.str(); return false;
    }
    if (maxOverallMem <= 0) {
        err << "max-overall-mem must be > 0, got " << maxOverallMem;
        errorMessage = err.str(); return false;
    }
    if (maxOverallMem % memPerFrame != 0) {
        err << "max-overall-mem (" << maxOverallMem
            << ") must be an exact multiple of mem-per-frame (" << memPerFrame << ")";
        errorMessage = err.str(); return false;
    }
    if (minMemPerProc < 64 || minMemPerProc > 65536 || !isPowerOfTwo(minMemPerProc)) {
        err << "min-mem-per-proc must be a power of 2 in [64, 65536], got " << minMemPerProc;
        errorMessage = err.str(); return false;
    }
    if (maxMemPerProc < 64 || maxMemPerProc > 65536 || !isPowerOfTwo(maxMemPerProc)) {
        err << "max-mem-per-proc must be a power of 2 in [64, 65536], got " << maxMemPerProc;
        errorMessage = err.str(); return false;
    }
    if (minMemPerProc > maxMemPerProc) {
        err << "min-mem-per-proc (" << minMemPerProc
            << ") cannot exceed max-mem-per-proc (" << maxMemPerProc << ")";
        errorMessage = err.str(); return false;
    }

    return true;
}
