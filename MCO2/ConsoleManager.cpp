#include "ConsoleManager.h"
#include "ConfigManager.h"
#include "ProcessScheduler.h"
#include "ScreenManager.h"
#include "PagingMemoryAllocator.h"
#include "IInstruction.h"
#include "Core.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <mutex>
#include <random>

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
        std::cout << "\nroot:\\> ";
        std::getline(std::cin, input);
        if (input == "exit") break;
        handleCommand(input);
    }
}

void ConsoleManager::handleCommand(const String& input) {
    std::istringstream iss(input);
    String cmd;
    iss >> cmd;

    if (cmd.empty()) return;

    if (!isInitialized && cmd != "initialize" && cmd != "help") {
        std::cout << "Please run 'initialize' first.\n";
        return;
    }

    if (cmd == "initialize") {
        handleInitialize();
    }
    else if (cmd == "vmstat") {
        handleVmstat();
    }
    else if (cmd == "process-smi") {
        handleProcessSmi();
    }
    else if (cmd == "screen") {
        String rest;
        std::getline(iss, rest); // preserves spacing/quoting for screen -c's instruction string
        handleScreenCommand(rest);
    }
    // Accept all three spellings since the spec is inconsistent about the name.
    else if (cmd == "scheduler-start" || cmd == "scheduler-test" || cmd == "scheduler_start") {
        ProcessScheduler::getInstance()->startScheduler();
        std::cout << "Scheduler started.\n";
    }
    else if (cmd == "scheduler-stop") {
        ProcessScheduler::getInstance()->stopScheduler();
        std::cout << "Scheduler stopped.\n";
    }
    else if (cmd == "help") {
        handleHelp();
    }
    else if (cmd == "report-util") {
        ScreenManager::getInstance()->reportUtil();
        std::cout << "Report generated at csopesy-log.txt\n";
    }
    else {
        std::cout << "Unknown command: " << cmd << "\n";
    }
}

void ConsoleManager::handleInitialize() {
    if (isInitialized) {
        std::cout << "System is already initialized.\n";
        return;
    }

    ConfigManager::initialize();
    ConfigManager* config = ConfigManager::getInstance();

    if (!config->loadConfig("config.txt")) {
        std::cout << "Failed to load config.txt.\n";
        ConfigManager::destroy();
        return;
    }

    String err;
    if (!config->validateConfig(err)) {
        std::cout << "Invalid config.txt: " << err << "\n";
        ConfigManager::destroy();
        return;
    }

    memoryAllocator = new PagingMemoryAllocator(
        static_cast<size_t>(config->maxOverallMem),
        static_cast<size_t>(config->memPerFrame)
    );

    ProcessScheduler::initialize();
    ProcessScheduler::getInstance()->start();
    ScreenManager::initialize();

    isInitialized = true;
    std::cout << "System initialized successfully with a demand-paging memory allocator.\n";
    std::cout << "Total memory: " << config->maxOverallMem
               << " bytes | Frame size: " << config->memPerFrame << " bytes\n";
    std::cout << "Process memory range: [" << config->minMemPerProc << ", "
               << config->maxMemPerProc << "] bytes\n";
}

void ConsoleManager::handleHelp() {
    std::cout << "------------------------------------------------------------\n";
    std::cout << "AVAILABLE COMMANDS\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "initialize                       - load config.txt, boot the system\n";
    std::cout << "screen -s <name> [mem]           - create a process, open its screen\n";
    std::cout << "screen -c <name> [mem] \"<ins>\"   - create a process with custom instructions\n";
    std::cout << "screen -r <name>                 - reattach to a running process's screen\n";
    std::cout << "screen -ls                       - list running/finished/violated processes\n";
    std::cout << "scheduler-start / scheduler-test - begin generating dummy processes\n";
    std::cout << "scheduler-stop                   - stop generating dummy processes\n";
    std::cout << "report-util                      - write the screen -ls report to csopesy-log.txt\n";
    std::cout << "vmstat                           - memory + CPU tick statistics\n";
    std::cout << "process-smi                      - memory usage / per-process summary\n";
    std::cout << "help                             - show this list\n";
    std::cout << "exit                             - terminate the program\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Inside a process screen: process-smi (refresh), exit (back to main menu)\n";
    std::cout << "------------------------------------------------------------\n";
}

void ConsoleManager::handleVmstat() {
    if (!isInitialized) { std::cout << "Please run 'initialize' first.\n"; return; }

    size_t total = memoryAllocator->getTotalMemory();
    size_t used  = memoryAllocator->getUsedMemory();
    size_t free  = memoryAllocator->getFreeMemory();
    long long activeTicks = Core::getTotalActiveTicks();
    long long idleTicks   = Core::getTotalIdleTicks();
    long long totalTicks  = activeTicks + idleTicks;

    std::cout << "------------------------------------------------------------\n";
    std::cout << "VMSTAT\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Total memory:"     << total       << " bytes\n";
    std::cout << std::left << std::setw(20) << "Used memory:"      << used        << " bytes\n";
    std::cout << std::left << std::setw(20) << "Free memory:"      << free        << " bytes\n";
    std::cout << std::left << std::setw(20) << "Idle cpu ticks:"   << idleTicks   << "\n";
    std::cout << std::left << std::setw(20) << "Active cpu ticks:" << activeTicks << "\n";
    std::cout << std::left << std::setw(20) << "Total cpu ticks:"  << totalTicks  << "\n";
    std::cout << std::left << std::setw(20) << "Num paged in:"     << memoryAllocator->getNumPagedIn()  << "\n";
    std::cout << std::left << std::setw(20) << "Num paged out:"    << memoryAllocator->getNumPagedOut() << "\n";
    std::cout << "------------------------------------------------------------\n";
}

// nvidia-smi-style summary: overall memory usage + per-process footprint.
void ConsoleManager::handleProcessSmi() {
    if (!isInitialized) { std::cout << "Please run 'initialize' first.\n"; return; }

    size_t total = memoryAllocator->getTotalMemory();
    size_t used  = memoryAllocator->getUsedMemory();
    double util  = total > 0 ? (100.0 * static_cast<double>(used) / static_cast<double>(total)) : 0.0;

    ProcessScheduler* sched = ProcessScheduler::getInstance();

    std::cout << "------------------------------------------------------------\n";
    std::cout << "PROCESS-SMI\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "Memory Usage: " << used << "B / " << total << "B\n";
    std::cout << "Memory Util : " << std::fixed << std::setprecision(1) << util << "%\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << std::left << std::setw(14) << "Process" << "Memory\n";
    {
        std::lock_guard<std::mutex> lk(sched->queueMutex);
        bool any = false;
        for (Process* p : sched->allProcesses) {
            if (!p->isFinished()) {
                any = true;
                std::cout << std::left << std::setw(14) << p->name << p->memSize << "B\n";
            }
        }
        if (!any) std::cout << "(no running processes)\n";
    }
    std::cout << "------------------------------------------------------------\n";
}

void ConsoleManager::handleScreenCommand(const String& rest) {
    std::istringstream iss(rest);
    String flag;
    iss >> flag;

    if (flag == "-ls") {
        ScreenManager::getInstance()->listScreens();
    }
    else if (flag == "-s") {
        handleScreenCreate(iss);
    }
    else if (flag == "-c") {
        handleScreenCustom(iss, rest);
    }
    else if (flag == "-r") {
        String name;
        iss >> name;
        if (name.empty()) { std::cout << "Usage: screen -r <process_name>\n"; return; }
        ScreenManager::getInstance()->reattachScreen(name);
    }
    else {
        std::cout << "Usage: screen -s <name> <mem_size> | screen -c <name> <mem_size> \"<instructions>\" "
                     "| screen -r <name> | screen -ls\n";
    }
}

bool ConsoleManager::validateProcessMemorySize(const String& sizeStr, size_t& outSize) {
    if (sizeStr.empty()) return false;
    for (char c : sizeStr) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }

    unsigned long long val;
    try { val = std::stoull(sizeStr); } catch (...) { return false; }

    if (val < 64ULL || val > 65536ULL) return false;      // spec: [2^6, 2^16]
    if (val & (val - 1ULL)) return false;                  // must be a power of two

    outSize = static_cast<size_t>(val);
    return true;
}

// Default size for screen -s/-c when omitted (spec max, not min/max-mem-per-proc).
size_t ConsoleManager::rollConfiguredMemSize() {
    return 65536;
}

void ConsoleManager::handleScreenCreate(std::istringstream& iss) {
    String name;
    iss >> name;
    if (name.empty()) {
        std::cout << "Usage: screen -s <process_name> [process_memory_size]\n";
        return;
    }

    // Size is optional: derive a default if omitted, else validate strictly.
    String sizeStr;
    iss >> sizeStr;

    size_t memSize;
    if (sizeStr.empty()) {
        memSize = rollConfiguredMemSize();
    } else if (!validateProcessMemorySize(sizeStr, memSize)) {
        std::cout << "invalid memory allocation\n";
        return;
    }

    ProcessScheduler* sched = ProcessScheduler::getInstance();
    {
        std::lock_guard<std::mutex> lk(sched->queueMutex);
        for (Process* p : sched->allProcesses) {
            if (p->name == name) {
                std::cout << "Process " << name << " already exists.\n";
                return;
            }
        }
    }

    void* allocatedPtr = memoryAllocator->allocate(memSize);
    if (allocatedPtr == nullptr) {
        std::cout << "Error: Out of memory. Could not allocate memory for process " << name << "\n";
        return;
    }

    ConfigManager* config = ConfigManager::getInstance();
    int pid = static_cast<int>(sched->allProcesses.size()) + 1;
    Process* p = new Process(name, pid, config->minIns, memSize);
    p->creationTime = ScreenManager::getInstance()->getTimestamp();
    p->generateInstructions(config->minIns, config->maxIns);
    p->memoryPtr = allocatedPtr;
    sched->addProcess(p);
    ScreenManager::getInstance()->openScreen(p);
}

void ConsoleManager::handleScreenCustom(std::istringstream& iss, const String& /*fullLine*/) {
    String name;
    iss >> name;
    if (name.empty()) {
        std::cout << "Usage: screen -c <process_name> [process_memory_size] \"<instructions>\"\n";
        return;
    }

    String remainder;
    std::getline(iss, remainder);
    size_t firstNonSpace = remainder.find_first_not_of(" \t");
    if (firstNonSpace == String::npos) {
        std::cout << "Usage: screen -c <process_name> [process_memory_size] \"<instructions>\"\n";
        return;
    }
    remainder = remainder.substr(firstNonSpace);

    // If size is omitted, the remainder starts with a quote; else the first token is the size.
    size_t memSize;
    String instrPart;

    if (remainder[0] == '"') {
        memSize = rollConfiguredMemSize();
        instrPart = remainder;
    } else {
        size_t spacePos = remainder.find_first_of(" \t");
        if (spacePos == String::npos) {
            std::cout << "Usage: screen -c <process_name> [process_memory_size] \"<instructions>\"\n";
            return;
        }
        String sizeToken = remainder.substr(0, spacePos);
        if (!validateProcessMemorySize(sizeToken, memSize)) {
            std::cout << "invalid memory allocation\n";
            return;
        }
        size_t afterSize = remainder.find_first_not_of(" \t", spacePos);
        if (afterSize == String::npos) {
            std::cout << "Usage: screen -c <process_name> [process_memory_size] \"<instructions>\"\n";
            return;
        }
        instrPart = remainder.substr(afterSize);
    }

    size_t firstQuote = instrPart.find('"');
    size_t lastQuote  = instrPart.rfind('"');
    if (firstQuote == String::npos || lastQuote == String::npos || firstQuote == lastQuote) {
        std::cout << "invalid command (expected a quoted \"<instructions>\" string)\n";
        return;
    }
    String instrText = instrPart.substr(firstQuote + 1, lastQuote - firstQuote - 1);

    ProcessScheduler* sched = ProcessScheduler::getInstance();
    {
        std::lock_guard<std::mutex> lk(sched->queueMutex);
        for (Process* p : sched->allProcesses) {
            if (p->name == name) {
                std::cout << "Process " << name << " already exists.\n";
                return;
            }
        }
    }

    std::vector<IInstruction*> parsed;
    String err;
    if (!ParseInstructionList(instrText, memSize, parsed, err)) {
        std::cout << err << "\n";
        return;
    }

    void* allocatedPtr = memoryAllocator->allocate(memSize);
    if (allocatedPtr == nullptr) {
        std::cout << "Error: Out of memory. Could not allocate memory for process " << name << "\n";
        for (auto* ins : parsed) delete ins;
        return;
    }

    int pid = static_cast<int>(sched->allProcesses.size()) + 1;
    Process* p = new Process(name, pid, static_cast<int>(parsed.size()), memSize);
    p->creationTime = ScreenManager::getInstance()->getTimestamp();
    p->instructions = parsed;
    p->totalLines   = static_cast<int>(parsed.size());
    p->memoryPtr    = allocatedPtr;
    sched->addProcess(p);
    ScreenManager::getInstance()->openScreen(p);
}
