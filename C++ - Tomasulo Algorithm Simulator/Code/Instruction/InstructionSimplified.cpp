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

// Função de Instruction.h:
// - Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // - ids 0-31:  R0..31 ('R').
    // - ids 32-63: F0..31 ('F').
    static std::unordered_map<std::string, Register> t;

    // Como não existem aliases de hardware, todos os registradores recebem a mascara default (0xFF - Integral).
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

// Monta o CDB com os registradores físicos:
CDB InstructionSimplified::MakeCDB() {
    // - ids 0-31:  'R'.
    // - ids 32-63: 'F'.
    CDB cdb;
    // Como não existem aliases de hardware, todos os registradores recebem a mascara default (0xFF - Integral).
    FillCDB(cdb, 'R', 0,  32); // Faixa de int.
    FillCDB(cdb, 'F', 32, 32); // Faixa de float.

    cdb.print_banks = {{'R', 0, 32}, {'F', 32, 32}};
    return cdb;
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
// Separa a instrução nos seus componentes (OpCode + reg/label/imediato ...).
std::vector<std::string> InstructionSimplified::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : str) {
        if (c == ',' || c == ' ' || c == '(' || c == ')' || c == '\t') {
            // Ignora os espaços repetidos.
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    // Evita perda de informação do último caracter (quando termina, faz só o piece += c).
    // - Verifica se está vazio, pois pode ser enviado algo como "add r0 r1 r2 " (espaço no final).
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
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i], RegisterTable())) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // Proteção contra linha truncada (ex.: "ld F4, 0" sem a base).
        if (tokens.size() < 4) {
            std::cerr << "[ERRO] Instrução incompleta:\n"  <<
            "Instrução: "<< tokens[0] << " ...\n";
            std::abort();
        }
        normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";
    } else if (type == INSTRUCTION_TYPE::BRANCH) {
        for (size_t i = 1; i < tokens.size(); ++i)
            normalized += (i == 1 ? "" : ", ") + tokens[i];
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

    // Instruções de 4+ tokens:
    if(tokens.size() > 3){
        // LOAD ("l.d f2, 0(r1)"):
        // - Fonte   = tokens[3]
        // - Destino = tokens[1];
        // - tokens[2] é o deslocamento (sempre imediato) - ignorado.
        if (type == INSTRUCTION_TYPE::LOAD) {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
        // STORE ("s.d f2, 0(r1)"):
        // - Fontes = tokens[1] e tokens[3];
        // Sem destino.
        // - tokens[2] é o deslocamento (sempre imediato) - ignorado.
        else if (type == INSTRUCTION_TYPE::STORE) {
            source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
        // "beq r1, r2, LOOP":
        // - Fonte = tokens[1] e tokens[2] ;
        // Sem destino.
        // - tokens[3] é o sempre o label - ignorado.
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            return;
        }
        // Aritmética ("add r1, r2, r3"):
        // Fontes  = Todos os registradores a partir de tokens[2];
        // Destino = tokens[1];
        else {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            if (IsRegister(tokens[3], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
    }
    // Instruções de 2-3 tokens:
    else if (tokens.size() > 1){
        // BRANCH ("bnez r3, LOOP" / "j LOOP" / "jr r3"):
        // - Fonte = tokens[1] (as vezes);
        // Sem destino.
        // - tokens[2] (se existe) é o sempre o label - ignorado.
        if (type == INSTRUCTION_TYPE::BRANCH) {
            if (tokens.size() > 1 && IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
    }
    // Não atendeu nenhum caso (sem return anterior).
    std::cerr << "[ERRO] Instrução não suportada:\n"
    "Instrução: "<< instruction_string << '\n';
    std::abort();
}

// Verifica se os destinos e fontes correspondem à sintaxe da linguagem.
// - Aborta em caso contrário, sem possibilidade de escrita incorreta.
void InstructionSimplified::ValidateInstruction(
    const std::vector<std::string>& tokens,
    const std::vector<int>&         expected_dests,
    const std::vector<int>&         expected_srcs

){

}

} // namespace processor
