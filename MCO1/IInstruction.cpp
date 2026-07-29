#include "IInstruction.h"
#include "Process.h"
#include "ScreenManager.h"
#include <algorithm>
#include <cctype>
#include <sstream>

static uint16_t clamp16(int32_t val) {
    if (val < 0)     return 0;
    if (val > 65535) return 65535;
    return static_cast<uint16_t>(val);
}

// ---------------------------------------------------------------------------
// PRINT
// ---------------------------------------------------------------------------

PrintInstruction::PrintInstruction(const String& msg, bool isVar, bool useDefault)
    : msg(msg), isVar(isVar), useDefault(useDefault) {
}

PrintInstruction::PrintInstruction(const String& prefixText, const String& varName, bool)
    : msg(""), isVar(false), useDefault(false), isConcat(true), prefixText(prefixText), varName(varName) {
}

void PrintInstruction::execute(Process* process) {
    String output;
    if (isConcat) {
        uint16_t val = process->getVariable(varName);
        if (process->isViolationShutdown()) return;
        output = prefixText + std::to_string(val);
    } else if (useDefault) {
        output = "Hello world from " + process->name + "!";
    } else if (isVar) {
        uint16_t val = process->getVariable(msg);
        if (process->isViolationShutdown()) return;
        output = "Value from: " + std::to_string(val);
    } else {
        output = msg;
    }
    process->addLog(ScreenManager::getInstance()->getTimestamp(), process->assignedCore, output);
    process->currentLine++;
}

// ---------------------------------------------------------------------------
// DECLARE - create-only: a no-op if the variable already exists (MO1 semantics)
// ---------------------------------------------------------------------------

DeclareInstruction::DeclareInstruction(const String& varName, uint16_t value)
    : varName(varName), value(value) {
}

void DeclareInstruction::execute(Process* process) {
    process->declareVariable(varName, value);
    if (process->isViolationShutdown()) return; // symbol table addr is always valid, but guard anyway
    process->currentLine++;
}

// ---------------------------------------------------------------------------
// ADD / SUBTRACT - dest is create-or-update (an implicit declare on first use)
// ---------------------------------------------------------------------------

AddInstruction::AddInstruction(const String& dest, const Operand& op1, const Operand& op2)
    : dest(dest), op1(op1), op2(op2) {
}

uint16_t AddInstruction::resolve(Process* process, const Operand& op) {
    if (op.isLiteral) return op.literal;
    return process->getVariable(op.name);
}

void AddInstruction::execute(Process* process) {
    int32_t a = resolve(process, op1);
    int32_t b = resolve(process, op2);
    process->setVariable(dest, clamp16(a + b));
    if (process->isViolationShutdown()) return;
    process->currentLine++;
}

SubtractInstruction::SubtractInstruction(const String& dest, const Operand& op1, const Operand& op2)
    : dest(dest), op1(op1), op2(op2) {
}

uint16_t SubtractInstruction::resolve(Process* process, const Operand& op) {
    if (op.isLiteral) return op.literal;
    return process->getVariable(op.name);
}

void SubtractInstruction::execute(Process* process) {
    int32_t a = resolve(process, op1);
    int32_t b = resolve(process, op2);
    process->setVariable(dest, clamp16(a - b));
    if (process->isViolationShutdown()) return;
    process->currentLine++;
}

// ---------------------------------------------------------------------------
// SLEEP
// ---------------------------------------------------------------------------

SleepInstruction::SleepInstruction(uint8_t ticks)
    : ticks(ticks) {
}

void SleepInstruction::execute(Process* process) {
    process->sleepTicks   = ticks;
    process->pendingSleep = true;
    process->currentLine++;
}

// ---------------------------------------------------------------------------
// FOR
// ---------------------------------------------------------------------------

ForInstruction::ForInstruction(const std::vector<IInstruction*>& body, int repeats)
    : body(body), repeats(repeats) {
}

ForInstruction::~ForInstruction() {
    for (auto ins : body)
        delete ins;
}

void ForInstruction::execute(Process* process) {
    int savedLine = process->currentLine;
    bool interrupted = false;
    for (int i = 0; i < repeats && !interrupted; ++i) {
        for (auto ins : body) {
            ins->execute(process);
            if (process->pendingSleep || process->isViolationShutdown()) { interrupted = true; break; }
        }
    }
    // If a memory violation fired mid-loop, leave currentLine exactly where the
    // faulting instruction left it, so screen -r/logs point at the real fault site.
    if (!process->isViolationShutdown()) {
        process->currentLine = savedLine + 1;
    }
}

// ---------------------------------------------------------------------------
// READ / WRITE - the actual demand-paged memory access instructions
// ---------------------------------------------------------------------------

ReadInstruction::ReadInstruction(const String& destVar, size_t address)
    : destVar(destVar), address(address) {
}

void ReadInstruction::execute(Process* process) {
    uint16_t value = 0;
    if (!process->readMemory(address, value)) {
        return; // violation already recorded on the process; halt without advancing
    }
    process->setVariable(destVar, value); // may be silently ignored if symbol table is full
    process->currentLine++;
}

WriteInstruction::WriteInstruction(size_t address, const Operand& valueOperand)
    : address(address), valueOperand(valueOperand) {
}

void WriteInstruction::execute(Process* process) {
    uint16_t value = valueOperand.isLiteral ? valueOperand.literal : process->getVariable(valueOperand.name);
    if (process->isViolationShutdown()) return;
    if (!process->writeMemory(address, value)) {
        return; // violation already recorded; halt without advancing
    }
    process->currentLine++;
}

// ---------------------------------------------------------------------------
// screen -c instruction-string parser
// ---------------------------------------------------------------------------

namespace {

String trim(const String& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == String::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<String> splitTopLevel(const String& text, char delim) {
    // Splits on `delim`, but ignores delimiters that appear inside a
    // double-quoted string, so PRINT("a; b") isn't torn in half.
    std::vector<String> parts;
    String current;
    bool inQuotes = false;
    for (char c : text) {
        if (c == '"') inQuotes = !inQuotes;
        if (c == delim && !inQuotes) {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

bool isValidIdentifier(const String& s) {
    if (s.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') return false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    }
    return true;
}

// Parses a decimal literal in [0, 65535]. Returns false if it isn't one.
bool parseUint16Literal(const String& s, uint16_t& out) {
    if (s.empty()) return false;
    for (char c : s) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    long val;
    try { val = std::stol(s); } catch (...) { return false; }
    if (val < 0 || val > 65535) return false;
    out = static_cast<uint16_t>(val);
    return true;
}

// Parses either a decimal literal or a variable name into an Operand.
bool parseOperand(const String& s, AddInstruction::Operand& out) {
    uint16_t lit;
    if (parseUint16Literal(s, lit)) {
        out.isLiteral = true; out.literal = lit; out.name.clear();
        return true;
    }
    if (isValidIdentifier(s)) {
        out.isLiteral = false; out.literal = 0; out.name = s;
        return true;
    }
    return false;
}

// Parses a "0x..." hex address. Range-checks against processMemSize (minus 1
// byte, since every access here is a uint16 -> needs address+1 too).
bool parseHexAddress(const String& s, size_t processMemSize, size_t& outAddress, String& err) {
    (void)processMemSize; // kept in the signature for callers/readability; see note below
    if (s.size() < 3 || (s[0] != '0') || (s[1] != 'x' && s[1] != 'X')) {
        err = "expected a hex address like 0x500, got '" + s + "'";
        return false;
    }
    for (size_t i = 2; i < s.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) {
            err = "invalid hex address '" + s + "'";
            return false;
        }
    }
    unsigned long long val;
    try { val = std::stoull(s.substr(2), nullptr, 16); } catch (...) {
        err = "invalid hex address '" + s + "'";
        return false;
    }
    // Note: we intentionally do NOT reject val >= processMemSize here as a
    // *parse* error - an out-of-range address is a legal instruction that is
    // supposed to compile fine and cause a runtime memory access violation
    // when it actually executes (that's the whole point of the feature).
    outAddress = static_cast<size_t>(val);
    return true;
}

std::vector<String> splitWhitespace(const String& s) {
    std::istringstream iss(s);
    std::vector<String> tokens;
    String tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

// Parses the inside of PRINT(...). Supports:
//   "literal text"
//   varName
//   "prefix" + varName
bool parsePrint(const String& insideParens, IInstruction*& outInstruction, String& err) {
    String s = trim(insideParens);
    size_t firstQuote = s.find('"');

    if (firstQuote == String::npos) {
        // Bare identifier: PRINT(varName)
        if (!isValidIdentifier(s)) { err = "PRINT expects a quoted string, a variable, or \"text\" + var"; return false; }
        outInstruction = new PrintInstruction(s, /*isVar=*/true, /*useDefault=*/false);
        return true;
    }

    size_t secondQuote = s.find('"', firstQuote + 1);
    if (secondQuote == String::npos) { err = "unterminated string in PRINT"; return false; }
    String literalText = s.substr(firstQuote + 1, secondQuote - firstQuote - 1);

    String remainder = trim(s.substr(secondQuote + 1));
    if (remainder.empty()) {
        outInstruction = new PrintInstruction(literalText, /*isVar=*/false, /*useDefault=*/false);
        return true;
    }
    if (remainder[0] != '+') { err = "expected '+' after quoted string in PRINT"; return false; }
    String varPart = trim(remainder.substr(1));
    if (!isValidIdentifier(varPart)) { err = "expected a variable name after '+' in PRINT"; return false; }

    outInstruction = new PrintInstruction(literalText, varPart, true);
    return true;
}

} // namespace

bool ParseInstructionList(const String& text, size_t processMemSize,
                          std::vector<IInstruction*>& outInstructions,
                          String& errorMessage) {
    outInstructions.clear();

    std::vector<String> rawParts = splitTopLevel(text, ';');
    std::vector<String> tokens;
    for (size_t i = 0; i < rawParts.size(); ++i) {
        String t = trim(rawParts[i]);
        bool isTrailingEmpty = (i == rawParts.size() - 1) && t.empty();
        if (t.empty() && !isTrailingEmpty) {
            errorMessage = "invalid command (empty instruction between ';' separators)";
            return false;
        }
        if (!t.empty()) tokens.push_back(t);
    }

    if (tokens.empty() || tokens.size() > 50) {
        errorMessage = "invalid command (instruction count " + std::to_string(tokens.size()) + " not in [1, 50])";
        return false;
    }

    for (const String& instr : tokens) {
        IInstruction* built = nullptr;
        String err;
        bool ok = true;

        // PRINT(...) is special-cased since its payload isn't whitespace-tokenized.
        size_t printParen = instr.find("PRINT(");
        if (printParen == 0) {
            size_t closeParen = instr.rfind(')');
            if (closeParen == String::npos || closeParen < 6) {
                ok = false; err = "malformed PRINT(...)";
            } else {
                String inside = instr.substr(6, closeParen - 6);
                ok = parsePrint(inside, built, err);
            }
        } else {
            std::vector<String> tok = splitWhitespace(instr);
            if (tok.empty()) {
                ok = false; err = "empty instruction";
            } else if (tok[0] == "DECLARE") {
                uint16_t val;
                if (tok.size() != 3 || !isValidIdentifier(tok[1]) || !parseUint16Literal(tok[2], val)) {
                    ok = false; err = "usage: DECLARE <var> <0-65535>";
                } else {
                    built = new DeclareInstruction(tok[1], val);
                }
            } else if (tok[0] == "ADD" || tok[0] == "SUBTRACT") {
                AddInstruction::Operand a, b;
                if (tok.size() != 4 || !isValidIdentifier(tok[1]) || !parseOperand(tok[2], a) || !parseOperand(tok[3], b)) {
                    ok = false; err = "usage: " + tok[0] + " <dest> <op1> <op2>";
                } else if (tok[0] == "ADD") {
                    built = new AddInstruction(tok[1], a, b);
                } else {
                    built = new SubtractInstruction(tok[1], a, b);
                }
            } else if (tok[0] == "SLEEP") {
                uint16_t val;
                if (tok.size() != 2 || !parseUint16Literal(tok[1], val) || val > 255) {
                    ok = false; err = "usage: SLEEP <0-255>";
                } else {
                    built = new SleepInstruction(static_cast<uint8_t>(val));
                }
            } else if (tok[0] == "READ") {
                size_t addr;
                if (tok.size() != 3 || !isValidIdentifier(tok[1]) || !parseHexAddress(tok[2], processMemSize, addr, err)) {
                    if (err.empty()) err = "usage: READ <var> <0xHEX_ADDRESS>";
                    ok = false;
                } else {
                    built = new ReadInstruction(tok[1], addr);
                }
            } else if (tok[0] == "WRITE") {
                size_t addr;
                AddInstruction::Operand val;
                if (tok.size() != 3 || !parseHexAddress(tok[1], processMemSize, addr, err) || !parseOperand(tok[2], val)) {
                    if (err.empty()) err = "usage: WRITE <0xHEX_ADDRESS> <value>";
                    ok = false;
                } else {
                    built = new WriteInstruction(addr, val);
                }
            } else {
                ok = false; err = "unrecognized instruction '" + tok[0] + "'";
            }
        }

        if (!ok || built == nullptr) {
            for (auto* ins : outInstructions) delete ins;
            outInstructions.clear();
            errorMessage = "invalid command" + (err.empty() ? String("") : (": " + err));
            return false;
        }
        outInstructions.push_back(built);
    }

    return true;
}
