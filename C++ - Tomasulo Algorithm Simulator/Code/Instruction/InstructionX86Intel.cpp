/* Instruction/InstructionX86Intel.cpp */
#include "headers/InstructionX86Intel.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const std::string MOV // Caso especial, já que pode agir como load, store e aritimético (com reg e com imediato).
    {"MOV"};

static const std::vector<std::string> LOADS
    {"MOVSS", "MOVSD", "LEA", "MOVAPS", "MOVUPS", "MOVQ", "MOVD"};

static const std::vector<std::string> INT_BASIC
    {"ADD", "SUB", "AND", "OR", "XOR", "INC", "DEC", "CMP", "SHL", "SHR", "NOT", "NEG", "TEST", "ROL", "ROR", "SAR", "SAL", "SBB", "ADC", "MOVSX", "MOVZX"};

static const std::vector<std::string> INT_MUL
    {"IMUL", "MUL"};

static const std::vector<std::string> INT_DIV
    {"IDIV", "DIV"};

static const std::vector<std::string> BRANCHES
    {"JMP", "JE", "JNE", "JG", "JGE", "JL", "JLE", "CALL", "JBE", "JA", "JAE", "JB", "JS", "JNS", "JP", "JO", "RET"};

static const std::vector<std::string> FLOAT_BASIC
    {"ADDSS", "ADDSD", "SUBSS", "SUBSD", "SQRTSS", "ADDPS", "SUBPS", "MULPS", "DIVPS", "CVTSI2SS", "CVTTSS2SI", "COMISS", "UCOMISS", "PXOR", "PAND", "POR"};

static const std::vector<std::string> FLOAT_MUL
    {"MULSS", "MULSD"};

static const std::vector<std::string> FLOAT_DIV
    {"DIVSS", "DIVSD"};

static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

static INSTRUCTION_TYPE IdentifyMOVType(const std::vector<std::string>& tokens) {
    if (tokens.size() > 1 && tokens[1].front() == '[') return INSTRUCTION_TYPE::STORE;
    if (tokens.size() > 2 && tokens[2].front() == '[') return INSTRUCTION_TYPE::LOAD;
    return INSTRUCTION_TYPE::INT_BASIC;
}

// Resolve um operando genérico do x86:
// - Registrador direto ("EBX") -> lookup normal (aborta se nome inválido).
// - Memória ("[RBX+4]" ou "[RAX+RBX*4+8]") -> usa apenas a base como fonte.
// - Imediato ("5", "0x10", "-4") -> não vira fonte (retorna false).
static bool LookupOperand(
    Register&          out,
    const std::string& token,
    const std::string& context
){
    std::string name{token};
    if (!name.empty() && name.front() == '[') {
        name = name.substr(1, name.size() - 2);         // remove colchetes
        size_t cut = name.find_first_of("+*");          // mantém só a base
        if (cut != std::string::npos) name = name.substr(0, cut);
    } else if (name.empty() || !std::isalpha(static_cast<unsigned char>(name[0]))) {
        return false;
    }
    out = LookupRegister(name, context, RegisterTable());
    return true;
}

// Monta o CDB com os registradores físicos:
CDB InstructionX86Intel::MakeCDB() {
    // Aliases compartilham o mesmo id:
    // - ids 0-15:  'L' = 'R' = 'W' = 'B'.
    // - ids 64-79: 'V'.
    // - id 80:     'G'.
    CDB cdb;
    cdb.registers.resize(81);
    fillCDB(cdb, 'L', 0, 16);  // Faixas de int: L, R, W, B compartilham os slots 0-15.
    fillCDB(cdb, 'R', 0, 16);
    fillCDB(cdb, 'W', 0, 16);
    fillCDB(cdb, 'B', 0, 16);
    fillCDB(cdb, 'V', 64, 16); // Faixa de vetoriais (uso float, double e int).
    fillCDB(cdb, 'G', 80, 1);  // Flags.
    cdb.print_banks = {{'R', 0, 16}, {'L', 0, 16}, {'V', 64, 16}, {'W', 0, 16}, {'B', 0, 16}, {'G', 80, 1}};
    return cdb;
}

// Tabela: (classe, id físico global).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Aliases compartilham o mesmo id:
    // - ids 0-15:  RAX..R15 ('L') = EAX..R15D ('R') = AX..R15W ('W') = AL/AH..R15B ('B').
    // - ids 64-79: XMM0-15 ('V').
    // - id 80:     EFLAGS ('G').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-15):
        // 64-bit (L, 0-15):
        const char* l64[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
        for (int i = 0; i < 8;  i++){
            t.emplace(l64[i], Register('L', i));
            t.emplace("R" + std::to_string(8 + i), Register('L', 8 + i));
        }
        // 32-bit (R, 0-15):
        const char* r32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"};
        for (int i = 0; i < 8;  i++){
            t.emplace(r32[i], Register('R', i));
            t.emplace("R" + std::to_string(8 + i) + "D", Register('R', 8 + i));
        }
        // 16-bit (W, 0-15):
        const char* w16[] = {"AX", "BX", "CX", "DX", "SI", "DI", "SP", "BP"};
        for (int i = 0; i < 8;  i++){
            t.emplace(w16[i], Register('W', i));
            t.emplace("R" + std::to_string(8 + i) + "W", Register('W', 8 + i));
        }
        // 8-bit (B, 0-15):
        // - AL/AH, BL/BH, ... compartilham o id do grupo.
        const char* b8[] = {"AL", "AH", "BL", "BH", "CL", "CH", "DL", "DH"};
        for (int i = 0; i < 8; i++) t.emplace(b8[i], Register('B', i / 2));
        t.emplace("SIL", Register('B', 4));
        t.emplace("DIL", Register('B', 5));
        t.emplace("SPL", Register('B', 6));
        t.emplace("BPL", Register('B', 7));
        for (int i = 8; i < 16; i++) t.emplace("R" + std::to_string(i) + "B", Register('B', i));

        // Vetorial (64-79):
        for (int i = 0; i < 16; i++) t.emplace("XMM" + std::to_string(i), Register('V', 64 + i));

        // Flags (80):
        t.emplace("EFLAGS", Register('G', 80));
    }

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
    const std::vector<std::string>& tokens
){
    std::string op{tokens[0]};
    for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (op == MOV)                      type = IdentifyMOVType(tokens);
    else if (Contains(LOADS, op))       type = INSTRUCTION_TYPE::LOAD;
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
        // Desvios condicionais leem EFLAGS.
        source_registers.push_back(Register('G', 80));
    } else if (type == INSTRUCTION_TYPE::LOAD) {
        // MOV reg, [mem].
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        Register base;
        if (tokens.size() > 2 && LookupOperand(base, tokens[2], instruction_string))
            source_registers.push_back(base);
    } else if (type == INSTRUCTION_TYPE::STORE) {
        // MOV [mem], reg.
        if (tokens.size() > 2)
            source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
        Register base;
        if (LookupOperand(base, tokens[1], instruction_string))
            source_registers.push_back(base);
    } else if (tokens.size() > 1) {
        dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
        // Operações aritméticas alteram EFLAGS implicitamente e usam o destino como fonte inicial
        dest_registers.push_back(Register('G', 80));
        source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));

        if (tokens.size() > 2) {
            Register op2;
            if (LookupOperand(op2, tokens[2], instruction_string))
                source_registers.push_back(op2);
        }
    }
}

} // namespace processor
