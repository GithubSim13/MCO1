#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "IInstruction.h"

typedef std::string String;

struct LogEntry {
    String timestamp;
    int coreId;
    String message;
};

class Process {
public:
    enum ProcessState { READY, RUNNING, FINISHED };

    Process(const String& name, int id, int totalInstructions);
    ~Process();

    String name;
    int id;
    int currentLine;
    int totalLines;
    ProcessState state;
    int assignedCore;
    String creationTime;

    std::vector<IInstruction*> instructions;
    std::vector<LogEntry> logs;
    std::unordered_map<String, uint16_t> variables;
    std::mutex processMutex;

    bool isFinished();
    void addLog(const String& timestamp, int coreId, const String& message);
};