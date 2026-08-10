/* Instruction/InstructionMips32.cpp */
#include "headers/InstructionMips32.h"

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

        // Int (0 - 31):
        // - $zero/$0 (0).
        t.emplace("$zero", Register('R', 0));
        t.emplace("$0", Register('R', 0));
        // - $at (1).
        t.emplace("$at", Register('R', 1));
        // - $v0..1 (2-3).
        for (int i = 0; i < 2; i++)  t.emplace("$v" + std::to_string(i), Register('R', 2 + i));
        // - $a0..3 (4-7).
        for (int i = 0; i < 4; i++)  t.emplace("$a" + std::to_string(i), Register('R', 4 + i));
        // - $t0..7 (8-15).
        for (int i = 0; i < 8; i++)  t.emplace("$t" + std::to_string(i), Register('R', 8 + i));
        // - $s0..7 (16-23).
        for (int i = 0; i < 8; i++)  t.emplace("$s" + std::to_string(i), Register('R', 16 + i));
        // - $t8..9 (24-25).
        for (int i = 0; i < 2; i++)  t.emplace("$t" + std::to_string(8 + i), Register('R', 24 + i));
        // - $k0..1 (26-27).
        for (int i = 0; i < 2; i++)  t.emplace("$k" + std::to_string(i), Register('R', 26 + i));
        // - $gp (28).
        t.emplace("$gp", Register('R', 28));
        // - $sp (29).
        t.emplace("$sp", Register('R', 29));
        // - $fp (30).
        t.emplace("$fp", Register('R', 30));
        // - $ra (31).
        t.emplace("$ra", Register('R', 31));
        // - $0..31 numéricos (0-31).
        for (int i = 1; i < 32; i++) t.emplace("$" + std::to_string(i), Register('R', i));

        // Float (32 - 63):
        for (int i{}; i < 32; i++) t.emplace("$f" + std::to_string(i), Register('F', 32 + i));
    }
    return t;
}

// Monta o CDB com os registradores físicos:
CDB InstructionMips32::MakeCDB() {
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
InstructionMips32::InstructionMips32(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool InstructionMips32::IdentifyType(
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
std::vector<std::string> InstructionMips32::SplitInstruction(
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
    // - Verifica se está vazio, pois pode ser enviado algo como "add $t0 $t1 $t2 " (espaço no final).
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

// Privado:
// Padroniza os opcodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionMips32::NormalizeInstruction(
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
        // Proteção contra linha truncada (ex.: "lw $t0, 4" sem a base).
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
void InstructionMips32::SetAttributes(
    const std::vector<std::string>& tokens
){
    dest_registers.clear();
    source_registers.clear();

    // Instruções de 4 tokens:
    if (tokens.size() > 3) {
        // "lw $t0, 4($t1)":
        // - Destino = tokens[1];
        // - Fonte   = tokens[3];
        // - tokens[2] é o deslocamento (sempre imediato) - ignorado.
        if (type == INSTRUCTION_TYPE::LOAD) {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
        // "sw $t0, 4($t1)":
        // - Fontes = tokens[1] e tokens[3];
        // - Sem destino.
        // - tokens[2] é o deslocamento (sempre imediato) - ignorado.
        else if (type == INSTRUCTION_TYPE::STORE) {
            source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
        // "beq $t0, $t1, LOOP" / "bne $t0, $t1, LOOP":
        // - Fontes = tokens[1] e tokens[2]
        // - Sem destino.
        // - tokens[3] é o sempre o label - ignorado.
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            return;
        }
        // Aritmética - "add $t0, $t1, $t2" / "addi $t0, $t1, 5":
        // - Fontes  = tokens[2] e tokens[3] (imediatos são ignorados por IsRegister);
        // - Destino = tokens[1];
        else {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            if (IsRegister(tokens[3], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[3], instruction_string, RegisterTable()));
            return;
        }
    }
    // Instruções de 3 tokens:
    else if (tokens.size() > 2) {
        // "jalr $ra, $t0" (forma de 2 operandos: rd explícito):
        // - Fonte   = tokens[2];
        // - Destino = tokens[1];
        if (tokens[0] == "jalr") {
            if (IsRegister(tokens[1], RegisterTable()) && IsRegister(tokens[2], RegisterTable())) {
                dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            }
            return;
        }
        // "bltzal $t0, LOOP" / "bgezal $t0, LOOP":
        // - Fonte   - tokens[1];
        // - Destino - $ra.
        else if (tokens[0] == "bltzal" || tokens[0] == "bgezal") {
            dest_registers.push_back(Register('R', 31));
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "bnez $t0, LOOP" (e beqz/bgtz/bltz/bgez/blez):
        // - Fonte = tokens[1].
        // - Sem destino.
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "lui $t0, 5" / "abs.s $f0, $f1" (dest + imediato ou dest + fonte única):
        // - Fonte   = tokens[2], se for registrador (lui não é; abs.s/neg.s/sqrt.s/cvt.* são).
        // - Destino = tokens[1];
        else {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            return;
        }
    }
    // Instruções de 2 tokens:
    else if (tokens.size() > 1) {
        // "jal FUNC":
        // - Destino = $ra.
        // Sem fonte.
        if (tokens[0] == "jal") {
            dest_registers.push_back(Register('R', 31));
            return;
        }
        // "jalr $t0" (forma de 1 operando: retorno implícito em $ra):
        // - Fonte   = tokens[1];
        // - Destino = $ra;
        else if (tokens[0] == "jalr") {
            if (IsRegister(tokens[1], RegisterTable())) {
                dest_registers.push_back(Register('R', 31));
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            }
            return;
        }
        // "jr $ra":
        // - Fonte = tokens[1];
        // - Sem destino.
        else if (tokens[0] == "jr") {
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "j LOOP":
        // - Não lê nem escreve registrador.
        else if (tokens[0] == "j") {
            return;
        }
    }
    // Não atendeu nenhum caso (sem return anterior).
    std::cerr << "[ERRO] Instrução incompleta:\n"
    "Instrução: "<< instruction_string << '\n';
    std::abort();
}

// Verifica se os destinos e fontes correspondem à sintaxe da linguagem.
// - Aborta em caso contrário, sem possibilidade de escrita incorreta.
void InstructionMips32::ValidateInstruction(
    const std::vector<std::string>& tokens,
    const std::vector<int>&         expected_dests,
    const std::vector<int>&         expected_srcs

){

}

} // namespace processor
