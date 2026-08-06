/* Instruction/InstructionRiscV.cpp */
#include "headers/InstructionRiscV.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{8};

// Opcodes da arquitetura:
static const std::vector<std::string> LOADS
    {"lw", "lh", "lb", "lbu", "lhu", "fld", "flw", "ld", "lwu"};

static const std::vector<std::string> STORES
    {"sw", "sh", "sb", "fsd", "fsw", "sd"};

static const std::vector<std::string> INT_BASIC
    {"add", "addi", "sub", "and", "andi", "or", "ori", "xor", "xori", "sll", "srl", "sra", "slli", "srli", "srai", "slt", "sltu", "slti", "sltiu", "lui", "auipc"};

static const std::vector<std::string> INT_MUL
    {"mul", "mulh", "mulhu"};

static const std::vector<std::string> INT_DIV
    {"div", "divu", "rem", "remu"};

static const std::vector<std::string> BRANCHES
    {"beq", "bne", "blt", "bge", "bltu", "bgeu", "jal", "jalr"};

static const std::vector<std::string> FLOAT_BASIC
    {"fadd.s", "fadd.d", "fsub.s", "fsub.d", "fsqrt.s", "fsqrt.d", "fmin.s", "fmin.d", "fmax.s", "fmax.d", "fabs.s", "fabs.d", "fneg.s", "fneg.d", "fle.s", "flt.s", "feq.s", "fcvt.s.w", "fcvt.w.s", "fcvt.s.d", "fcvt.d.s", "fcvt.d.w", "fcvt.w.d", "fmv.x.w", "fmv.w.x"};

static const std::vector<std::string> FLOAT_MUL
    {"fmul.s", "fmul.d", "fmadd.s", "fmsub.s", "fnmadd.s", "fnmsub.s"};

static const std::vector<std::string> FLOAT_DIV
    {"fdiv.s", "fdiv.d"};

// Verifica se o opcode existe (está na tabela).
static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// Verifica se o dado é um registrador ou um dado que não precisa ser armazenado (labels, imediatos, etc.)
static bool IsRegister(
    const std::string& token
){
    if (token.size() < 2) return false;
    char first = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (first != 'X' && first != 'F') return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

// Monta o CDB com os registradores físicos:
CDB InstructionRiscV::MakeCDB() {
    // - ids 0-31:  'L'.
    // - ids 32-63: 'F'.
    CDB cdb;
    cdb.registers.resize(64);
    fillCDB(cdb, 'L', 0, 32);  // Faixa de int.
    fillCDB(cdb, 'F', 32, 32); // Faixa de float.
    cdb.print_banks = {{'F', 32, 32}, {'L', 0, 32}};
    return cdb;
}

// Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // - ids 0-31:  x0..31 ('L').
    // - ids 32-63: f0..31 ('F').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).
        for (int i = 0; i < 32; i++){
            // Int (0-31):
            t.emplace("x" + std::to_string(i), Register('L', i));
            // Float (32-63):
            t.emplace("f" + std::to_string(i), Register('F', 32 + i));
        }
    }
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
    for (char& c : op) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

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
    // Evita perda de informação do último caracter.
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os opcodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionRiscV::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: só opcode e registradores viram minúsculo.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i])) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

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

    // Imediatos ("8", "0x10") e labels não viram fonte (só registradores).
    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        for (size_t i = 2; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        for (size_t i = 1; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        for (size_t i = 1; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        for (size_t i = 2; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    }
}

} // namespace processor
