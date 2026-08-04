#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <cstddef>
#include "IInstruction.h"

typedef std::string String;

struct LogEntry {
    String timestamp;
    int    coreId;
    String message;
};

class Process {
public:
    // SHUTDOWN_VIOLATION = terminated early from an out-of-bounds memory access.
    enum ProcessState { READY, RUNNING, FINISHED, SHUTDOWN_VIOLATION };

    // Symbol table: first 64 bytes of memory, max 32 uint16 variables.
    static const size_t SYMBOL_TABLE_SIZE     = 64;
    static const size_t MAX_SYMBOL_TABLE_VARS = SYMBOL_TABLE_SIZE / sizeof(uint16_t);

    Process(const String& name, int id, int totalInstructions, size_t memSize = 0);
    ~Process();

    String name;
    int    id;

    int          currentLine;
    int          totalLines;
    std::atomic<ProcessState> state;
    int          assignedCore;
    String       creationTime;

    int  sleepTicks;
    bool pendingSleep = false;

    std::vector<IInstruction*> instructions;
    std::vector<LogEntry>      logs;

    void* memoryPtr = nullptr;
    size_t memSize  = 0; // total bytes allocated to this process (set at creation)

    std::mutex processMutex;

    bool isFinished();                     // true for FINISHED or SHUTDOWN_VIOLATION
    bool isViolationShutdown() const { return state == SHUTDOWN_VIOLATION; }
    const String& getViolationMessage() const { return violationMessage; }

    void addLog(const String& timestamp, int coreId, const String& message);

    // Symbol table access (routed through the memory allocator).
    uint16_t getVariable(const String& name);                  // 0 if never declared
    bool declareVariable(const String& name, uint16_t value);  // DECLARE semantics: no-op if exists
    bool setVariable(const String& name, uint16_t value);      // READ/ADD/SUB dest semantics: create-or-update

    // Addressable access for READ/WRITE; false + violation if out of bounds.
    bool readMemory(size_t address, uint16_t& outValue);
    bool writeMemory(size_t address, uint16_t value);

    void triggerViolation(size_t address);

    void generateInstructions(int minIns, int maxIns, int forDepth = 0);

private:
    std::unordered_map<String, size_t> varSlots; // variable name -> slot index [0, MAX_SYMBOL_TABLE_VARS)

    bool violation = false;
    String violationMessage;

    IInstruction* makeRandomInstruction(int currentDepth, int remainingBudget);
};
