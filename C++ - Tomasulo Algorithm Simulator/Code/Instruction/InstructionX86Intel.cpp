/* Instruction/InstructionX86Intel.cpp */
#include "headers/InstructionX86Intel.h"
#include <unordered_map>

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

// Monta o CDB com os registradores físicos:
// - Slots 0-15 inteiros ('R', 'L', 'W' e 'B' compartilhando), 64-79 vetorial ('V') e 80 flags ('G').
CDB InstructionX86Intel::MakeCDB() {
    CDB cdb;
    cdb.registers.resize(81);
    fillCDB(cdb, 'L', 0, 16);   // Faixas inteiras: L, R, W, B compartilham os slots 0-15.
    fillCDB(cdb, 'R', 0, 16);
    fillCDB(cdb, 'W', 0, 16);
    fillCDB(cdb, 'B', 0, 16);
    fillCDB(cdb, 'V', 64, 16);
    fillCDB(cdb, 'G', 80, 1);
    cdb.print_banks = {{'R', 0, 16}, {'L', 0, 16}, {'V', 64, 16}, {'W', 0, 16}, {'B', 0, 16}, {'G', 80, 1}};
    return cdb;
}

// Tabela nome -> (classe, id físico global).
// Aliases compartilham o mesmo id:
// - ids 0-15:  RAX..R15 ('L') = EAX..R15D ('R') = AX..R15W ('W') = AL/AH..R15B ('B').
// - ids 64-79: XMM0-15 ('V').
// - id 80:     EFLAGS ('G').
const std::unordered_map<std::string, Register>& RegisterTable() {
    static std::unordered_map<std::string, Register> t;
    // 64-bit (L, 0-15):
    const char* l64[] = {"RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RSP", "RBP"};
    for (int i = 0; i < 8;  i++) t.emplace(l64[i], Register('L', i));
    for (int i = 8; i < 16; i++) t.emplace("R" + std::to_string(i), Register('L', i));

    // 32-bit (R, 0-15):
    const char* r32[] = {"EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "ESP", "EBP"};
    for (int i = 0; i < 8;  i++) t.emplace(r32[i], Register('R', i));
    for (int i = 8; i < 16; i++) t.emplace("R" + std::to_string(i) + "D", Register('R', i));

    // 16-bit (W, 0-15):
    const char* w16[] = {"AX", "BX", "CX", "DX", "SI", "DI", "SP", "BP"};
    for (int i = 0; i < 8;  i++) t.emplace(w16[i], Register('W', i));
    for (int i = 8; i < 16; i++) t.emplace("R" + std::to_string(i) + "W", Register('W', i));

    // 8-bit (B, 0-15) — AL/AH, BL/BH, ... compartilham o id do grupo.
    const char* b8[] = {"AL", "AH", "BL", "BH", "CL", "CH", "DL", "DH"};
    for (int i = 0; i < 8; i++) t.emplace(b8[i], Register('B', i / 2));
    t.emplace("SIL", Register('B', 4));
    t.emplace("DIL", Register('B', 5));
    t.emplace("SPL", Register('B', 6));
    t.emplace("BPL", Register('B', 7));
    for (int i = 8; i < 16; i++) t.emplace("R" + std::to_string(i) + "B", Register('B', i));

    // Vetorial (V, 64-79)
    for (int i = 0; i < 16; i++) t.emplace("XMM" + std::to_string(i), Register('V', 64 + i));

    // Flags (G, 80):
    t.emplace("EFLAGS", Register('G', 80));
    return t;
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
        source_registers.push_back(Register('G', 80));
    } else if (tokens.size() > 1) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        // Operações aritméticas alteram EFLAGS implicitamente e usam o destino como fonte inicial
        dest_registers.push_back(Register('G', 80));
        source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));

        if (tokens.size() > 2) {
            source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
        }
    }
}

} // namespace processor
