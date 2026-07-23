#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include "IInstruction.h"

typedef std::string String;

struct LogEntry {
    String timestamp;
    int    coreId;
    String message;
};

class Process {
public:
    enum ProcessState { READY, RUNNING, FINISHED };

    Process(const String& name, int id, int totalInstructions);
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

    
    std::unordered_map<String, uint16_t> variables;

    void* memoryPtr = nullptr;
    std::unordered_map<String, size_t> varSlots;

    std::mutex processMutex;

    bool isFinished();
    void addLog(const String& timestamp, int coreId, const String& message);
    void accessMemory(const String& varName, bool isWrite);

    
    void generateInstructions(int minIns, int maxIns, int forDepth = 0);

private:
    
    IInstruction* makeRandomInstruction(int currentDepth, int remainingBudget);
};