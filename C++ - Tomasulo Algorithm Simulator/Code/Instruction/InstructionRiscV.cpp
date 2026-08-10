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
    {"add", "addi", "sub", "and", "andi", "or", "ori", "xor", "xori", "sll", "srl", "sra", "slli", "srli", "srai", "slt", "sltu", "slti", "sltiu", "lui", "auipc", "li", "mv", "nop"};

static const std::vector<std::string> INT_MUL
    {"mul", "mulh", "mulhu"};

static const std::vector<std::string> INT_DIV
    {"div", "divu", "rem", "remu"};

// Pseudo-instruções de controle de fluxo inclusas (expansões reais):
// - j = jal x0, offset; ret = jalr x0, 0(x1); call = auipc ra, hi + jalr ra, lo(ra).
static const std::vector<std::string> BRANCHES
    {"beq", "bne", "blt", "bge", "bltu", "bgeu", "jal", "jalr", "j", "ret", "call"};

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

// Função de Instruction.h:
// - Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // - ids 0-31:  x0..31 ('L').
    // - ids 32-63: f0..31 ('F').
    static std::unordered_map<std::string, Register> t;

    // Como não existem aliases de hardware, todos os registradores recebem a mascara default (0xFF - Integral).
    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).
        for (int i = 0; i < 32; i++){
            // Int (0-31):
            t.emplace("x" + std::to_string(i), Register('L', i));
            // Float (32-63):
            t.emplace("f" + std::to_string(i), Register('F', 32 + i));
        }

        // Apelidos ABI (assembly real mapeado ao registrador físico):
        t.emplace("zero", Register('L', 0));
        t.emplace("ra",   Register('L', 1));
        t.emplace("sp",   Register('L', 2));
        t.emplace("gp",   Register('L', 3));
        t.emplace("tp",   Register('L', 4));
        for (int i = 0; i < 3;  i++) t.emplace("t" + std::to_string(i), Register('L', 5 + i));   // t0-t2 (5-7).
        t.emplace("s0", Register('L', 8));
        t.emplace("fp", Register('L', 8));   // alias de s0.
        t.emplace("s1", Register('L', 9));
        for (int i = 0; i < 8;  i++) t.emplace("a" + std::to_string(i), Register('L', 10 + i));  // a0-a7 (10-17).
        for (int i = 2; i < 12; i++) t.emplace("s" + std::to_string(i), Register('L', 16 + i));  // s2-s11 (18-27).
        for (int i = 3; i < 7;  i++) t.emplace("t" + std::to_string(i), Register('L', 25 + i));  // t3-t6 (28-31).
    }
    return t;
}

// Monta o CDB com os registradores físicos:
CDB InstructionRiscV::MakeCDB() {
    // - ids 0-31:  'L'.
    // - ids 32-63: 'F'.
    CDB cdb;
    // Como não existem aliases de hardware, todos os registradores recebem a mascara default (0xFF - Integral).
    FillCDB(cdb, 'L', 0,  32);  // Faixa de int.
    FillCDB(cdb, 'F', 32, 32); // Faixa de float.

    cdb.print_banks = {{'L', 0, 32}, {'F', 32, 32}};
    return cdb;
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
// Separa a instrução nos seus componentes (OpCode + reg/label/imediato ...).
std::vector<std::string> InstructionRiscV::SplitInstruction(
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
    // - Verifica se está vazio, pois pode ser enviado algo como "add x0 x1 x2 " (espaço no final).
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
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i], RegisterTable())) continue;
        for (char& c : tokens[i]) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::string normalized = tokens[0];
    while (normalized.length() < biggest_instruction) normalized += ' ';

    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // Proteção contra linha truncada (ex.: "lw x5, 4" sem a base).
        if (tokens.size() < 4) {
            std::cerr << "[ERRO] Instrução incompleta:\n"  <<
            "Instrução: "<< tokens[0] << " ...\n";
            std::abort();
        }
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

    // Instruções de 4+ tokens:
    if (tokens.size() > 3) {
        // "lw x5, 4(x6)":
        // - Fontes  = qualquer token registrador a partir de tokens[2] (imediato é ignorado).
        // - Destino = tokens[1];
        if (type == INSTRUCTION_TYPE::LOAD) {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            for (size_t i = 2; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
            return;
        }
        // "sw x5, 4(x6)":
        // - Fontes = qualquer token registrador (dado + base);
        // Sem destino.
        else if (type == INSTRUCTION_TYPE::STORE) {
            for (size_t i = 1; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
            return;
        }
        // "jalr x1, 0(x5)" (forma completa: rd, imediato, rs1):
        // - Fonte   = tokens[3];
        // - Destino = tokens[1];
        // - tokens[2] é o deslocamento (sempre imediato) - ignorado.
        else if (tokens[0] == "jalr") {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            for (size_t i = 2; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
            return;
        }
        // "beq x5, x6, LOOP" (e bne/blt/bge/bltu/bgeu):
        // - Fontes = tokens[1] e tokens[2].
        // - Sem destino.
        // - tokens[3] é o deslocamento é sempre o label - ignorado.
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            for (size_t i = 1; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
            return;
        }
        // Aritmética - "add x5, x6, x7" / "addi x5, x6, 10" (e formas com mais fontes, como fmadd.s dest,src,src,src):
        // - Fontes  = qualquer token registrador a partir de tokens[2].
        // - Destino = tokens[1];
        else {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            for (size_t i = 2; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    source_registers.push_back(LookupRegister(tokens[i], instruction_string, RegisterTable()));
            return;
        }
    }
    // Instruções de 3 tokens:
    else if (tokens.size() > 2) {
        // "jal x1, func" (rd explícito):
        // - Destino = tokens[1];
        // Sem destino.
        if (tokens[0] == "jal") {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "mv x5, x6" (pseudo de "addi x5, x6, 0"):
        // - Fonte   = tokens[2];
        // - Destino = tokens[1];
        else if (tokens[0] == "mv") {
            dest_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            if (IsRegister(tokens[2], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[2], instruction_string, RegisterTable()));
            return;
        }
        // "li x5, 10" / "lui x5, 10" / "auipc x5, 10" (dest + imediato) ou "fsqrt.s f1, f2" / "fabs.s f1, f2" / "fcvt.s.w f1, x2" (dest + fonte única):
        // - Fonte   = tokens[2], se for registrador (li/lui/auipc não são; fsqrt.s/fabs.s/fneg.s/fcvt.*/fmv.* são).
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
        // "jal func"
        // - Destino = rd implícito (x1);
        // Sem fonte;
        if (tokens[0] == "jal") {
            dest_registers.push_back(Register('L', 1));
            return;
        }
        // "jalr x5" (pseudo: rd implícito = x1, offset implícito = 0):
        // - Fonte = tokens[1];
        // - Destino = x1;
        else if (tokens[0] == "jalr") {
            dest_registers.push_back(Register('L', 1));
            if (IsRegister(tokens[1], RegisterTable()))
                source_registers.push_back(LookupRegister(tokens[1], instruction_string, RegisterTable()));
            return;
        }
        // "call func":
        // - Destino = x1 (ra);
        // Sem fonte
        else if (tokens[0] == "call") {
            dest_registers.push_back(Register('L', 1));
            return;
        }
        // "j LOOP":
        // - Não lê nem escreve registrador.
        else if (tokens[0] == "j") {
            return;
        }
    }
    // Instruções de 1 token:
    else {
        // "ret" (pseudo de "jalr x0, 0(x1)"):
        // - Fonte = x1 (ra);
        // Sem destino.
        if (tokens[0] == "ret") {
            source_registers.push_back(Register('L', 1));
            return;
        }
        // "nop" (pseudo de "addi x0, x0, 0"):
        // - Não lê nem escreve registrador.
        else if (tokens[0] == "nop") {
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
void InstructionRiscV::ValidateInstruction(
    const std::vector<std::string>& tokens,
    const std::vector<int>&         expected_dests,
    const std::vector<int>&         expected_srcs

){

}

} // namespace processor
