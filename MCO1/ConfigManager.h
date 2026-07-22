#pragma once
#include <string>

typedef std::string String;

class ConfigManager {
public:
    static ConfigManager* getInstance();
    static void initialize();
    static void destroy();
    int maxOverallMem;
    int memPerFrame;
    int memPerProc;

    bool loadConfig(const String& filename);

    int numCpu;
    String scheduler;
    int quantumCycles;
    int batchProcessFreq;
    int minIns;
    int maxIns;
    int delayPerExec;

private:
    ConfigManager() {}
    static ConfigManager* instance;
};