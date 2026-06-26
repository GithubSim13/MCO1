#include "Process.h"
#include <algorithm>
#include <random>
#include <ctime>


static thread_local std::mt19937 rng(std::random_device{}());

static int randInt(int lo, int hi) {               
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


Process::Process(const String& name, int id, int totalInstructions)
    : name(name),
      id(id),
      currentLine(0),
      totalLines(totalInstructions),
      state(READY),
      assignedCore(-1),
      sleepTicks(0)
{
    // creationTime is set externally by whoever creates the process
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


IInstruction* Process::makeRandomInstruction(int currentDepth, int remainingBudget) {
    
    int typeMax = (currentDepth >= 3) ? 4 : 5;  
    int type = randInt(0, typeMax);

    switch (type) {
        case 0: {   
           
            if (randInt(0, 9) < 7) {
                return new PrintInstruction();         
            } else {
                return new PrintInstruction(randomVar(), true, false);
            }
        }

        case 1: {   
            return new DeclareInstruction(randomVar(), randU16());
        }

        case 2: {   
            return new AddInstruction(randomVar(), randomOperand(), randomOperand());
        }

        case 3: {  
            return new SubtractInstruction(randomVar(), randomOperand(), randomOperand());
        }

        case 4: {   
            return new SleepInstruction(randU8());
        }

        default: {  
            
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


void Process::generateInstructions(int minIns, int maxIns, int ) {
   
    for (auto ins : instructions)
        delete ins;
    instructions.clear();

    
    totalLines = randInt(minIns, maxIns);
    instructions.reserve(totalLines);

    for (int i = 0; i < totalLines; ++i) {
        instructions.push_back(makeRandomInstruction(0, totalLines - i));
    }
}