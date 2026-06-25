#include "Process.h"

Process::Process(const String& name, int id, int totalInstructions)
    : name(name), id(id), currentLine(0),
    totalLines(totalInstructions), state(READY), assignedCore(-1) {
}

Process::~Process() {
    for (auto ins : instructions)
        delete ins;
}

bool Process::isFinished() {
    return state == FINISHED;
}

void Process::addLog(const String& timestamp, int coreId, const String& message) {
    std::lock_guard<std::mutex> lock(processMutex);
    logs.push_back({ timestamp, coreId, message });
}