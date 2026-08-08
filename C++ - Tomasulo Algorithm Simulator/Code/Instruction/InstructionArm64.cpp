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

// xzr/wzr são o zero arquitetural: nunca são sobrescritos.
// - Não criam dependências nem ocupam slot no CDB (mesma ideia dos imediatos).
static bool IsZeroRegister(
    const std::string& token
){
    std::string lower = token;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == "xzr" || lower == "wzr";
}

// Recebe o registrador alvo para procurar dependências de hardware:
// - x0 (64 bits) contém o w0 (32 bits de baixo).
// - Quando o x0 é usado, ele bloqueia w0 e vice-versa.
// - Mesmo id + máscaras sobrepostas = mesmo espaço de hardware (bloqueiam-se mutuamente).

// Mais simples que X86, porque, se dividem o mesmo hardware, necessariamente um bloqueia o outro.
static std::vector<Register> GetMaskedRegisters(
    const Register& target_reg
){
    std::vector<Register> blocked_regs;

    for (const auto& [name, reg] : RegisterTable()) {
        if (
            target_reg.GetType()   != reg.GetType() && // Tipo igual e id igual => o próprio registrador.
            target_reg.GetId()     == reg.GetId()   &&
            ((target_reg.GetMask() &  reg.GetMask()) != 0))
        {
            blocked_regs.push_back(reg);
        }
    }

    return blocked_regs;
}

// Privado:
// Adiciona o registrador e suas variantes mascaradas, sem duplicar slots:
// - ex: mov al, bl -> dests {al, ax, eax, rax} (al + os que compartilham hardware).
static void PushWithMasked(
    std::vector<Register>& regs,
    const Register&        reg
){
    for (const Register& candidate : regs) {
        if (candidate.GetType() == reg.GetType() && candidate.GetId() == reg.GetId() && candidate.GetMask() == reg.GetMask())
            return; // Já adicionado.
    }

    regs.push_back(reg);
    for (const Register& variant : GetMaskedRegisters(reg))
        regs.push_back(variant);
}

// Função de Instruction.h:
// - Tabela: (nome, registrador físico).
const std::unordered_map<std::string, Register>& RegisterTable() {
    // Aliases compartilham o mesmo id:
    // - ids 0-30:  x0-30 ('L') = w0-30 ('R').
    // - ids 32-63: d0-31 ('S') = s0-31 ('F').
    // - id 80:     cpsr ('G').
    static std::unordered_map<std::string, Register> t;

    if (t.empty()){ // Evita refazer os emplaces a cada chamada da função (já que t é static).

        // Int (0-30):
        for (int i = 0; i < 31; i++){
            t.emplace("x" + std::to_string(i), Register('L', i, 0xFF));
            t.emplace("w" + std::to_string(i), Register('R', i, 0x0F));
        }

        // Aliases arquiteturais (mesmo hardware dos slots acima):
        // - lr (link register) é o x30.
        // - sp (stack pointer) é o x31 (slot extra do CDB).
        // - xzr/wzr (zero register) ficam de fora (nunca são escritos, então não geram dependência).
        t.emplace("lr", Register('L', 30, 0xFF));
        t.emplace("sp", Register('L', 31, 0xFF));

        // Float (32-63):
        for (int i = 0; i < 32; i++) {
            t.emplace("d" + std::to_string(i), Register('S', 32 + i, 0xFF));
            t.emplace("s" + std::to_string(i), Register('F', 32 + i, 0x0F));
        }

        t.emplace("cpsr", Register('G', 80, 0xFF));
    }

    return t;
}

// Monta o CDB com os registradores físicos:
// - Layout por variante (um slot por (id, mask)), contíguo por variante:
// - ids 0-32:  Faixas int (L/R), máscaras 0xFF/0x0F (31 inclui o sp).
// - ids 32-64: Faixas int (S/F), máscaras 0xFF/0x0F.
// - id 80:     'G' (cspr), 0xFF.
CDB InstructionArm64::MakeCDB() {
    CDB cdb;
    // Layout contíguo por variante:
    FillCDB(cdb, 'L', 0,  32, 0xFF);
    FillCDB(cdb, 'R', 0,  31, 0x0F);
    FillCDB(cdb, 'S', 32, 32, 0xFF);
    FillCDB(cdb, 'F', 32, 32, 0x0F);
    FillCDB(cdb, 'G', 80, 1,  0xFF);

    cdb.print_banks = {
        {'L', 0,   32},
        {'R', 32,  31},
        {'S', 63,  32},
        {'F', 95,  32},
        {'G', 127, 1}
    };
    return cdb;
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
// Separa a instrução nos seus componentes (OpCode + reg/label/imediato ...).
std::vector<std::string> InstructionArm64::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;

    for (char c : str) {
        if (c == ',' || c == ' ' || c == '[' || c == ']' || c == '#' || c == '\t') {
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
// Padroniza os OpCodes e registradores em minúsculo, coloca as devidas vírgulas e realiza a tabulação.
void InstructionArm64::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Labels de desvio são case-sensitive: só opcode e registradores viram minúsculo.
        if (type == INSTRUCTION_TYPE::BRANCH && i > 0 && !IsRegister(tokens[i], RegisterTable())) continue;
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

    // Pares load/store ("ldp x29, x30, [sp, 16]" / "stp x29, x30, [sp, -16]!"):
    // - ldp: destinos = tokens[1] e tokens[2]; fonte = a base (última).
    // - stp: fontes = dados (tokens[1], tokens[2]) + a base (última).
    if (tokens[0] == "ldp" || tokens[0] == "stp") {
        if (type == INSTRUCTION_TYPE::LOAD) {
            PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushWithMasked(dest_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
        } else { // STORE
            PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
            PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
        }
        // Base: primeiro token após o par ("ldp x0, x1, [x2, #8]" -> x2).
        // - O offset imediato vem depois e não é registrador.
        if (tokens.size() > 3 && !IsZeroRegister(tokens[3]))
            PushWithMasked(source_registers, LookupRegister(tokens[3], instruction_string, RegisterTable()));
    }
    // Imediatos ("#5", "#0x10") e labels não viram fonte (só registradores).
    else if (type == INSTRUCTION_TYPE::LOAD) {
        PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2 && !IsZeroRegister(tokens[2]))
            PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
    }
    else if (type == INSTRUCTION_TYPE::STORE) {
        // Dado primeiro; a base fica por último (o gate de endereço usa Q.back()).
        if (!IsZeroRegister(tokens[1]))
            PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens.size() > 2 && !IsZeroRegister(tokens[2]))
            PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
    }
    else if (type == INSTRUCTION_TYPE::BRANCH) {
        // Desvios condicionais (b.eq, b.ne, b.lo, ...) leem CPSR.
        if (tokens[0].rfind("b.", 0) == 0) {
            source_registers.push_back(Register('G', 80));
        }
        // Operando registrador vira fonte, mas só nos desvios que têm registrador de verdade:
        // - cbz/cbnz/tbz/tbnz/br/blr.
        // Em b/bl o alvo é label, que nunca deve virar fonte.
        // - Evita falso positivo de labels numéricos como "X10".
        if (tokens[0] == "cbz" || tokens[0] == "cbnz" || tokens[0] == "tbz" || tokens[0] == "tbnz" || tokens[0] == "br" || tokens[0] == "blr") {
            for (size_t i = 1; i < tokens.size(); ++i)
                if (IsRegister(tokens[i], RegisterTable()))
                    PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
        }
        // bl/blr escrevem o link register (x30).
        if (tokens[0] == "bl" || tokens[0] == "blr") {
            dest_registers.push_back(Register('L', 30, 0xFF));
        }
        // ret lê o x30 (endereço de retorno).
        if (tokens[0] == "ret") {
            source_registers.push_back(Register('L', 30, 0xFF));
        }
    }
    else if (tokens[0] == "cmp" || tokens[0] == "cmn" || tokens[0] == "tst" || tokens[0] == "fcmp") {
        // Comparadores não escrevem registrador de dados: apenas CPSR.
        dest_registers.push_back(Register('G', 80));
        for (size_t i = 1; i < tokens.size(); ++i)
            if (IsRegister(tokens[i], RegisterTable()))
                PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
    } else {
        PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
        if (tokens[0].back() == 's')
            dest_registers.push_back(Register('G', 80)); // ADDS atualiza CPSR.
        for (size_t i = 2; i < tokens.size(); ++i)
            if (IsRegister(tokens[i], RegisterTable()))
                PushWithMasked(source_registers, LookupRegister(tokens[i], instruction_string, RegisterTable()));
    }
}

} // namespace processor
