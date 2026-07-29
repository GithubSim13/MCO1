#pragma once
#include <string>

typedef std::string String;

// ConfigManager loads and validates config.txt.
//
// MO2 adds min-mem-per-proc / max-mem-per-proc (replacing the single MO1
// mem-per-proc value): scheduler-generated processes now roll a random
// per-process memory size M in [min-mem-per-proc, max-mem-per-proc], while
// manually created processes ("screen -s"/"screen -c") supply their own size
// on the command line. All per-process sizes must still obey the global
// [2^6, 2^16] power-of-two rule enforced in ConsoleManager.
class ConfigManager {
public:
    static ConfigManager* getInstance();
    static void initialize();
    static void destroy();

    // From MCO1 - Scheduler
    int numCpu = 1;
    String scheduler = "fcfs";
    int quantumCycles = 1;
    int batchProcessFreq = 1;
    int minIns = 1;
    int maxIns = 1;
    int delayPerExec = 0;

    // MO2 - Memory manager
    int maxOverallMem = 0;
    int memPerFrame = 0;
    int minMemPerProc = 0;
    int maxMemPerProc = 0;

    bool loadConfig(const String& filename);

    // Validates every field against the ranges specified in the MO2 spec.
    // Prints a specific diagnostic for the first problem found and returns
    // false. Call this right after loadConfig() and before wiring up the
    // scheduler/memory allocator - we never want to boot the emulator on a
    // config that would silently misbehave.
    bool validateConfig(String& errorMessage) const;

private:
    ConfigManager() {}
    static ConfigManager* instance;
};
