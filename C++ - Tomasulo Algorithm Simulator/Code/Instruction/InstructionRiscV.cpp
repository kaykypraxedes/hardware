/* Instruction/InstructionRiscV.cpp */
#include "headers/InstructionRiscV.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const std::vector<std::string> LOADS
    {"LW", "LH", "LB", "LBU", "LHU", "FLD", "FLW", "LD", "LWU"};

static const std::vector<std::string> STORES
    {"SW", "SH", "SB", "FSD", "FSW", "SD"};

static const std::vector<std::string> INT_BASIC
    {"ADD", "ADDI", "SUB", "AND", "ANDI", "OR", "ORI", "XOR", "XORI", "SLL", "SRL", "SRA", "SLLI", "SRLI", "SRAI", "SLT", "SLTU", "SLTI", "SLTIU", "LUI", "AUIPC"};

static const std::vector<std::string> INT_MUL
    {"MUL", "MULH", "MULHU"};

static const std::vector<std::string> INT_DIV
    {"DIV", "DIVU", "REM", "REMU"};

static const std::vector<std::string> BRANCHES
    {"BEQ", "BNE", "BLT", "BGE", "BLTU", "BGEU", "JAL", "JALR"};

static const std::vector<std::string> FLOAT_BASIC
    {"FADD.S", "FADD.D", "FSUB.S", "FSUB.D", "FSQRT.S", "FSQRT.D", "FMIN.S", "FMIN.D", "FMAX.S", "FMAX.D", "FABS.S", "FABS.D", "FNEG.S", "FNEG.D", "FLE.S", "FLT.S", "FEQ.S", "FCVT.S.W", "FCVT.W.S", "FCVT.S.D", "FCVT.D.S", "FCVT.D.W", "FCVT.W.D", "FMV.X.W", "FMV.W.X"};

static const std::vector<std::string> FLOAT_MUL
    {"FMUL.S", "FMUL.D", "FMADD.S", "FMSUB.S", "FNMADD.S", "FNMSUB.S"};

static const std::vector<std::string> FLOAT_DIV
    {"FDIV.S", "FDIV.D"};

static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// Monta o CDB com os registradores físicos:
// - ids 0-31:  x0..31 ('L').
// - ids 32-63: f0..31 ('F').
CDB InstructionRiscV::MakeCDB() {
    CDB cdb;
    cdb.registers.resize(64);
    fillCDB(cdb, 'L', 0, 32);
    fillCDB(cdb, 'F', 32, 32);
    cdb.print_banks = {{'F', 32, 32}, {'L', 0, 32}};
    return cdb;
}

// Tabela nome -> (classe, id físico global):
// - ids 0-31:  x0..31 ('L').
// - ids 32-63: f0..31 ('F').
const std::unordered_map<std::string, Register>& RegisterTable() {
    static std::unordered_map<std::string, Register> t;
    for (int i = 0; i < 32; i++) t.emplace("x" + std::to_string(i), Register('L', i));
    for (int i = 0; i < 32; i++) t.emplace("f" + std::to_string(i), Register('F', 32 + i));
    return t;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionRiscV::InstructionRiscV(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionRiscV::IdentifyType(
    const std::vector<std::string>& tokens
){
    std::string op{tokens[0]};
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
std::vector<std::string> InstructionRiscV::SplitInstruction(
    const std::string& str
) const {
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

// Privado:
void InstructionRiscV::NormalizeInstruction(
    std::vector<std::string>& tokens
){
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

// Privado:
void InstructionRiscV::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    // Imediatos ("8", "0x10") e labels não viram fonte: só registradores x/f.
    // (NormalizeInstruction já deixou tudo minúsculo.)
    auto is_register = [](const std::string& token) {
        return token.size() > 1 && (token[0] == 'x' || token[0] == 'f');
    };

    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        for (size_t i = 2; i < tokens.size(); ++i)
            if (is_register(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        for (size_t i = 1; i < tokens.size(); ++i)
            if (is_register(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        for (size_t i = 1; i < tokens.size(); ++i)
            if (is_register(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        for (size_t i = 2; i < tokens.size(); ++i)
            if (is_register(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    }
}

} // namespace processor
