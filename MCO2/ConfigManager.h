#pragma once
#include <string>

typedef std::string String;

// Loads and validates config.txt.
class ConfigManager {
public:
    static ConfigManager* getInstance();
    static void initialize();
    static void destroy();

    int numCpu = 1;
    String scheduler = "fcfs";
    int quantumCycles = 1;
    int batchProcessFreq = 1;
    int minIns = 1;
    int maxIns = 1;
    int delayPerExec = 0;

    int maxOverallMem = 0;
    int memPerFrame = 0;
    int minMemPerProc = 0;
    int maxMemPerProc = 0;

    bool loadConfig(const String& filename);

    // Validates all fields; call after loadConfig() and before booting the system.
    bool validateConfig(String& errorMessage) const;

private:
    ConfigManager() {}
    static ConfigManager* instance;
};
