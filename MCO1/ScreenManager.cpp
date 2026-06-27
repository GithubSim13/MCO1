#include "ScreenManager.h"
#include "ProcessScheduler.h"
#include "ConfigManager.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager* ScreenManager::getInstance() { return instance; }
void ScreenManager::initialize()           { instance = new ScreenManager(); }
void ScreenManager::destroy()              { delete instance; instance = nullptr; }

// -----------------------------------------------------------------------
// Timestamp helper (also called by ConsoleManager for process creation)
// -----------------------------------------------------------------------

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
        << "(" << (ltm.tm_mon + 1) << "/" << ltm.tm_mday
        << "/" << (1900 + ltm.tm_year)
        << " " << std::setw(2) << h12
        << ":" << std::setw(2) << ltm.tm_min
        << ":" << std::setw(2) << ltm.tm_sec
        << (ltm.tm_hour >= 12 ? "PM" : "AM") << ")";
    return oss.str();
}

// -----------------------------------------------------------------------
// process-smi display
// -----------------------------------------------------------------------

void ScreenManager::printProcessSMI(Process* process) {
    std::cout << "Process name: " << process->name << "\n";
    std::cout << "ID: "           << process->id   << "\n\n";

    std::cout << "Logs:\n";
    {
        std::lock_guard<std::mutex> lock(process->processMutex);
        for (auto& log : process->logs) {
            std::cout << log.timestamp << " Core:" << log.coreId
                      << " " << log.message << "\n";
        }
    }
    std::cout << "\n";

    if (process->isFinished()) {
        std::cout << "Finished!\n";
    } else {
        std::cout << "Current instruction line: " << process->currentLine << "\n";
        std::cout << "Lines of code: "            << process->totalLines  << "\n";
    }
}

// -----------------------------------------------------------------------
// Shared interactive screen loop (used by both openScreen and reattachScreen)
// -----------------------------------------------------------------------

void ScreenManager::runScreenLoop(Process* process) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    String cmd;
    while (true) {
        printProcessSMI(process);
        std::cout << "\nroot:\\> ";
        std::getline(std::cin, cmd);

        if (cmd == "exit") {
            break;
        } else if (cmd == "process-smi") {
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
        } else {
            std::cout << "Commands: process-smi (refresh), exit (back to main)\n";
        }
    }

#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// -----------------------------------------------------------------------
// screen -s  : open a new process screen (process already added to scheduler)
// -----------------------------------------------------------------------

void ScreenManager::openScreen(Process* process) {
    std::cout << "Screen created for process '" << process->name << "'. Opening...\n";
    runScreenLoop(process);
}

// -----------------------------------------------------------------------
// screen -r  : reattach to an existing process by name
// -----------------------------------------------------------------------

void ScreenManager::reattachScreen(const String& processName) {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    Process* found = nullptr;

    for (Process* p : scheduler->allProcesses) {
        if (p->name == processName) { found = p; break; }
    }

    if (!found) {
        std::cout << "Process '" << processName << "' not found.\n";
        return;
    }

    runScreenLoop(found);
}

// -----------------------------------------------------------------------
// screen -ls : list all processes with CPU utilisation summary
// -----------------------------------------------------------------------

void ScreenManager::listScreens() {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    ConfigManager*    config    = ConfigManager::getInstance();

    int total = config->numCpu;
    int used  = 0;
    for (Process* p : scheduler->allProcesses)
        if (p->state == Process::RUNNING) used++;

    int util = total > 0 ? (used * 100 / total) : 0;

    std::cout << "CPU utilization: " << util << "%\n";
    std::cout << "Cores used: "      << used           << "\n";
    std::cout << "Cores available: " << (total - used) << "\n\n";
    std::cout << "--------------------------------------\n";

    std::cout << "Running processes:\n";
    bool anyRunning = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::RUNNING) {
            anyRunning = true;
            std::cout << std::left << std::setw(12) << p->name
                      << p->creationTime
                      << "   Core: " << p->assignedCore
                      << "   " << p->currentLine << " / " << p->totalLines << "\n";
        }
    }
    if (!anyRunning) std::cout << "(none)\n";

    std::cout << "\nFinished processes:\n";
    bool anyFinished = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::FINISHED) {
            anyFinished = true;
            std::cout << std::left << std::setw(12) << p->name
                      << p->creationTime
                      << "   Finished   "
                      << p->totalLines << " / " << p->totalLines << "\n";
        }
    }
    if (!anyFinished) std::cout << "(none)\n";

    std::cout << "--------------------------------------\n";
}

// -----------------------------------------------------------------------
// report-util : write csopesy-log.txt
// -----------------------------------------------------------------------

void ScreenManager::reportUtil() {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    ConfigManager*    config    = ConfigManager::getInstance();

    const String filename = "csopesy-log.txt";
    std::ofstream out(filename);
    if (!out.is_open()) {
        std::cout << "Error: could not open " << filename << " for writing.\n";
        return;
    }

    int total = config->numCpu;
    int used  = 0;
    for (Process* p : scheduler->allProcesses)
        if (p->state == Process::RUNNING) used++;

    int util = total > 0 ? (used * 100 / total) : 0;

    out << "CPU utilization: " << util << "%\n";
    out << "Cores used: "      << used           << "\n";
    out << "Cores available: " << (total - used) << "\n\n";
    out << "--------------------------------------\n";

    out << "Running processes:\n";
    bool anyRunning = false;
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::RUNNING) {
            anyRunning = true;
            out << std::left << std::setw(12) << p->name
                << p->creationTime
                << "   Core: " << p->assignedCore
                << "   " << p->currentLine << " / " << p->totalLines << "\n";
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
                << p->totalLines << " / " << p->totalLines << "\n";
        }
    }
    if (!anyFinished) out << "(none)\n";

    out << "--------------------------------------\n";
    out.close();

    // Also print to console so user sees it immediately
    listScreens();
    std::cout << "Report saved to " << filename << "\n";
}
