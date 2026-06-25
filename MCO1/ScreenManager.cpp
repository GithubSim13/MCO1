#include "ScreenManager.h"
#include "ProcessScheduler.h"
#include "ConfigManager.h"  
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

ScreenManager* ScreenManager::instance = nullptr;

ScreenManager* ScreenManager::getInstance() { return instance; }

void ScreenManager::initialize() { instance = new ScreenManager(); }

void ScreenManager::destroy() { delete instance; instance = nullptr; }

String ScreenManager::getCurrentTimestamp() {
    time_t now = time(0);
    tm ltm;
    localtime_s(&ltm, &now);
    std::ostringstream oss;
    oss << std::setfill('0')
        << "(" << (ltm.tm_mon + 1) << "/" << ltm.tm_mday << "/" << (1900 + ltm.tm_year)
        << " " << std::setw(2) << ((ltm.tm_hour % 12) == 0 ? 12 : ltm.tm_hour % 12)
        << ":" << std::setw(2) << ltm.tm_min
        << ":" << std::setw(2) << ltm.tm_sec
        << (ltm.tm_hour >= 12 ? "PM" : "AM") << ")";
    return oss.str();
}

void ScreenManager::printProcessSMI(Process* process) {
    std::cout << "Process name: " << process->name << "\n";
    std::cout << "ID: " << process->id << "\n\n";
    std::cout << "Logs:\n";
    for (auto& log : process->logs) {
        std::cout << log.timestamp << " Core:" << log.coreId << " " << log.message << "\n";
    }
    std::cout << "\n";
    if (process->isFinished()) {
        std::cout << "Finished!\n";
    }
    else {
        std::cout << "Current instruction line: " << process->currentLine << "\n";
        std::cout << "Lines of code: " << process->totalLines << "\n";
    }
}

void ScreenManager::openScreen(const String& processName) {
    // for screen -s: create new process (handled in ConsoleManager)
    // for now just print placeholder
    std::cout << "Screen " << processName << " opened.\n";
}

void ScreenManager::reattachScreen(const String& processName) {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    Process* found = nullptr;
    for (Process* p : scheduler->allProcesses) {
        if (p->name == processName) { found = p; break; }
    }
    if (!found) {
        std::cout << "Process " << processName << " not found.\n";
        return;
    }

    system("cls");
    String cmd;
    while (true) {
        printProcessSMI(found);
        std::cout << "\nroot:\\> ";
        std::getline(std::cin, cmd);
        if (cmd == "exit") break;
        else if (cmd == "process-smi") { system("cls"); }
        else std::cout << "Unknown command.\n";
    }
    system("cls");
}

void ScreenManager::listScreens() {
    ProcessScheduler* scheduler = ProcessScheduler::getInstance();
    int used = 0, total = ConfigManager::getInstance()->numCpu;

    for (Process* p : scheduler->allProcesses)
        if (p->state == Process::RUNNING) used++;

    int util = total > 0 ? (used * 100 / total) : 0;
    std::cout << "CPU utilization: " << util << "%\n";
    std::cout << "Cores used: " << used << "\n";
    std::cout << "Cores available: " << (total - used) << "\n\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Running processes:\n";
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::RUNNING)
            std::cout << p->name << "\t" << p->creationTime
            << "\tCore: " << p->assignedCore
            << "\t" << p->currentLine << " / " << p->totalLines << "\n";
    }
    std::cout << "\nFinished processes:\n";
    for (Process* p : scheduler->allProcesses) {
        if (p->state == Process::FINISHED)
            std::cout << p->name << "\t" << p->creationTime
            << "\tFinished\t" << p->totalLines << " / " << p->totalLines << "\n";
    }
    std::cout << "--------------------------------------\n";
}