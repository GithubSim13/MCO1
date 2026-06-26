#pragma once
#include <string>
#include <vector>
#include <cstdint>

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


class PrintInstruction : public IInstruction {
public:
   
    explicit PrintInstruction(const String& msg = "", bool isVar = false, bool useDefault = true);

    void execute(class Process* process) override;
    InstructionType getType() override { return PRINT; }

private:
    String  msg;
    bool    isVar;       
    bool    useDefault; 
};



class DeclareInstruction : public IInstruction {
public:
    DeclareInstruction(const String& varName, uint16_t value);

    void execute(class Process* process) override;
    InstructionType getType() override { return DECLARE; }

private:
    String   varName;
    uint16_t value;
};

class AddInstruction : public IInstruction {
public:
    
    struct Operand {
        String   name;     
        uint16_t literal;   
        bool     isLiteral;
    };

    AddInstruction(const String& dest,
                   const Operand& op1,
                   const Operand& op2);

    void execute(class Process* process) override;
    InstructionType getType() override { return ADD; }

private:
    String  dest;
    Operand op1, op2;

    uint16_t resolve(class Process* process, const Operand& op);
};


class SubtractInstruction : public IInstruction {
public:
    using Operand = AddInstruction::Operand;

    SubtractInstruction(const String& dest,
                        const Operand& op1,
                        const Operand& op2);

    void execute(class Process* process) override;
    InstructionType getType() override { return SUBTRACT; }

private:
    String  dest;
    Operand op1, op2;

    uint16_t resolve(class Process* process, const Operand& op);
};


class SleepInstruction : public IInstruction {
public:
    explicit SleepInstruction(uint8_t ticks);

    void execute(class Process* process) override;
    InstructionType getType() override { return SLEEP; }

private:
    uint8_t ticks;
};


class ForInstruction : public IInstruction {
public:
    ForInstruction(const std::vector<IInstruction*>& body, int repeats);
    ~ForInstruction();

    void execute(class Process* process) override;
    InstructionType getType() override { return FOR; }

private:
    std::vector<IInstruction*> body;
    int repeats;
};