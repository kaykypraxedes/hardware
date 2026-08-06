/* Instruction/InstructionArm64.cpp */
#include "headers/InstructionArm64.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{6};

// Opcodes da arquitetura:
static const std::vector<std::string> LOADS
    {"ldr", "ldur", "ldp", "ldrb", "ldrh", "ldrsb", "ldrsh", "ldurb", "ldurh", "ldursb", "ldursh"};

static const std::vector<std::string> STORES
    {"str", "stur", "stp", "strb", "strh", "sturb", "sturh"};

static const std::vector<std::string> INT_BASIC
    {"add", "adds", "sub", "subs", "and", "orr", "eor", "lsl", "lsr", "mov", "movz", "movk", "movn", "mvn", "bic", "eon", "orn", "cmp", "cmn", "tst", "neg", "adc", "sbc", "asr", "ror"};

static const std::vector<std::string> INT_MUL
    {"mul", "smull", "umull", "madd", "msub", "smaddl", "umaddl"};

static const std::vector<std::string> INT_DIV
    {"sdiv", "udiv"};

static const std::vector<std::string> BRANCHES
    {"b", "b.eq", "b.ne", "bl", "ret", "cbz", "cbnz", "b.lt", "b.gt", "b.le", "b.ge", "b.hs", "b.hi", "b.ls", "b.lo", "tbz", "tbnz", "br"};

static const std::vector<std::string> FLOAT_BASIC
    {"fadd", "fsub", "fsqrt", "fcvt", "scvtf", "ucvtf", "fcvtzs", "fcvtzu", "fabs", "fneg", "fmin", "fmax", "fmla", "fcmp"};

static const std::vector<std::string> FLOAT_MUL
    {"fmul", "fmadd", "fmsub", "fnmadd", "fnmsub"};

static const std::vector<std::string> FLOAT_DIV
    {"fdiv"};

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
    if (first != 'X' && first != 'W' && first != 'D' && first != 'S') return false;
    for (size_t i = 1; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

// Monta o CDB com os registradores físicos:
CDB InstructionArm64::MakeCDB() {
    // Aliases compartilham o mesmo id:
    // - ids 0-30:  'L' = 'R'.
    // - ids 32-63: 'S' = 'F'.
    // - id 80:     'G'.
    CDB cdb;
    cdb.registers.resize(81);
    fillCDB(cdb, 'L', 0,  31);  // Faixas de int: L e R compartilham os slots 0-30.
    fillCDB(cdb, 'R', 0,  31);
    fillCDB(cdb, 'S', 32, 32);  // Faixas de float: S e F compartilham os slots 32-63.
    fillCDB(cdb, 'F', 32, 32);
    fillCDB(cdb, 'G', 80, 1);
    cdb.print_banks = {{'L', 0, 31}, {'R', 0, 31}, {'S', 32, 32}, {'F', 32, 32}, {'G', 80, 1}};
    return cdb;
}

// Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Aliases compartilham o mesmo id:
    // - ids 0-30:  X0-30 ('L') = W0-30 ('R').
    // - ids 32-63: D0-31 ('S') = S0-31 ('F').
    // - id 80:     CPSR ('G').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-30):
        for (int i = 0; i < 31; i++){
            t.emplace("x" + std::to_string(i), Register('L', i));
            t.emplace("w" + std::to_string(i), Register('R', i));
        }

        // Float (32-63):
        for (int i = 0; i < 32; i++) {
            t.emplace("d" + std::to_string(i), Register('S', 32 + i));
            t.emplace("s" + std::to_string(i), Register('F', 32 + i));
        }

        t.emplace("cpsr", Register('G', 80));
    }

    return t;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionArm64::InstructionArm64(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionArm64::IdentifyType(
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
    // Evita perda de informação do último caracter.
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os opcodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionArm64::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: só opcode e registradores viram minúsculo.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i])) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

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

    // Imediatos ("#5", "#0x10") e labels não viram fonte (só registradores).
    if (type == INSTRUCTION_TYPE::LOAD) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2) source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::STORE) {
        source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2) source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        if (tokens[0].find(".eq") != std::string::npos || tokens[0].find(".ne") != std::string::npos) {
            source_registers.push_back(Register('G', 80));
        }
    } else if (tokens[0] == "cmp" || tokens[0] == "cmn" || tokens[0] == "tst" || tokens[0] == "fcmp") {
        // Comparadores não escrevem registrador de dados: apenas CPSR.
        dest_registers.push_back(Register('G', 80));
        for (size_t i = 1; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens[0].back() == 's') dest_registers.push_back(Register('G', 80)); // ADDS atualiza CPSR.
        for (size_t i = 2; i < tokens.size(); ++i)
            if (IsRegister(tokens[i]))
                source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
    }
}

} // namespace processor
