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


// PRINT has three shapes:
//   PRINT("literal text")                -> useDefault=false, isVar=false
//   PRINT(varName)                       -> isVar=true            (prints "Value from: <val>")
//   PRINT("prefix" + varName)            -> isConcat=true          (prints "<prefix><val>")
// plus the MO1 default no-arg PRINT() -> "Hello world from <process>!"
class PrintInstruction : public IInstruction {
public:
    explicit PrintInstruction(const String& msg = "", bool isVar = false, bool useDefault = true);
    // "prefix" + varName concatenation form
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


// READ(var, memory_address): retrieves the uint16 at `address` (within the
// process's own memory block) and stores it into `var` (a symbol-table
// variable, created if it doesn't exist yet, subject to the 32-variable cap).
// If `address` is outside the process's allocated memory, this triggers a
// memory access violation and shuts the process down.
class ReadInstruction : public IInstruction {
public:
    ReadInstruction(const String& destVar, size_t address);

    void execute(class Process* process) override;
    InstructionType getType() override { return READ; }

private:
    String destVar;
    size_t address;
};


// WRITE(memory_address, value): writes a uint16 (literal or the current value
// of a variable) to `address` within the process's own memory block. Same
// out-of-bounds -> violation behavior as READ.
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


// Parses a semicolon-separated instruction string as accepted by
// "screen -c <name> <mem> \"<instructions>\"" into concrete IInstruction
// objects. Grammar per instruction (case-sensitive keywords):
//   PRINT("text")  |  PRINT(varName)  |  PRINT("text" + varName)
//   DECLARE <var> <uint16literal>
//   ADD <dest> <op1> <op2>            (op := uint16literal | varName)
//   SUBTRACT <dest> <op1> <op2>
//   READ <var> <hexAddress>
//   WRITE <hexAddress> <op>
//   SLEEP <uint8ticks>
//
// Returns true and fills `outInstructions` (1-50 of them) on success.
// Returns false, leaves `outInstructions` empty (any partial objects are
// cleaned up internally), and fills `errorMessage` on any failure, including
// the instruction-count check ("invalid command" per the spec's wording).
bool ParseInstructionList(const String& text, size_t processMemSize,
                          std::vector<IInstruction*>& outInstructions,
                          String& errorMessage);
