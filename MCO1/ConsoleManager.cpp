#include "ConsoleManager.h"
#include "ConfigManager.h"
#include "ProcessScheduler.h"
#include "ScreenManager.h"
#include "PagingMemoryAllocator.h"
#include <iostream>
#include <sstream>

ConsoleManager* ConsoleManager::instance = nullptr;

ConsoleManager* ConsoleManager::getInstance() { return instance; }

void ConsoleManager::initialize() { instance = new ConsoleManager(); }

void ConsoleManager::destroy() { 
    delete instance; 
    instance = nullptr; 
}

ConsoleManager::~ConsoleManager() {
    if (memoryAllocator != nullptr) {
        delete memoryAllocator;
        memoryAllocator = nullptr;
    }
}

void ConsoleManager::printHeader() {
    std::cout << " ____  _____  ______  _______     ____  __ _    _ _            _______ ____  _____\n";
    std::cout << "/ __ \\|  __ \\|  ____|/ ____\\ \\   / /  \\/  | |  | | |        /\\|__   __/ __ \\|  __ \\\n";
    std::cout << "| |  | | |__) | |__  | (___  \\ \\_/ /| \\  / | |  | | |       /  \\  | | | |  | | |__) |\n";
    std::cout << "| |  | |  ___/|  __|  \\___ \\  \\   / | |\\/| | |  | | |      / /\\ \\ | | | |  | |  _  /\n";
    std::cout << "| |__| | |    | |____ ____) |  | |  | |  | | |__| | |____ / ____ \\| | | |__| | | \\ \\\n";
    std::cout << " \\____/|_|    |______|_____/   |_|  |_|  |_|\\____/|______/_/    \\_\\_|  \\____/|_|  \\_\\\n";
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
        ConfigManager* config = ConfigManager::getInstance();

        if (config->loadConfig("config.txt")) {
            // Instantiate Paging Memory Allocator using loaded config parameters
            memoryAllocator = new PagingMemoryAllocator(
                static_cast<size_t>(config->maxOverallMem),
                static_cast<size_t>(config->memPerFrame)
            );

            ProcessScheduler::initialize();
            ProcessScheduler::getInstance()->start();
            ScreenManager::initialize();
            
            isInitialized = true;
            std::cout << "System initialized successfully with Paging Memory Allocator.\n";
            std::cout << "Total Memory: " << config->maxOverallMem 
                      << " KB | Frame Size: " << config->memPerFrame << " KB\n";
        } else {
            std::cout << "Failed to load config.txt.\n";
        }
    }
    else if (cmd == "vmstat") {
        if (memoryAllocator != nullptr) {
            std::cout << "=== Memory & Paging Statistics ===\n";
            std::cout << memoryAllocator->visualizeMemory() << "\n";
            
            // Downcast to access specific Paging counters if needed
            auto* pagingAlloc = dynamic_cast<PagingMemoryAllocator*>(memoryAllocator);
            if (pagingAlloc != nullptr) {
                std::cout << "Num Paged In : " << pagingAlloc->getNumPagedIn() << "\n";
                std::cout << "Num Paged Out: " << pagingAlloc->getNumPagedOut() << "\n";
            }
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

            void* allocatedPtr = memoryAllocator->allocate(static_cast<size_t>(config->memPerProc));
            if (allocatedPtr == nullptr) {
                std::cout << "Error: Out of memory. Could not allocate memory for process " << name << "\n";
                return;
            }

            int pid = (int)sched->allProcesses.size() + 1;
            Process* p = new Process(name, pid, config->minIns);
            p->creationTime = ScreenManager::getInstance()->getTimestamp();
            p->generateInstructions(config->minIns, config->maxIns);
            p->memoryPtr = allocatedPtr;
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