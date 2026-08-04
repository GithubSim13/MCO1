#include "ScreenManager.h"
#include "ProcessScheduler.h"
#include "ConfigManager.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <vector>

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager* ScreenManager::getInstance() { return instance; }
void ScreenManager::initialize() { instance = new ScreenManager(); }
void ScreenManager::destroy() { delete instance; instance = nullptr; }

String ScreenManager::getTimestamp() {
    time_t now = time(0);
    tm ltm;
#ifdef _WIN32
    localtime_s(&ltm, &now);
#else
    localtime_r(&now, &ltm);
#endif
    std::ostringstream oss;
    int h12 = (ltm.tm_hour % 12 == 0) ? 12 : ltm.tm_hour % 12;
    oss << std::setfill('0')
        << "(" << std::setw(2) << (ltm.tm_mon + 1) << "/" << std::setw(2) << ltm.tm_mday
        << "/" << (1900 + ltm.tm_year)
        << " " << std::setw(2) << h12
        << ":" << std::setw(2) << ltm.tm_min
        << ":" << std::setw(2) << ltm.tm_sec
        << (ltm.tm_hour >= 12 ? "PM" : "AM") << ")";
    return oss.str();
}

String ScreenManager::getTimeOnly() {
    time_t now = time(0);
    tm ltm;
#ifdef _WIN32
    localtime_s(&ltm, &now);
#else
    localtime_r(&now, &ltm);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << ltm.tm_hour << ":"
        << std::setw(2) << ltm.tm_min  << ":"
        << std::setw(2) << ltm.tm_sec;
    return oss.str();
}

void ScreenManager::printProcessSMI(Process* process) {
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Process name: " << process->name << "\n";
    std::cout << "ID: " << process->id << "\n";
    std::cout << "Memory: " << process->memSize << " bytes\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Logs:\n";
    {
        std::lock_guard<std::mutex> lock(process->processMutex);
        for (auto& log : process->logs) {
            std::cout << log.timestamp << " Core:" << log.coreId
                << " \"" << log.message << "\"\n";
        }
    }
    std::cout << "\n";
    if (process->isViolationShutdown()) {
        std::cout << process->getViolationMessage() << "\n";
    } else if (process->isFinished()) {
        std::cout << "Finished!\n";
    } else {
        std::cout << "Current instruction line: " << process->currentLine << "\n";
        std::cout << "Lines of code: " << process->totalLines << "\n";
    }
}

void ScreenManager::runScreenLoop(Process* process) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
    // Root-level commands typed reflexively while still inside a process screen.
    static const std::vector<String> rootOnlyCommands = {
        "initialize", "vmstat", "screen", "scheduler-start", "scheduler-test",
        "scheduler_start", "scheduler-stop", "report-util"
    };

    String cmd;
    while (true) {
        printProcessSMI(process);
        std::cout << "\n" << process->name << ":\\> ";
        std::getline(std::cin, cmd);

        std::istringstream firstTokenStream(cmd);
        String firstToken;
        firstTokenStream >> firstToken;

        if (cmd == "exit") {
            break;
        }
        else if (cmd == "process-smi") {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
        }
        else if (std::find(rootOnlyCommands.begin(), rootOnlyCommands.end(), firstToken) != rootOnlyCommands.end()) {
            std::cout << "'" << firstToken << "' is a root-level command. Type 'exit' to return to the main menu first.\n";
        }
        else {
            std::cout << "Commands: process-smi (refresh), exit (back to main)\n";
        }
    }
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void ScreenManager::openScreen(Process* process) {
    runScreenLoop(process);
}

void ScreenManager::reattachScreen(const String& processName) {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    Process* found = nullptr;
    {
        std::lock_guard<std::mutex> lock(scheduler->queueMutex);
        for (Process* p : scheduler->allProcesses) {
            if (p->name == processName) { found = p; break; }
        }
    }

    if (!found) {
        std::cout << "Process " << processName << " not found.\n";
        return;
    }

    // A violation shutdown gets a specific diagnostic instead of "not found."
    if (found->isViolationShutdown()) {
        std::cout << found->getViolationMessage() << "\n";
        return;
    }

    // A normally-finished process can no longer be reattached to.
    if (found->isFinished()) {
        std::cout << "Process " << processName << " not found.\n";
        return;
    }

    runScreenLoop(found);
}

// Shared by listScreens() and reportUtil() so they can't drift apart.
static void writeUtilizationReport(std::ostream& out) {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    ConfigManager* config = ConfigManager::getInstance();

    std::lock_guard<std::mutex> lock(scheduler->queueMutex);

    int total = config->numCpu;
    int used = 0;
    for (Process* p : scheduler->allProcesses)
        if (p->state == Process::RUNNING) used++;

    int util = total > 0 ? (used * 100 / total) : 0;

    out << "------------------------------------------------------------\n";
    out << "SCREEN -LS\n";
    out << "------------------------------------------------------------\n";
    out << "CPU utilization: " << util << "%\n";
    out << "Cores used: " << used << "\n";
    out << "Cores available: " << (total - used) << "\n";
    out << "------------------------------------------------------------\n";

    out << "Running processes:\n";
    bool anyRunning = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::RUNNING || p->state == Process::READY) {
            anyRunning = true;
            String coreStr = (p->state == Process::RUNNING) ? std::to_string(p->assignedCore) : "-";
            out << std::left << std::setw(12) << p->name
                << p->creationTime
                << "   Core: " << coreStr
                << "   " << p->currentLine << " / " << p->totalLines
                << "   Mem: " << p->memSize << "B\n";
        }
    }
    if (!anyRunning) out << "(none)\n";

    out << "\nFinished processes:\n";
    bool anyFinished = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::FINISHED) {
            anyFinished = true;
            out << std::left << std::setw(12) << p->name
                << p->creationTime
                << "   Finished   "
                << p->totalLines << " / " << p->totalLines
                << "   Mem: " << p->memSize << "B\n";
        }
    }
    if (!anyFinished) out << "(none)\n";

    out << "\nShut down (memory access violation):\n";
    bool anyViolated = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->isViolationShutdown()) {
            anyViolated = true;
            out << std::left << std::setw(12) << p->name << p->getViolationMessage() << "\n";
        }
    }
    if (!anyViolated) out << "(none)\n";

    out << "------------------------------------------------------------\n";
}

void ScreenManager::listScreens() {
    writeUtilizationReport(std::cout);
}

void ScreenManager::reportUtil() {
    const String filename = "csopesy-log.txt";
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cout << "Error: could not open " << filename << " for writing.\n";
        return;
    }
    writeUtilizationReport(out);
    out.close();
}
