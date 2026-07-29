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
    // FINISHED   = ran to completion normally.
    // SHUTDOWN_VIOLATION = terminated early because of an out-of-bounds
    //                       memory access (READ/WRITE to an address outside
    //                       this process's own memory block).
    enum ProcessState { READY, RUNNING, FINISHED, SHUTDOWN_VIOLATION };

    // Every process's symbol table (where DECLARE'd / READ-destination /
    // ADD-SUBTRACT-dest uint16 variables live) is a fixed 64-byte region at
    // the very start of the process's memory block, holding at most 32
    // variables (64 / sizeof(uint16_t)). This is a hard cap per the spec:
    // once full, further NEW variable declarations are silently ignored.
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

    // --- Symbol table variable access (goes through the memory allocator, so
    // page faults / eviction / backing-store I/O happen transparently) -------
    uint16_t getVariable(const String& name);                  // 0 if never declared
    bool declareVariable(const String& name, uint16_t value);  // DECLARE semantics: no-op if exists
    bool setVariable(const String& name, uint16_t value);      // READ/ADD/SUB dest semantics: create-or-update

    // --- General-purpose addressable access for READ/WRITE instructions ----
    // Returns false (and shuts the process down via triggerViolation) if
    // `address` falls outside this process's own memory block.
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
