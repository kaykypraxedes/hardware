/* Instruction/InstructionSimplified.cpp */
#include "headers/InstructionSimplified.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{7};

// Opcodes da arquitetura:
static const std::vector<std::string> LOADS
    {"load", "lw", "lb", "lh", "lbu", "lhu", "l.d", "l.s", "ld", "lwu", "ll"};

static const std::vector<std::string> STORES
    {"store", "sw", "sb", "sh", "s.d", "s.s", "sd", "sc"};

static const std::vector<std::string> INT_BASIC
    {"add", "addi", "addu", "addiu", "daddu", "daddiu", "sub", "subi", "subu", "dsubu", "and", "andi", "or", "ori", "xor", "xori", "nor", "lui", "sll", "srl", "sra", "sllv", "srlv", "srav", "dsll", "dsrl", "dsra", "slt", "slti", "sltu", "sltiu", "dslt", "dslti", "dsltu", "dsltiu"};

static const std::vector<std::string> BRANCHES
    {"beq", "bne", "bnez", "beqz", "bgtz", "bltz", "bgez", "blez", "bltzal", "bgezal", "j", "jal", "jr", "jalr"};

static const std::vector<std::string> INT_MUL
    {"mult", "multu", "mul", "dmult", "dmultu"};

static const std::vector<std::string> INT_DIV
    {"div", "divu", "ddiv", "ddivu"};

static const std::vector<std::string> FLOAT_BASIC
    {"add.d", "add.s", "sub.d", "sub.s", "abs.s", "neg.s", "sqrt.s", "cvt.s.w", "cvt.d.w", "cvt.s.d", "cvt.d.s", "cvt.w.s", "cvt.w.d", "c.le.s", "c.lt.s", "c.eq.s"};

static const std::vector<std::string> FLOAT_MUL
    {"mul.d", "mul.s"};

static const std::vector<std::string> FLOAT_DIV
    {"div.d", "div.s"};

// Verifica se o opcode existe (está na tabela).
static bool Contains(
    const std::vector<std::string>& vec,
    const std::string& op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// Verifica se o dado é um registrador ou um dado que não precisa ser armazenado (labels, imediatos, etc.)
static bool IsRegister(
    const std::string& token
){
    if (token.size() < 2) return false;
    char first = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (first != 'R' && first != 'F') return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

// Monta o CDB com os registradores físicos:
CDB InstructionSimplified::MakeCDB() {
    // - ids 0-31:  'R'.
    // - ids 32-63: 'F'.
    CDB cdb;
    cdb.registers.resize(64);
    fillCDB(cdb, 'R', 0,  32); // Faixa de int.
    fillCDB(cdb, 'F', 32, 32); // Faixa de float.
    cdb.print_banks = {{'R', 0, 32}, {'F', 32, 32}};
    return cdb;
}

// Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // - ids 0-31:  R0..31 ('R').
    // - ids 32-63: F0..31 ('F').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).
        for (int i = 0; i < 32; i++) {
            // Int (0-31):
            t.emplace("r" + std::to_string(i), Register('R', i));
            // Float (32-63):
            t.emplace("f" + std::to_string(i), Register('F', 32 + i));
        }
    }
    return t;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionSimplified::InstructionSimplified(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionSimplified::IdentifyType(
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
std::vector<std::string> InstructionSimplified::SplitInstruction(
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
void InstructionSimplified::NormalizeInstruction(
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
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string& token = tokens[i];
            if (IsRegister(token))
                for (char& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            normalized += (i == 1 ? "" : ", ") + token;
        }
    } else {
        for (size_t i = 1; i < tokens.size(); ++i)
            normalized += (i == 1 ? "" : ", ") + tokens[i];
    }
    instruction_string = normalized;
}

// Privado:
void InstructionSimplified::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    // Imediatos ("#5", "#0x10") e labels não viram fonte (só registradores).
    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        if (tokens.size() > 1 && IsRegister(tokens[1]))
            source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2 && IsRegister(tokens[2]))
            source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
    } else {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2 && IsRegister(tokens[2]))
            source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
        if (tokens.size() > 3 && IsRegister(tokens[3]))
            source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
    }
}

} // namespace processor
