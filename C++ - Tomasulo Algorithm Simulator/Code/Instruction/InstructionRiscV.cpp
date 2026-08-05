/* Instruction/InstructionRiscV.cpp */
#include "headers/InstructionRiscV.h"

namespace processor {

static const std::vector<std::string> RISCV_LOADS = {"LW", "LH", "LB", "LBU", "LHU", "FLD", "FLW"};
static const std::vector<std::string> RISCV_STORES = {"SW", "SH", "SB", "FSD", "FSW"};
static const std::vector<std::string> RISCV_INT_BASIC = {"ADD", "ADDI", "SUB", "AND", "ANDI", "OR", "ORI", "XOR", "XORI", "SLL", "SRL", "SRA"};
static const std::vector<std::string> RISCV_INT_MUL = {"MUL", "MULH", "MULHU"};
static const std::vector<std::string> RISCV_INT_DIV = {"DIV", "DIVU", "REM", "REMU"};
static const std::vector<std::string> RISCV_BRANCHES = {"BEQ", "BNE", "BLT", "BGE", "BLTU", "BGEU", "JAL", "JALR"};
static const std::vector<std::string> RISCV_FLOAT_BASIC = {"FADD.S", "FADD.D", "FSUB.S", "FSUB.D"};
static const std::vector<std::string> RISCV_FLOAT_MUL = {"FMUL.S", "FMUL.D"};
static const std::vector<std::string> RISCV_FLOAT_DIV = {"FDIV.S", "FDIV.D"};

static bool Contains(const std::vector<std::string>& vec, const std::string& op) {
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

InstructionRiscV::InstructionRiscV(const int position) : Instruction(position) {}

bool InstructionRiscV::IdentifyType(const std::string& prev_op) {
    std::string op = prev_op;
    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (Contains(RISCV_LOADS, op))         type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(RISCV_STORES, op))      type = INSTRUCTION_TYPE::STORE;
    else if (Contains(RISCV_INT_BASIC, op))   type = INSTRUCTION_TYPE::INT_BASIC;
    else if (Contains(RISCV_BRANCHES, op))    type = INSTRUCTION_TYPE::BRANCH;
    else if (Contains(RISCV_INT_MUL, op))     type = INSTRUCTION_TYPE::INT_MUL;
    else if (Contains(RISCV_INT_DIV, op))     type = INSTRUCTION_TYPE::INT_DIV;
    else if (Contains(RISCV_FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (Contains(RISCV_FLOAT_MUL, op))   type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (Contains(RISCV_FLOAT_DIV, op))   type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

std::vector<std::string> InstructionRiscV::SplitInstruction(const std::string& str) const {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : str) {
        if (c == ',' || c == ' ' || c == '(' || c == ')' || c == '\t') {
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

void InstructionRiscV::NormalizeInstruction(std::vector<std::string>& tokens) {
    for (std::string& token : tokens)
        for (char& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string normalized = tokens[0];
    while (normalized.length() < 7) normalized += ' ';

    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";
    } else {
        for (size_t i = 1; i < tokens.size(); ++i)
            normalized += (i == 1 ? "" : ", ") + tokens[i];
    }
    instruction_string = normalized;
}

void InstructionRiscV::SetAttributes(const std::vector<std::string>& tokens) {
    dest_registers.clear();
    source_registers.clear();

    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(Register(tokens[1]));
        source_registers.push_back(Register(tokens[3]));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        source_registers.push_back(Register(tokens[1]));
        source_registers.push_back(Register(tokens[3]));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        source_registers.push_back(Register(tokens[1]));
        source_registers.push_back(Register(tokens[2]));
    } else {
        dest_registers.push_back(Register(tokens[1]));
        if (tokens.size() > 2) source_registers.push_back(Register(tokens[2]));
        if (tokens.size() > 3) source_registers.push_back(Register(tokens[3]));
    }
}

} // namespace processor
