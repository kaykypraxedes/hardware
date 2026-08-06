/* Instruction/InstructionX86Intel.cpp */
#include "headers/InstructionX86Intel.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int biggest_instruction{9};

// Opcodes da arquitetura:
static const std::string LOAD // Exclusivo
    {"lea"};

static const std::vector<std::string> MOVS // Caso especial, já que pode agir como load, store e aritimético (com reg e com imediato).
    {"mov", "movss", "movsd", "movaps", "movups", "movq", "movd"};

static const std::vector<std::string> INT_BASIC
    {"add", "sub", "and", "or", "xor", "inc", "dec", "cmp", "shl", "shr", "not", "neg", "test", "rol", "ror", "sar", "sal", "sbb", "adc", "movsx", "movzx"};

static const std::vector<std::string> INT_MUL
    {"imul", "mul"};

static const std::vector<std::string> INT_DIV
    {"idiv", "div"};

static const std::vector<std::string> BRANCHES
    {"jmp", "je", "jne", "jg", "jge", "jl", "jle", "call", "jbe", "ja", "jae", "jb", "js", "jns", "jp", "jo", "ret"};

static const std::vector<std::string> FLOAT_BASIC
    {"addss", "addsd", "subss", "subsd", "sqrtss", "addps", "subps", "mulps", "divps", "cvtsi2ss", "cvttss2si", "comiss", "ucomiss", "pxor", "pand", "por"};

static const std::vector<std::string> FLOAT_MUL
    {"mulss", "mulsd"};

static const std::vector<std::string> FLOAT_DIV
    {"divss", "divsd"};

// Verifica se o opcode existe (está na tabela).
static bool Contains(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// Identificação do tipo de 'mov' (caso especial, já que ele pode ser LOAD, STORE ou um INT_BASIC)
static INSTRUCTION_TYPE IdentifyMOVType(const std::vector<std::string>& tokens) {
    if (tokens.size() > 1 && tokens[1].front() == '[') return INSTRUCTION_TYPE::STORE;
    if (tokens.size() > 2 && tokens[2].front() == '[') return INSTRUCTION_TYPE::LOAD;
    return INSTRUCTION_TYPE::INT_BASIC;
}

// Resolve um operando genérico do x86:
// - Registrador direto ("ebx") -> lookup normal (aborta se nome inválido).
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

// Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Aliases compartilham o mesmo id:
    // - ids 0-15:  RAX..R15 ('L') = EAX..R15D ('R') = AX..R15W ('W') = AL/AH..R15B ('B').
    // - ids 64-79: XMM0-15 ('V').
    // - id 80:     EFLAGS ('G').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-15):
        // 64-bit (L, 0-15).
        const char* l64[] = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
        for (int i = 0; i < 8;  i++){
            t.emplace(l64[i], Register('L', i));
            t.emplace("r" + std::to_string(8 + i), Register('L', 8 + i));
        }
        // 32-bit (R, 0-15).
        const char* r32[] = {"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"};
        for (int i = 0; i < 8;  i++){
            t.emplace(r32[i], Register('R', i));
            t.emplace("r" + std::to_string(8 + i) + "d", Register('R', 8 + i));
        }
        // 16-bit (W, 0-15).
        const char* w16[] = {"ax", "bx", "cx", "dx", "si", "di", "sp", "bp"};
        for (int i = 0; i < 8;  i++){
            t.emplace(w16[i], Register('W', i));
            t.emplace("r" + std::to_string(8 + i) + "w", Register('W', 8 + i));
        }
        // 8-bit (B, 0-15).
        // - AL/AH, BL/BH, ... compartilham o id do grupo.
        const char* b8[] = {"al", "ah", "bl", "bh", "cl", "ch", "dl", "dh"};
        for (int i = 0; i < 8; i++) t.emplace(b8[i], Register('B', i / 2));
        t.emplace("sil", Register('B', 4));
        t.emplace("dil", Register('B', 5));
        t.emplace("spl", Register('B', 6));
        t.emplace("bpl", Register('B', 7));
        for (int i = 8; i < 16; i++) t.emplace("r" + std::to_string(i) + "b", Register('B', i));

        // Vetorial (64-79):
        for (int i = 0; i < 16; i++) t.emplace("xmm" + std::to_string(i), Register('V', 64 + i));

        // Flags (80):
        t.emplace("eflags", Register('G', 80));
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
    for (char& c : op) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (op == LOAD)                     type = INSTRUCTION_TYPE::LOAD;
    else if (Contains(MOVS, op))        type = IdentifyMOVType(tokens);
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
    // Evita perda de informação do último caracter.
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os opcodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionX86Intel::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: operando de desvio nunca vira fonte.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

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
