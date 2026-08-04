#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

typedef std::string String;

class IInstruction {
public:
    enum InstructionType {
        PRINT,
        DECLARE,
        ADD,
        SUBTRACT,
        SLEEP,
        FOR,
        READ,
        WRITE
    };

    virtual void execute(class Process* process) = 0;
    virtual InstructionType getType() = 0;
    virtual ~IInstruction() {}
};


// PRINT(): default greeting | PRINT("text") | PRINT(var) | PRINT("prefix" + var)
class PrintInstruction : public IInstruction {
public:
    explicit PrintInstruction(const String& msg = "", bool isVar = false, bool useDefault = true);
    PrintInstruction(const String& prefixText, const String& varName, bool /*concatTag*/);

    void execute(class Process* process) override;
    InstructionType getType() override { return PRINT; }

private:
    String msg;
    bool   isVar;
    bool   useDefault;
    bool   isConcat = false;
    String prefixText;
    String varName;
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


// READ <var> <hexAddress>: loads a uint16 into var; out-of-bounds triggers a violation.
class ReadInstruction : public IInstruction {
public:
    ReadInstruction(const String& destVar, size_t address);

    void execute(class Process* process) override;
    InstructionType getType() override { return READ; }

private:
    String destVar;
    size_t address;
};


// WRITE <hexAddress> <value>: stores a uint16; out-of-bounds triggers a violation.
class WriteInstruction : public IInstruction {
public:
    using Operand = AddInstruction::Operand;

    WriteInstruction(size_t address, const Operand& valueOperand);

    void execute(class Process* process) override;
    InstructionType getType() override { return WRITE; }

private:
    size_t  address;
    Operand valueOperand;
};


// Parses a screen -c instruction string into IInstruction objects; false + errorMessage on bad syntax or an out-of-[1,50] count.
bool ParseInstructionList(const String& text, size_t processMemSize,
                          std::vector<IInstruction*>& outInstructions,
                          String& errorMessage);
