#include "IInstruction.h"
#include "Process.h"
#include <algorithm>  
#include <stdexcept>


static uint16_t clamp16(int32_t val) {
    if (val < 0)      return 0;
    if (val > 65535)  return 65535;
    return static_cast<uint16_t>(val);
}


PrintInstruction::PrintInstruction(const String& msg, bool isVar, bool useDefault)
    : msg(msg), isVar(isVar), useDefault(useDefault) {}

void PrintInstruction::execute(Process* process) {
    String output;

    if (useDefault) {
        output = "Hello world from " + process->name + "!";
    } else if (isVar) {
        // Print value of a variable: "Value from: <val>"
        auto it = process->variables.find(msg);
        uint16_t val = (it != process->variables.end()) ? it->second : 0;
        output = "Value from: " + std::to_string(val);
    } else {
        output = msg;
    }

    
    process->addLog(process->creationTime, process->assignedCore, output);
    process->currentLine++;
}


DeclareInstruction::DeclareInstruction(const String& varName, uint16_t value)
    : varName(varName), value(value) {}

void DeclareInstruction::execute(Process* process) {
   
    if (process->variables.find(varName) == process->variables.end()) {
        process->variables[varName] = value;
    }
    process->currentLine++;
}


AddInstruction::AddInstruction(const String& dest, const Operand& op1, const Operand& op2)
    : dest(dest), op1(op1), op2(op2) {}

uint16_t AddInstruction::resolve(Process* process, const Operand& op) {
    if (op.isLiteral) return op.literal;
    auto it = process->variables.find(op.name);
    return (it != process->variables.end()) ? it->second : 0;
}

void AddInstruction::execute(Process* process) {
    int32_t a = resolve(process, op1);
    int32_t b = resolve(process, op2);
    process->variables[dest] = clamp16(a + b);
    process->currentLine++;
}


SubtractInstruction::SubtractInstruction(const String& dest, const Operand& op1, const Operand& op2)
    : dest(dest), op1(op1), op2(op2) {}

uint16_t SubtractInstruction::resolve(Process* process, const Operand& op) {
    if (op.isLiteral) return op.literal;
    auto it = process->variables.find(op.name);
    return (it != process->variables.end()) ? it->second : 0;
}

void SubtractInstruction::execute(Process* process) {
    int32_t a = resolve(process, op1);
    int32_t b = resolve(process, op2);
    process->variables[dest] = clamp16(a - b);  
    process->currentLine++;
}


SleepInstruction::SleepInstruction(uint8_t ticks)
    : ticks(ticks) {}

void SleepInstruction::execute(Process* process) {
   
    process->sleepTicks = ticks;
    process->currentLine++;
}


ForInstruction::ForInstruction(const std::vector<IInstruction*>& body, int repeats)
    : body(body), repeats(repeats) {}

ForInstruction::~ForInstruction() {
    for (auto ins : body)
        delete ins;
}

void ForInstruction::execute(Process* process) {
    for (int i = 0; i < repeats; ++i) {
        for (auto ins : body) {
            ins->execute(process);
        }
    }
    