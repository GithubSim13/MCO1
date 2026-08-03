#pragma once
#include <string>
#include <sstream>
#include "IMemoryAllocator.h"

typedef std::string String;

class ConsoleManager {
public:
    static ConsoleManager* getInstance();
    static void initialize();
    static void destroy();

    void run();

    // Helper getter for process/scheduler access
    IMemoryAllocator* getMemoryAllocator() const { return memoryAllocator; }

private:
    ConsoleManager() = default;
    ~ConsoleManager();

    static ConsoleManager* instance;

    bool isInitialized = false;
    IMemoryAllocator* memoryAllocator = nullptr;

    void printHeader();
    void handleCommand(const String& command);

    void handleInitialize();
    void handleScreenCommand(const String& rest);
    void handleScreenCreate(std::istringstream& iss);   // screen -s <name> <mem>
    void handleScreenCustom(std::istringstream& iss, const String& fullLine); // screen -c <name> <mem> "<instr>"
    void handleVmstat();
    void handleProcessSmi();

    // Shared validation for process memory sizes: must be a power of 2 in
    // [64, 65536] per the spec ("invalid memory allocation" otherwise).
    bool validateProcessMemorySize(const String& sizeStr, size_t& outSize);

    // Used by screen -s / screen -c when no explicit memory size is given:
    // rolls a size the same way a scheduler-generated process gets one
    // (uniform random in [min-mem-per-proc, max-mem-per-proc]).
    size_t rollConfiguredMemSize();
};
