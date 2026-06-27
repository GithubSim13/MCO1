#include "ConsoleManager.h"
#include "ConfigManager.h"
#include "ProcessScheduler.h"
#include "ScreenManager.h"
#include <iostream>
#include <sstream>

ConsoleManager* ConsoleManager::instance = nullptr;

ConsoleManager* ConsoleManager::getInstance() { return instance; }
void ConsoleManager::initialize() { instance = new ConsoleManager(); }
void ConsoleManager::destroy() { delete instance; instance = nullptr; }

void ConsoleManager::printHeader() {
    std::cout << " ____  _____  ______  _______     ____  __ _    _ _            _______ ____  _____\n";
    std::cout << "/ __ \\|  __ \\|  ____|/ ____\\ \\   / /  \\/  | |  | | |        /\\|__   __/ __ \\|  __ \\\n";
    std::cout << "| |  | | |__) | |__  | (___  \\ \\_/ /| \\  / | |  | | |       /  \\  | | | |  | | |__) |\n";
    std::cout << "| |  | |  ___/|  __|  \\___ \\  \\   / | |\\/| | |  | | |      / /\\ \\ | | | |  | |  _  /\n";
    std::cout << "| |__| | |    | |____ ____) |  | |  | |  | | |__| | |____ / ____ \\| | | |__| | | \\ \\\n";
    std::cout << " \\____/|_|    |______|_____/   |_|  |_|  |_|\\____/|______/_/    \\_\\_|  \\____/|_|  \\_\\\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Welcome to OPESYmulator!\n\n";
    std::cout << "Developers:\nSimbillo, Jose Miguel B.\n\n";
    std::cout << "Last updated: 06-25-2026\n";
    std::cout << "--------------------------------------\n";
}

void ConsoleManager::run() {
    printHeader();
    String input;
    while (true) {
        std::cout << "root:\\> ";
        std::getline(std::cin, input);
        if (input == "exit") break;
        handleCommand(input);
    }
}

void ConsoleManager::handleCommand(const String& input) {
    std::istringstream iss(input);
    String cmd;
    iss >> cmd;

    if (!isInitialized && cmd != "initialize") {
        std::cout << "Please run 'initialize' first.\n";
        return;
    }

    if (cmd == "initialize") {
        ConfigManager::initialize();
        if (ConfigManager::getInstance()->loadConfig("config.txt")) {
            ProcessScheduler::initialize();
            ProcessScheduler::getInstance()->start();
            ScreenManager::initialize();
            isInitialized = true;
            std::cout << "Initialized.\n";
        }
    }
    else if (cmd == "screen") {
        String flag, name;
        iss >> flag;
        if (flag == "-ls") {
            ScreenManager::getInstance()->listScreens();
        }
        else if (flag == "-s") {
            iss >> name;
            ConfigManager* config = ConfigManager::getInstance();
            ProcessScheduler* sched = ProcessScheduler::getInstance();
            {
                std::lock_guard<std::mutex> lk(sched->queueMutex);
                for (Process* p : sched->allProcesses)
                    if (p->name == name) {
                        std::cout << "Process " << name << " already exists.\n";
                        return;
                    }
            }
            int pid = (int)sched->allProcesses.size() + 1;
            Process* p = new Process(name, pid, config->minIns);
            p->creationTime = ScreenManager::getInstance()->getTimestamp();
            p->generateInstructions(config->minIns, config->maxIns);
            sched->addProcess(p);
            ScreenManager::getInstance()->openScreen(p);
        }
        else if (flag == "-r") {
            iss >> name;
            ScreenManager::getInstance()->reattachScreen(name);
        }
    }
    else if (cmd == "scheduler-start") {
        ProcessScheduler::getInstance()->startScheduler();
        std::cout << "Scheduler started.\n";
    }
    else if (cmd == "scheduler-stop") {
        ProcessScheduler::getInstance()->stopScheduler();
        std::cout << "Scheduler stopped.\n";
    }
    else if (cmd == "report-util") {
        ScreenManager::getInstance()->reportUtil();
        std::cout << "Report generated at csopesy-log.txt\n";
    }
    else {
        std::cout << "Unknown command: " << cmd << "\n";
    }
}