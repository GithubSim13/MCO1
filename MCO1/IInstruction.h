#pragma once
#include <string>

typedef std::string String;

class IInstruction {
public:
    enum InstructionType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR
    };

    virtual void execute(class Process* process) = 0;
    virtual InstructionType getType() = 0;
    virtual ~IInstruction() {}
};