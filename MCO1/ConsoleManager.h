#pragma once
#include <string>
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
};