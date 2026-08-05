/* Instruction/InstructionArm64.cpp */
#include "headers/InstructionArm64.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const std::vector<std::string> LOADS =
    {"LDR", "LDUR", "LDP"};

static const std::vector<std::string> STORES =
    {"STR", "STUR", "STP"};

static const std::vector<std::string> INT_BASIC =
    {"ADD", "ADDS", "SUB", "SUBS", "AND", "ORR", "EOR", "LSL", "LSR"};

static const std::vector<std::string> INT_MUL =
    {"MUL", "SMULL", "UMULL"};

static const std::vector<std::string> INT_DIV =
    {"SDIV", "UDIV"};

static const std::vector<std::string> BRANCHES =
    {"B", "B.EQ", "B.NE", "BL", "RET", "CBZ", "CBNZ"};

static const std::vector<std::string> FLOAT_BASIC =
    {"FADD", "FSUB"};

static const std::vector<std::string> FLOAT_MUL =
    {"FMUL"};

static const std::vector<std::string> FLOAT_DIV =
    {"FDIV"};

static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionArm64::InstructionArm64(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionArm64::IdentifyType(
    const std::string& prev_op
){
    std::string op{prev_op};
    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (Contains(LOADS, op))            type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(STORES, op))      type = INSTRUCTION_TYPE::STORE;
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
std::vector<std::string> InstructionArm64::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : str) {
        if (c == ',' || c == ' ' || c == '[' || c == ']' || c == '#' || c == '\t') {
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
void InstructionArm64::NormalizeInstruction(
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
void InstructionArm64::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(Register(tokens[1]));
        if (tokens.size() > 2) source_registers.push_back(Register(tokens[2]));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        source_registers.push_back(Register(tokens[1]));
        if (tokens.size() > 2) source_registers.push_back(Register(tokens[2]));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        if (tokens[0].find(".EQ") != std::string::npos || tokens[0].find(".NE") != std::string::npos) {
            source_registers.push_back(Register("CPSR"));
        }
    } else {
        dest_registers.push_back(Register(tokens[1]));
        if (tokens[0].back() == 'S') dest_registers.push_back(Register("CPSR")); // ADDS atualiza CPSR.
        if (tokens.size() > 2) source_registers.push_back(Register(tokens[2]));
        if (tokens.size() > 3) source_registers.push_back(Register(tokens[3]));
    }
}

} // namespace processor
