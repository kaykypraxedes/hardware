/* Instruction/InstructionMips32.cpp */
#include "headers/InstructionMips32.h"

namespace processor {

static const std::vector<std::string> MIPS_LOADS =
    {"LOAD", "LW", "LB", "LH", "LBU", "LHU", "L.D", "L.S", "LD", "LWU", "LL"};

static const std::vector<std::string> MIPS_STORES =
    {"STORE", "SW", "SB", "SH", "S.D", "S.S", "SD", "SC"};

static const std::vector<std::string> MIPS_INT_BASIC =
    {"ADD", "ADDI", "ADDU", "ADDIU", "DADDU", "DADDIU", "SUB", "SUBI", "SUBU", "DSUBU", "AND", "ANDI", "OR", "ORI", "XOR", "XORI", "NOR", "LUI", "SLL", "SRL", "SRA", "SLLV", "SRLV", "SRAV", "DSLL", "DSRL", "DSRA", "SLT", "SLTI", "SLTU", "SLTIU", "DSLT", "DSLTI", "DSLTU", "DSLTIU"};

static const std::vector<std::string> MIPS_BRANCHES =
    {"BEQ", "BNE", "BNEZ", "BEQZ", "BGTZ", "BLTZ", "BGEZ", "BLEZ", "BLTZAL", "BGEZAL", "J", "JAL", "JR", "JALR"};

static const std::vector<std::string> MIPS_INT_MUL =
    {"MULT", "MULTU", "MUL", "DMULT", "DMULTU"};

static const std::vector<std::string> MIPS_INT_DIV =
    {"DIV", "DIVU", "DDIV", "DDIVU"};

static const std::vector<std::string> MIPS_FLOAT_BASIC =
    {"ADD.D", "ADD.S", "SUB.D", "SUB.S"};

static const std::vector<std::string> MIPS_FLOAT_MUL =
    {"MUL.D", "MUL.S"};

static const std::vector<std::string> MIPS_FLOAT_DIV =
    {"DIV.D", "DIV.S"};

static bool Contains(const std::vector<std::string>& vec, const std::string& op) {
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

static bool IsRegister(const std::string& token) {
    if (token.size() < 2) return false;
    char first = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (first != 'R' && first != 'F') return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

InstructionMips32::InstructionMips32(const int position) : Instruction(position) {}

bool InstructionMips32::IdentifyType(const std::string& prev_op) {
    std::string op = prev_op;
    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (Contains(MIPS_LOADS, op))         type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(MIPS_STORES, op))      type = INSTRUCTION_TYPE::STORE;
    else if (Contains(MIPS_INT_BASIC, op))   type = INSTRUCTION_TYPE::INT_BASIC;
    else if (Contains(MIPS_BRANCHES, op))    type = INSTRUCTION_TYPE::BRANCH;
    else if (Contains(MIPS_INT_MUL, op))     type = INSTRUCTION_TYPE::INT_MUL;
    else if (Contains(MIPS_INT_DIV, op))     type = INSTRUCTION_TYPE::INT_DIV;
    else if (Contains(MIPS_FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (Contains(MIPS_FLOAT_MUL, op))   type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (Contains(MIPS_FLOAT_DIV, op))   type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

std::vector<std::string> InstructionMips32::SplitInstruction(const std::string& str) const {
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

void InstructionMips32::NormalizeInstruction(std::vector<std::string>& tokens) {
    for (std::string& token : tokens)
        for (char& c : token) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    std::string normalized = tokens[0];
    while (normalized.length() < 7) normalized += ' ';

    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string& token = tokens[i];
            if (!IsRegister(token))
                for (char& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            normalized += (i == 1 ? "" : ", ") + token;
        }
    } else {
        for (size_t i = 1; i < tokens.size(); ++i)
            normalized += (i == 1 ? "" : ", ") + tokens[i];
    }
    instruction_string = normalized;
}

void InstructionMips32::SetAttributes(const std::vector<std::string>& tokens) {
    dest_registers.clear();
    source_registers.clear();

    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(Register(tokens[1]));
        source_registers.push_back(Register(tokens[3]));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        source_registers.push_back(Register(tokens[1]));
        source_registers.push_back(Register(tokens[3]));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        if (tokens.size() > 1 && (tokens[1][0] == 'R' || tokens[1][0] == 'F'))
            source_registers.push_back(Register(tokens[1]));
        if (tokens.size() > 2 && (tokens[2][0] == 'R' || tokens[2][0] == 'F'))
            source_registers.push_back(Register(tokens[2]));
    } else {
        dest_registers.push_back(Register(tokens[1]));
        if (tokens.size() > 2 && (tokens[2][0] == 'R' || tokens[2][0] == 'F'))
            source_registers.push_back(Register(tokens[2]));
        if (tokens.size() > 3 && (tokens[3][0] == 'R' || tokens[3][0] == 'F'))
            source_registers.push_back(Register(tokens[3]));
    }
}

} // namespace processor
