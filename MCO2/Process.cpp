#include "Process.h"
#include "ConsoleManager.h"
#include "ScreenManager.h"
#include <algorithm>
#include <random>
#include <ctime>
#include <sstream>
#include <iomanip>

static thread_local std::mt19937 rng(std::random_device{}());

static int randInt(int lo, int hi) {
    if (lo > hi) std::swap(lo, hi);
    return std::uniform_int_distribution<int>(lo, hi)(rng);
}
static uint16_t randU16() {
    return static_cast<uint16_t>(randInt(0, 65535));
}
static uint8_t randU8() {
    return static_cast<uint8_t>(randInt(1, 255));
}

static const std::vector<String> VAR_POOL = {
    "x", "y", "z", "a", "b", "c", "n", "m", "r", "s"
};

static String randomVar() {
    return VAR_POOL[randInt(0, static_cast<int>(VAR_POOL.size()) - 1)];
}

static AddInstruction::Operand randomOperand() {
    AddInstruction::Operand op;
    if (randInt(0, 1) == 0) {
        op.isLiteral = true;
        op.literal   = randU16();
    } else {
        op.isLiteral = false;
        op.name      = randomVar();
        op.literal   = 0;
    }
    return op;
}


Process::Process(const String& name, int id, int totalInstructions, size_t memSize)
    : name(name),
      id(id),
      currentLine(0),
      totalLines(totalInstructions),
      state(READY),
      assignedCore(-1),
      creationTime(""),
      sleepTicks(0),
      memSize(memSize)
{
}

Process::~Process() {
    for (auto ins : instructions)
        delete ins;
}


bool Process::isFinished() {
    ProcessState s = state;
    return s == FINISHED || s == SHUTDOWN_VIOLATION;
}

void Process::addLog(const String& timestamp, int coreId, const String& message) {
    std::lock_guard<std::mutex> lock(processMutex);
    logs.push_back({ timestamp, coreId, message });
}

// Symbol table access; reads/writes go through the allocator like any other memory access.

uint16_t Process::getVariable(const String& name) {
    auto it = varSlots.find(name);
    if (it == varSlots.end()) return 0; // never declared -> spec says uninitialized reads are 0

    IMemoryAllocator* alloc = ConsoleManager::getInstance()->getMemoryAllocator();
    uint16_t val = 0;
    if (!alloc->readUint16(memoryPtr, it->second * sizeof(uint16_t), val)) {
        // Should be unreachable; guard anyway rather than return garbage.
        triggerViolation(it->second * sizeof(uint16_t));
        return 0;
    }
    return val;
}

bool Process::declareVariable(const String& name, uint16_t value) {
    auto it = varSlots.find(name);
    if (it != varSlots.end()) {
        // MO1 semantics: DECLARE never overwrites an existing variable.
        return true;
    }
    if (varSlots.size() >= MAX_SYMBOL_TABLE_VARS) {
        // Symbol table full: new declarations are silently ignored per spec.
        return false;
    }
    size_t slot = varSlots.size();
    varSlots[name] = slot;

    IMemoryAllocator* alloc = ConsoleManager::getInstance()->getMemoryAllocator();
    if (!alloc->writeUint16(memoryPtr, slot * sizeof(uint16_t), value)) {
        triggerViolation(slot * sizeof(uint16_t));
        return false;
    }
    return true;
}

bool Process::setVariable(const String& name, uint16_t value) {
    auto it = varSlots.find(name);
    size_t slot;
    if (it != varSlots.end()) {
        slot = it->second;
    } else {
        if (varSlots.size() >= MAX_SYMBOL_TABLE_VARS) return false; // table full, ignored
        slot = varSlots.size();
        varSlots[name] = slot;
    }

    IMemoryAllocator* alloc = ConsoleManager::getInstance()->getMemoryAllocator();
    if (!alloc->writeUint16(memoryPtr, slot * sizeof(uint16_t), value)) {
        triggerViolation(slot * sizeof(uint16_t));
        return false;
    }
    return true;
}

// General-purpose addressable access for READ/WRITE instructions.

bool Process::readMemory(size_t address, uint16_t& outValue) {
    IMemoryAllocator* alloc = ConsoleManager::getInstance()->getMemoryAllocator();
    if (!alloc->readUint16(memoryPtr, address, outValue)) {
        triggerViolation(address);
        return false;
    }
    return true;
}

bool Process::writeMemory(size_t address, uint16_t value) {
    IMemoryAllocator* alloc = ConsoleManager::getInstance()->getMemoryAllocator();
    if (!alloc->writeUint16(memoryPtr, address, value)) {
        triggerViolation(address);
        return false;
    }
    return true;
}

void Process::triggerViolation(size_t address) {
    std::lock_guard<std::mutex> lock(processMutex);
    if (state == SHUTDOWN_VIOLATION) return; // already flagged, don't overwrite the original fault

    violation = true;
    String timeOnly = ScreenManager::getInstance()->getTimeOnly();

    std::ostringstream oss;
    oss << "Process " << name << " shut down due to memory access violation error that occurred at "
        << timeOnly << ". 0x"
        << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << address
        << " invalid.";
    violationMessage = oss.str();

    state = SHUTDOWN_VIOLATION;
}


IInstruction* Process::makeRandomInstruction(int currentDepth, int remainingBudget) {
    // No data segment on a 64-byte process, so skip READ/WRITE there.
    bool hasDataSegment = memSize > SYMBOL_TABLE_SIZE + 1; // need room for a 2-byte access
    bool allowFor        = currentDepth < 3;               // cap nesting depth

    // Pick uniformly from the kinds actually available right now.
    enum Kind { K_PRINT, K_DECLARE, K_ADD, K_SUBTRACT, K_SLEEP, K_READ, K_WRITE, K_FOR };
    Kind available[8];
    int  count = 0;
    available[count++] = K_PRINT;
    available[count++] = K_DECLARE;
    available[count++] = K_ADD;
    available[count++] = K_SUBTRACT;
    available[count++] = K_SLEEP;
    if (hasDataSegment) {
        available[count++] = K_READ;
        available[count++] = K_WRITE;
    }
    if (allowFor) {
        available[count++] = K_FOR;
    }

    Kind chosen = available[randInt(0, count - 1)];

    switch (chosen) {
        case K_PRINT: {
            if (randInt(0, 9) < 7) {
                return new PrintInstruction();
            } else {
                return new PrintInstruction(randomVar(), true, false);
            }
        }
        case K_DECLARE: {
            return new DeclareInstruction(randomVar(), randU16());
        }
        case K_ADD: {
            return new AddInstruction(randomVar(), randomOperand(), randomOperand());
        }
        case K_SUBTRACT: {
            return new SubtractInstruction(randomVar(), randomOperand(), randomOperand());
        }
        case K_SLEEP: {
            return new SleepInstruction(randU8());
        }
        case K_READ: {
            // READ <var> <random address in the data segment>
            size_t addr = SYMBOL_TABLE_SIZE + static_cast<size_t>(
                randInt(0, static_cast<int>(memSize - SYMBOL_TABLE_SIZE - 2)));
            return new ReadInstruction(randomVar(), addr);
        }
        case K_WRITE: {
            // WRITE <random address in the data segment> <literal or var>
            size_t addr = SYMBOL_TABLE_SIZE + static_cast<size_t>(
                randInt(0, static_cast<int>(memSize - SYMBOL_TABLE_SIZE - 2)));
            return new WriteInstruction(addr, randomOperand());
        }
        default: { // K_FOR
            int bodySize = std::min(randInt(2, 5), std::max(1, remainingBudget - 1));
            int repeats  = randInt(2, 5);

            std::vector<IInstruction*> body;
            body.reserve(bodySize);
            for (int i = 0; i < bodySize; ++i) {
                body.push_back(makeRandomInstruction(currentDepth + 1, bodySize - i));
            }
            return new ForInstruction(body, repeats);
        }
    }
}


void Process::generateInstructions(int minIns, int maxIns, int) {
    for (auto ins : instructions)
        delete ins;
    instructions.clear();

    totalLines = randInt(minIns, maxIns);
    instructions.reserve(totalLines);
    for (int i = 0; i < totalLines; ++i) {
        instructions.push_back(makeRandomInstruction(0, totalLines - i));
    }
}
