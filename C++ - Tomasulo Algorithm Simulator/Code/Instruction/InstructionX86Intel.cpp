/* Instruction/InstructionX86Intel.cpp */
#include "headers/InstructionX86Intel.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const std::vector<std::string> LOADS =
    {"MOV", "MOVSS", "MOVSD", "LEA"};

static const std::vector<std::string> STORES =
    {"MOV_STORE"};

static const std::vector<std::string> INT_BASIC =
    {"ADD", "SUB", "AND", "OR", "XOR", "INC", "DEC", "CMP", "SHL", "SHR"};

static const std::vector<std::string> INT_MUL =
    {"IMUL", "MUL"};

static const std::vector<std::string> INT_DIV =
    {"IDIV", "DIV"};

static const std::vector<std::string> BRANCHES =
    {"JMP", "JE", "JNE", "JG", "JGE", "JL", "JLE", "CALL"};

static const std::vector<std::string> FLOAT_BASIC =
    {"ADDSS", "ADDSD", "SUBSS", "SUBSD"};

static const std::vector<std::string> FLOAT_MUL =
    {"MULSS", "MULSD"};

static const std::vector<std::string> FLOAT_DIV =
    {"DIVSS", "DIVSD"};

static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionX86Intel::InstructionX86Intel(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionX86Intel::IdentifyType(
    const std::string& prev_op
){
    std::string op = prev_op;
    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (Contains(LOADS, op))            type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(INT_BASIC, op))   type = INSTRUCTION_TYPE::INT_BASIC;
    else if (Contains(BRANCHES, op))    type = INSTRUCTION_TYPE::BRANCH;
    else if (Contains(INT_MUL, op))     type = INSTRUCTION_TYPE::INT_MUL;
    else if (Contains(INT_DIV, op))     type = INSTRUCTION_TYPE::INT_DIV;
    else if (Contains(FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (Contains(FLOAT_MUL, op))   type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (Contains(FLOAT_DIV, op))   type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

// Privado:
std::vector<std::string> InstructionX86Intel::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;
    bool in_bracket = false;

    for (char c : str) {
        if (c == '[') in_bracket = true;
        if (c == ']') in_bracket = false;

        if ((c == ',' || c == ' ' || c == '\t') && !in_bracket) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
void InstructionX86Intel::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (std::string& token : tokens)
        for (char& c : token) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    std::string normalized = tokens[0];
    while (normalized.length() < 7) normalized += ' ';

    for (size_t i = 1; i < tokens.size(); ++i)
        normalized += (i == 1 ? "" : ", ") + tokens[i];

    instruction_string = normalized;
}

// Privado:
void InstructionX86Intel::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    // No x86 Intel, o primeiro operando costuma ser o Destino/Fonte (ex: ADD EAX, EBX -> EAX = EAX + EBX)
    if (type == INSTRUCTION_TYPE::BRANCH) {
        // Desvios condicionais leem EFLAGS
        source_registers.push_back(Register("EFLAGS"));
    } else if (tokens.size() > 1) {
        dest_registers.push_back(Register(tokens[1]));
        // Operações aritméticas alteram EFLAGS implicitamente e usam o destino como fonte inicial
        dest_registers.push_back(Register("EFLAGS"));
        source_registers.push_back(Register(tokens[1]));

        if (tokens.size() > 2) {
            source_registers.push_back(Register(tokens[2]));
        }
    }
}

} // namespace processor
