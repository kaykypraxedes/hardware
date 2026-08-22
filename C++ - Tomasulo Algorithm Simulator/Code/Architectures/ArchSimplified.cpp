/* Architectures/ArchSimplified.cpp */
#include "headers/ArchSimplified.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header "Architecture.h"
 *
 * Dessa maneira, evita-se a redundância de comentários em cada
 * subclasse, visto que todos os métodos tem o mesmo resultado,
 * mudando apenas o método de implementação (consequência da sua
 * arquitetura e organização interna).
 */

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const size_t biggest_instruction{7};

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
    {"add.d", "add.s", "sub.d", "sub.s", "abs.s", "neg.s", "sqrt.s", "cvt.s.w", "cvt.d.w", "cvt.s.d", "cvt.d.s", "cvt.w.s", "cvt.w.d"};

static const std::vector<std::string> FLOAT_MUL
    {"mul.d", "mul.s"};

static const std::vector<std::string> FLOAT_DIV
    {"div.d", "div.s"};

// Função de Instruction.h.
const std::unordered_map<std::string, Register>& RegisterTable() {
    // - Tabela: (nome, registrador físico).
    // - ids 0-31:  r0..31  ('R').
    // - ids 32-63: f0..31  ('F').
    // - ids 64-65:  hi - lo ('M').

    static std::unordered_map<std::string, Register> t;

    // Como não existem máscaras de hardware, todos os registradores recebem default (0xFF - Integral).
    if (t.empty()) { // Evita refazer os emplaces a cada chamada da função (já que "t" é "static").
        for (int i = 0; i < 32; i++) {
            // Int (0-31):
            t.emplace("r" + std::to_string(i), Register('R', i));
            // Float (32-63):
            t.emplace("f" + std::to_string(i), Register('F', 32 + i));
        }
        // Aliase de hardware ("ra" = "r31").
        t.emplace("ra", Register('R', 31));
        // Registradores HI/LO (resultado de mult e div).
        t.emplace("hi", Register('M', 64));
        t.emplace("lo", Register('M', 65));

    }
    return t;
}

// ─── HELPERS ──────────────────────────────────────────────────────
static std::string ToLower(
    const std::string& token
){
    std::string lower{token};
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    return lower;
}

static bool IsIntReg(
    const std::string& token
){
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};

    return (it != table.end() && it->second.GetType() == 'R');
}

static bool IsFloatReg(
    const std::string& token
){
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};

    return (it != table.end() && it->second.GetType() == 'F');
}

static bool IsImmediate(
    const std::string& token,
    const bool         is_signed
){
    if (token.empty()) return false;

    // Obrigatório para todo imediato na sintaxe simplificada do livro ("signed" e "unsigned").
    if(token[0] != '#') return false;

    std::size_t i{1};

    // Número pode ser negativo ou positivo.
    if (token[1] == '-'){
        if(!is_signed) return false; // Não pode ser negativo.
        i = 2;
    }

    // Enviado apenas o '#'.
    if (i >= token.size()) return false;

    for (; i < token.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;

    return true;
}

// Deslocamento de load/store ("IMM(IREG)" do livro): número puro, SEM '#'.
// Offset é sempre com sinal (base+offset permite deslocamento negativo).
static bool IsOffset(
    const std::string& token
){
    if (token.empty()) return false;

    std::size_t i{0};

    // Deslocamento pode ser negativo.
    if (token[0] == '-'){
        i = 1;
    }

    // Enviado apenas o '-'.
    if (i >= token.size()) return false;

    for (; i < token.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;

    return true;
}

static bool IsLabel(
    const std::string& token
){
    if (token.empty()) return false;
    if (IsIntReg(token) || IsFloatReg(token)) return false;
    if (IsImmediate(token, true)) return false;
    if (IsImmediate(token, false)) return false;

    // Deve começar com letra ou "_" (identificador válido).
    if (!std::isalpha(static_cast<unsigned char>(token[0])) && token[0] != '_') return false;
    for (const char c : token)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;

    return true;
}

static const Register& LookupReg(
    const std::string& token
){
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};

    // Aborta em caso de inconsistência (registrador não validado na tabela).
    // - Não deveria ocorrer após "ValidateInstruction()".
    if (it == table.end()) {
        std::cerr << "[ERRO] Registrador não encontrado na tabela: " << token << '\n';
        std::abort();
    }

    return it->second;
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
// Público:
CDB InstructionSimplified::MakeCDB() {
    // Monta o CDB com os registradores físicos:
    // - ids 0-31:  'R'.
    // - ids 32-63: 'F'.
    CDB cdb;
    // Como não existem aliases de hardware, todos os registradores recebem a mascara default (0xFF - Integral).
    FillCDB(cdb, 'R', 0,  32); // Faixa de int.
    FillCDB(cdb, 'F', 32, 32); // Faixa de float.
    FillCDB(cdb, 'M', 64, 2);  // HI/LO
    cdb.print_banks = {{'R', 0, 32}, {'F', 32, 32}, {'M', 64, 2}};
    return cdb;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
InstructionSimplified::InstructionSimplified(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
std::vector<std::string> InstructionSimplified::SplitInstruction(
    const std::string& str
) const {
    std::vector<std::string> tokens;
    std::string current;

    // Delimitadores:
    // - espaço;
    // - tab;
    // - vírgula
    // - parênteses (usados no endereçamento "imediato(registrador)" de load/store).

    // - O ponto ('.') NÃO é delimitador: faz parte do opcode.
    // - ex: "add.d", "l.s", "cvt.w.d", etc.
    for (const char c : str) {
        const bool is_delimiter{c == ' ' || c == '\t' || c == ',' || c == '(' || c == ')'};
        if (is_delimiter) {
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
bool InstructionSimplified::IdentifyType(
    const std::vector<std::string>& tokens
){
    const std::string op{ToLower(tokens[0])};

    if      (ContainsOpcode(LOADS,       op)) type = INSTRUCTION_TYPE::LOAD;
    else if (ContainsOpcode(STORES,      op)) type = INSTRUCTION_TYPE::STORE;
    else if (ContainsOpcode(BRANCHES,    op)) type = INSTRUCTION_TYPE::BRANCH;
    else if (ContainsOpcode(INT_BASIC,   op)) type = INSTRUCTION_TYPE::INT_BASIC;
    else if (ContainsOpcode(INT_MUL,     op)) type = INSTRUCTION_TYPE::INT_MUL;
    else if (ContainsOpcode(INT_DIV,     op)) type = INSTRUCTION_TYPE::INT_DIV;
    else if (ContainsOpcode(FLOAT_BASIC, op)) type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (ContainsOpcode(FLOAT_MUL,   op)) type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (ContainsOpcode(FLOAT_DIV,   op)) type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    return true;
}

// Privado:
void InstructionSimplified::ValidateInstruction(
    const std::vector<std::string>& tokens
){
    const std::string op{ToLower(tokens[0])};

    switch (type) {
        case INSTRUCTION_TYPE::LOAD: {
            if (op == "load") {
                if (tokens.size() == 4 &&
                    (IsIntReg(tokens[1]) || IsFloatReg(tokens[1])) &&
                    IsOffset(tokens[2]) &&
                    IsIntReg(tokens[3]))

                    return;
            }
            else if (op == "lw") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "lb") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "lh") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "lbu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "lhu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "ld") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "lwu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "ll") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "l.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "l.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            // ── rt, imm(rs) — valor inteiro ─────────────────────────
            if (op == "sw") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sb") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sh") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sd") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sc") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }

            // ── ft, imm(rs) — valor float ───────────────────────────
            else if (op == "s.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "s.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }

            // ── rt/ft, imm(rs) — genérico (valor inteiro OU float) ──
            else if (op == "store") {
                if (tokens.size() == 4 && (IsIntReg(tokens[1]) || IsFloatReg(tokens[1])) && IsOffset(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            // ── rs, rt, label ────────────────────────────────────────
            if (op == "beq") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsLabel(tokens[3]))
                    return;
            }
            else if (op == "bne") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsLabel(tokens[3]))
                    return;
            }

            // ── rs, label (comparação implícita com zero) ────────────
            else if (op == "bnez") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "beqz") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "bgtz") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "bltz") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "bgez") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "blez") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "bltzal") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }
            else if (op == "bgezal") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]))
                    return;
            }

            // ── label (sem registrador) ───────────────────────────────
            else if (op == "j") {
                if (tokens.size() == 2 && IsLabel(tokens[1]))
                    return;
            }
            else if (op == "jal") {
                if (tokens.size() == 2 && IsLabel(tokens[1]))
                    return;
            }

            // ── rs (sem label) ─────────────────────────────────────────
            else if (op == "jr") {
                if (tokens.size() == 2 && IsIntReg(tokens[1]))
                    return;
            }

            // ── jalr: duas sintaxes válidas — (rs) ou (rd, rs) ─────────
            else if (op == "jalr") {
                if (tokens.size() == 2 && IsIntReg(tokens[1]))
                    return;
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            // ── rd, rs, rt (3 registradores inteiros) ──────────────
            if (op == "add") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "addu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "daddu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sub") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "subu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "dsubu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "and") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "or") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "xor") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "nor") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "slt") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sltu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "dslt") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "dsltu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "sllv") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "srlv") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            else if (op == "srav") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }

            // ── rt/rd, rs, imm (2 registradores inteiros + imediato) ──
            else if (op == "addi") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "addiu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "daddiu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "subi") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "slti") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "sltiu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "dslti") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "dsltiu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true))
                    return;
            }
            else if (op == "andi") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "ori") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "xori") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }

            // ── rd, rt, shamt (shift por valor imediato) ───────────
            if (op == "sll") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "srl") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "sra") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "dsll") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "dsrl") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }
            else if (op == "dsra") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false))
                    return;
            }

            // ── rt, imm (sem rs) ────────────────────────────────────
            else if (op == "lui") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsImmediate(tokens[2], false))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL: {
            // ── rs, rt (resultado implícito em HI/LO — não modelado) ──
            if (op == "mult") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "multu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "dmult") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "dmultu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }

            // ── rd, rs, rt (resultado direto no registrador) ────────
            else if (op == "mul") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_DIV: {
            // ── rs, rt (quociente/resto implícito em HI/LO — não modelado) ──
            if (op == "div") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "divu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "ddiv") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            else if (op == "ddivu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC: {
            // ── fd, fs, ft (3 registradores float) ──────────────────
            if (op == "add.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            else if (op == "add.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            else if (op == "sub.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            else if (op == "sub.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }

            // ── fd, fs (unário / conversão) ──────────────────────────
            if (op == "abs.s") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "neg.s") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "sqrt.s") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.s.w") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.d.w") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.s.d") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.d.s") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.w.s") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            else if (op == "cvt.w.d") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_MUL: {
            if (op == "mul.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            else if (op == "mul.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            if (op == "div.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            else if (op == "div.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]))
                    return;
            }
            break;
        }
        default: // Demais casos (inválidos).
            break;
    }

    // Nenhum dos testes acima retornou:
    // - Sintaxe inválida para o opcode.
    std::cerr << "[ERRO] Sintaxe inválida para a instrução:\n- Instrução: ";
    for (size_t i{}; i < tokens.size(); i++) std::cerr << tokens[i] << ' ';
    std::cerr << '\n';
    std::abort();
}

// Privado:
void InstructionSimplified::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    // Padroniza o opcode para minúsculo.
    // - registradores são resolvidos "case-insensitive" na hora do lookup;
    // - labels preservam a caixa original por serem identificadores escritos pelo usuário.
    tokens[0] = ToLower(tokens[0]);

    // Guarda a forma canônica da instrução (debug/impressão).
    instruction_string = tokens[0];

    // Tabula o começo dos registradores (facilita o debugging na tabela)
    for (size_t i{tokens[0].size()}; i < biggest_instruction; i++){
        instruction_string += " ";
    }

    // Formata a instrução com base no seu tipo
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // Proteção contra linha truncada (ex.: "ld F4, 0" sem a base).
        if (tokens.size() < 4) {
            std::cerr << "[ERRO] Instrução incompleta:\n"  <<
            "Instrução: "<< tokens[0] << " ...\n";
            std::abort();
        }
        instruction_string += ToLower(tokens[1]) + ", " + tokens[2] + "(" + ToLower(tokens[3]) + ")";
    } else {
        for (size_t i{1}; i < tokens.size(); i++)
            instruction_string += (i == 1 ? "" : ", ") + (IsLabel(tokens[i]) ? tokens[i] : ToLower(tokens[i]));
    }
}

// Privado:
void InstructionSimplified::SetAttributes(
    const std::vector<std::string>& tokens
){
    // tokens[0] já normalizado (minúsculo) por "NormalizeInstruction()".
    const std::string& op{tokens[0]};

    switch (type) {
        case INSTRUCTION_TYPE::LOAD: {
            // Endereço (rs, base) é necessário em EX (cálculo do endereço).
            // Destino é sempre "dest_registers" (int OU float, conforme o opcode).
            if (op == "lw") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "lb") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "lh") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "lbu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "lhu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "ld") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "lwu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "ll") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "l.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "l.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "load") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            // Endereço (rs, base) é necessário em EX (cálculo do endereço).
            // Valor armazenado só precisa estar pronto em MEM (momento da escrita) -> mem_source_registers.
            // Store não escreve em nenhum registrador (sem dest_registers).
            if (op == "sw") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "sb") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "sh") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "sd") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "sc") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "s.d") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "s.s") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "store") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            if (op == "beq") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "bne") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "bnez") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "beqz") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "bgtz") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "bltz") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "bgez") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "blez") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "bltzal") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
                return;
            }
            else if (op == "bgezal") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                dest_registers.push_back(LookupReg("ra"));
                return;
            }
            else if (op == "j") {
                return; // Sem registradores envolvidos.
            }
            else if (op == "jal") {
                dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
                return;
            }
            else if (op == "jr") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            else if (op == "jalr") {
                if (tokens.size() == 2) {
                    // rs apenas -> rd = "ra" implícito.
                    ex_source_registers.push_back(LookupReg(tokens[1]));
                    dest_registers.push_back(LookupReg("ra"));
                    return;
                }
                // rd, rs explícitos.
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            // ── rd, rs, rt (3 registradores inteiros) ──────────────
            if (op == "add") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "addu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "daddu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "sub") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "subu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "dsubu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "and") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "or") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "xor") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "nor") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "slt") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "sltu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "dslt") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "dsltu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "sllv") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "srlv") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "srav") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }

            // ── rt/rd, rs, imm (imm não é registrador -> não gera dependência) ──
            else if (op == "addi") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "addiu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "daddiu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "subi") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "andi") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "ori") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "xori") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "slti") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "sltiu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dslti") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dsltiu") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }

            // ── rd, rt, shamt (shift por valor imediato) ───────────
            else if (op == "sll") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "srl") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "sra") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dsll") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dsrl") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dsra") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }

            // ── rt, imm (sem rs -> nenhum registrador fonte) ───────
            else if (op == "lui") {
                dest_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL: {
            // HI/LO não são modelados nesta arquitetura (sem registrador
            // especial) -> resultado de mult/multu/dmult/dmultu não gera
            // "dest_registers" rastreável.
            if (op == "mult") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "multu") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dmult") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "dmultu") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "mul") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_DIV: {
            // Mesma observação do INT_MUL: quociente/resto ficam em HI/LO,
            // não modelados -> sem "dest_registers".
            if (op == "div") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "divu") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "ddiv") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "ddivu") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC: {
            if (op == "add.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "add.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "sub.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "sub.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "abs.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "neg.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "sqrt.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.s.w") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.d.w") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.s.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.d.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.w.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            else if (op == "cvt.w.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_MUL: {
            if (op == "mul.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "mul.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            if (op == "div.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            else if (op == "div.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        default: // Casos inválidos (não deveria chegar aqui).
            break;
    }

    // Não deveria chegar aqui: "ValidateInstruction()" já garantiu
    // que o opcode pertence a um dos casos tratados acima.
    std::cerr << "[ERRO] Opcode validado mas não tratado em SetAttributes(): " << op << '\n';
    std::abort();
}

} // namespace processor
