#include "ConsoleManager.h"
#include "ConfigManager.h"
#include "ProcessScheduler.h"
#include "ScreenManager.h"
#include <iostream>
#include <sstream>

ConsoleManager* ConsoleManager::instance = nullptr;
ConsoleManager* ConsoleManager::getInstance() { return instance; }
void ConsoleManager::initialize()             { instance = new ConsoleManager(); }
void ConsoleManager::destroy()                { delete instance; instance = nullptr; }

void ConsoleManager::printHeader() {
    std::cout << " _______  _______  _______  _______  _______  _______  __   __ \n";
    std::cout << "|       ||       ||       ||       ||       ||       ||  | |  |\n";
    std::cout << "|       ||  _____||   _   ||    _  ||    ___||  _____||  |_|  |\n";
    std::cout << "|       || |_____ |  | |  ||   |_| ||   |___ | |_____ |       |\n";
    std::cout << "|      _||_____  ||  |_|  ||    ___||    ___||_____  ||_     _|\n";
    std::cout << "|     |_  _____| ||       ||   |    |   |___  _____| |  |   |  \n";
    std::cout << "|_______||_______||_______||___|    |_______||_______|  |___|  \n";
    std::cout << "--------------------------------------\n";
    std::cout << "Welcome to CSOPESY Emulator!\n\n";
    std::cout << "Developers:\nSimbillo, Jose Miguel B.\n Arucan, Oliver Aldrin H, \n Pangan, Vince Vergel R., \n So, Jazlyn Daniella C.\n\n"; std::cout << "Last updated: 06-25-2026\n";
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
    if (input.empty()) return;

    std::istringstream iss(input);
    String cmd;
    iss >> cmd;

   
    if (!isInitialized && cmd != "initialize") {
        std::cout << "Please run 'initialize' first.\n";
        return;
    }

   
    if (cmd == "initialize") {
        if (isInitialized) { std::cout << "Already initialized.\n"; return; }
        ConfigManager::initialize();
        if (!ConfigManager::getInstance()->loadConfig("config.txt")) return;
        ProcessScheduler::initialize();
        ProcessScheduler::getInstance()->start();   
        ScreenManager::initialize();
        isInitialized = true;
        std::cout << "Initialized successfully.\n";
    }

    
    else if (cmd == "screen") {
        String flag, name;
        iss >> flag;

        if (flag == "-ls") {
            ScreenManager::getInstance()->listScreens();
        }
        else if (flag == "-s") {
            if (!(iss >> name)) { std::cout << "Usage: screen -s <name>\n"; return; }

         
            ProcessScheduler* sched = ProcessScheduler::getInstance();
            {
                std::lock_guard<std::mutex> lock(sched->allProcessesMutex);
                for (Process* p : sched->allProcesses)
                    if (p->name == name) {
                        std::cout << "Process '" << name << "' already exists.\n";
                        return;
                    }
            }

            ConfigManager* config = ConfigManager::getInstance();
            int pid = ProcessScheduler::getInstance()->nextPid.fetch_add(1);
            
            Process* p = new Process(name, pid, config->minIns);
            p->creationTime = ScreenManager::getInstance()->getTimestamp();
            p->generateInstructions(config->minIns, config->maxIns);

            sched->addProcess(p);
            std::cout << "Screen created for '" << name << "'.\n";
            ScreenManager::getInstance()->openScreen(p);
        }
        else if (flag == "-r") {
            if (!(iss >> name)) { std::cout << "Usage: screen -r <name>\n"; return; }
            ScreenManager::getInstance()->reattachScreen(name);
        }
        else {
            std::cout << "Usage: screen -s <name> | screen -r <name> | screen -ls\n";
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
    }

    
    else {
        std::cout << "Unknown command: '" << cmd << "'\n";
        std::cout << "Valid commands: initialize | screen -s/-r/-ls | "
                     "scheduler-start | scheduler-stop | report-util | exit\n";
    }
}
