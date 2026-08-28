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

// ─── ENUM ─────────────────────────────────────────────────────────

/**
 * @brief Agrupa as instruções por seu tipo de sintaxe (resumo de
 * código para estruturas de atribuição e verificação identicas).
 */
enum class OP_SHAPE {
    // ── LOAD ──────────────────────────────────────────────────────
    LOAD_INT,        // IREG,        OFF(IREG)   [lw,lb,lh,lbu,lhu,ld,lwu,ll]
    LOAD_FLOAT,      // FREG,        OFF(IREG)   [l.d,l.s]
    LOAD_ANY,        // IREG/FREG,   OFF(IREG)   [load]

    // ── STORE ─────────────────────────────────────────────────────
    STORE_INT,       // IREG,        OFF(IREG)   [sw,sb,sh,sd,sc]
    STORE_FLOAT,     // FREG,        OFF(IREG)   [s.d,s.s]
    STORE_ANY,       // IREG/FREG,   OFF(IREG)   [store]

    // ── BRANCH ────────────────────────────────────────────────────
    BR_2REG_LABEL,       // rs, rt, label               [beq,bne]
    BR_1REG_LABEL,       // rs, label                   [bnez,beqz,bgtz,bltz,bgez,blez]
    BR_1REG_LABEL_LINK,  // rs, label   (+ "ra" dest)   [bltzal,bgezal]
    BR_LABEL,            // label                       [j]
    BR_LABEL_LINK,       // label       (+ "ra" dest)   [jal]
    BR_1REG,             // rs                          [jr]
    BR_JALR,             // (rs) OU (rd, rs) — duas sintaxes  [jalr]

    // ── INT_BASIC ─────────────────────────────────────────────────
    INT_3REG,            // rd, rs, rt                  [add..dsltu, sllv/srlv/srav — 18 opcodes]
    INT_2REG_IMM_S,      // rt, rs, imm (com sinal)     [addi..dsltiu — 8 opcodes]
    INT_2REG_IMM_U,      // rt, rs, imm (sem sinal / shamt) [andi,ori,xori,sll,srl,sra,dsll,dsrl,dsra — 9 opcodes]
    INT_1REG_IMM,        // rt, imm  (sem rs)           [lui]
    INT_1REG_LO,         // rd  (fonte implícita LO)    [mflo]
    INT_1REG_HI,         // rd  (fonte implícita HI)    [mfhi]

    // ── INT_MUL / INT_DIV ─────────────────────────────────────────
    MULDIV_2REG_HILO,    // rs, rt  -> dest implícito HI/LO  [mult,multu,dmult,dmultu,div,divu,ddiv,ddivu]
    MUL_3REG,            // rd, rs, rt                  [mul]

    // ── FLOAT ─────────────────────────────────────────────────────
    FLOAT_3REG,          // fd, fs, ft                  [add.d,add.s,sub.d,sub.s,mul.d,mul.s,div.d,div.s]
    FLOAT_2REG,          // fd, fs (unário/conversão)   [abs.s,neg.s,sqrt.s,cvt.* — 9 opcodes]
};

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const size_t biggest_instruction{7};

// Opcodes da arquitetura:
static const std::vector<std::string> LOADS
    {"load", "lw", "lb", "lh", "lbu", "lhu", "l.d", "l.s", "ld", "lwu", "ll"};

static const std::vector<std::string> STORES
    {"store", "sw", "sb", "sh", "s.d", "s.s", "sd", "sc"};

static const std::vector<std::string> INT_BASIC
    {"add", "addi", "addu", "addiu", "daddu", "daddiu", "sub", "subi", "subu", "dsubu", "and", "andi", "or", "ori", "xor", "xori", "nor", "lui", "sll", "srl", "sra", "sllv", "srlv", "srav", "dsll", "dsrl", "dsra", "slt", "slti", "sltu", "sltiu", "dslt", "dslti", "dsltu", "dsltiu", "mflo", "mfhi"};

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

static const std::unordered_map<std::string, OP_SHAPE> OPCODE_SHAPE {
    // LOAD
    {"lw", OP_SHAPE::LOAD_INT},    {"lb", OP_SHAPE::LOAD_INT},  {"lh", OP_SHAPE::LOAD_INT},
    {"lbu", OP_SHAPE::LOAD_INT},   {"lhu", OP_SHAPE::LOAD_INT}, {"ld", OP_SHAPE::LOAD_INT},
    {"lwu", OP_SHAPE::LOAD_INT},   {"ll", OP_SHAPE::LOAD_INT},
    {"l.d", OP_SHAPE::LOAD_FLOAT}, {"l.s", OP_SHAPE::LOAD_FLOAT},
    {"load", OP_SHAPE::LOAD_ANY},

    // STORE
    {"sw", OP_SHAPE::STORE_INT},    {"sb", OP_SHAPE::STORE_INT}, {"sh", OP_SHAPE::STORE_INT},
    {"sd", OP_SHAPE::STORE_INT},    {"sc", OP_SHAPE::STORE_INT},
    {"s.d", OP_SHAPE::STORE_FLOAT}, {"s.s", OP_SHAPE::STORE_FLOAT},
    {"store", OP_SHAPE::STORE_ANY},

    // BRANCH
    {"beq", OP_SHAPE::BR_2REG_LABEL},  {"bne", OP_SHAPE::BR_2REG_LABEL},
    {"bnez", OP_SHAPE::BR_1REG_LABEL}, {"beqz", OP_SHAPE::BR_1REG_LABEL},
    {"bgtz", OP_SHAPE::BR_1REG_LABEL}, {"bltz", OP_SHAPE::BR_1REG_LABEL},
    {"bgez", OP_SHAPE::BR_1REG_LABEL}, {"blez", OP_SHAPE::BR_1REG_LABEL},
    {"bltzal", OP_SHAPE::BR_1REG_LABEL_LINK}, {"bgezal", OP_SHAPE::BR_1REG_LABEL_LINK},
    {"j", OP_SHAPE::BR_LABEL}, {"jal", OP_SHAPE::BR_LABEL_LINK},
    {"jr", OP_SHAPE::BR_1REG}, {"jalr", OP_SHAPE::BR_JALR},

    // INT_BASIC
    {"add", OP_SHAPE::INT_3REG},  {"addu", OP_SHAPE::INT_3REG}, {"daddu", OP_SHAPE::INT_3REG},
    {"sub", OP_SHAPE::INT_3REG},  {"subu", OP_SHAPE::INT_3REG}, {"dsubu", OP_SHAPE::INT_3REG},
    {"and", OP_SHAPE::INT_3REG},  {"or", OP_SHAPE::INT_3REG},   {"xor", OP_SHAPE::INT_3REG},
    {"nor", OP_SHAPE::INT_3REG},
    {"sllv", OP_SHAPE::INT_3REG}, {"srlv", OP_SHAPE::INT_3REG}, {"srav", OP_SHAPE::INT_3REG},
    {"slt", OP_SHAPE::INT_3REG},  {"sltu", OP_SHAPE::INT_3REG},
    {"dslt", OP_SHAPE::INT_3REG}, {"dsltu", OP_SHAPE::INT_3REG},

    {"addi", OP_SHAPE::INT_2REG_IMM_S},  {"addiu", OP_SHAPE::INT_2REG_IMM_S},
    {"daddiu", OP_SHAPE::INT_2REG_IMM_S}, {"subi", OP_SHAPE::INT_2REG_IMM_S},
    {"slti", OP_SHAPE::INT_2REG_IMM_S},   {"sltiu", OP_SHAPE::INT_2REG_IMM_S},
    {"dslti", OP_SHAPE::INT_2REG_IMM_S},  {"dsltiu", OP_SHAPE::INT_2REG_IMM_S},

    {"andi", OP_SHAPE::INT_2REG_IMM_U}, {"ori", OP_SHAPE::INT_2REG_IMM_U},  {"xori", OP_SHAPE::INT_2REG_IMM_U},
    {"sll", OP_SHAPE::INT_2REG_IMM_U},  {"srl", OP_SHAPE::INT_2REG_IMM_U},  {"sra", OP_SHAPE::INT_2REG_IMM_U},
    {"dsll", OP_SHAPE::INT_2REG_IMM_U}, {"dsrl", OP_SHAPE::INT_2REG_IMM_U}, {"dsra", OP_SHAPE::INT_2REG_IMM_U},

    {"lui", OP_SHAPE::INT_1REG_IMM},
    {"mflo", OP_SHAPE::INT_1REG_LO}, {"mfhi", OP_SHAPE::INT_1REG_HI},

    // INT_MUL / INT_DIV
    {"mult", OP_SHAPE::MULDIV_2REG_HILO},  {"multu", OP_SHAPE::MULDIV_2REG_HILO},
    {"dmult", OP_SHAPE::MULDIV_2REG_HILO}, {"dmultu", OP_SHAPE::MULDIV_2REG_HILO},
    {"div", OP_SHAPE::MULDIV_2REG_HILO},   {"divu", OP_SHAPE::MULDIV_2REG_HILO},
    {"ddiv", OP_SHAPE::MULDIV_2REG_HILO},  {"ddivu", OP_SHAPE::MULDIV_2REG_HILO},
    {"mul", OP_SHAPE::MUL_3REG},

    // FLOAT
    {"add.d", OP_SHAPE::FLOAT_3REG},   {"add.s", OP_SHAPE::FLOAT_3REG},
    {"sub.d", OP_SHAPE::FLOAT_3REG},   {"sub.s", OP_SHAPE::FLOAT_3REG},
    {"mul.d", OP_SHAPE::FLOAT_3REG},   {"mul.s", OP_SHAPE::FLOAT_3REG},
    {"div.d", OP_SHAPE::FLOAT_3REG},   {"div.s", OP_SHAPE::FLOAT_3REG},
    {"abs.s", OP_SHAPE::FLOAT_2REG},   {"neg.s", OP_SHAPE::FLOAT_2REG}, {"sqrt.s", OP_SHAPE::FLOAT_2REG},
    {"cvt.s.w", OP_SHAPE::FLOAT_2REG}, {"cvt.d.w", OP_SHAPE::FLOAT_2REG},
    {"cvt.s.d", OP_SHAPE::FLOAT_2REG}, {"cvt.d.s", OP_SHAPE::FLOAT_2REG},
    {"cvt.w.s", OP_SHAPE::FLOAT_2REG}, {"cvt.w.d", OP_SHAPE::FLOAT_2REG},
};

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
// Público:
CDB InstructionSimplified::MakeCDB() {
    // Monta o CDB com os registradores físicos:
    // - ids 0-31:  'R'.
    // - ids 32-63: 'F'.
    // - ids 64-65: 'M'.
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
bool InstructionSimplified::SetStages(
    const std::vector<std::string>& tokens
){
    const std::string op{ToLower(tokens[0])};
    INSTRUCTION_TYPE instruction_type{INSTRUCTION_TYPE::INVALID};

    if      (ContainsOpcode(LOADS,       op)) instruction_type = INSTRUCTION_TYPE::LOAD;
    else if (ContainsOpcode(STORES,      op)) instruction_type = INSTRUCTION_TYPE::STORE;
    else if (ContainsOpcode(BRANCHES,    op)) instruction_type = INSTRUCTION_TYPE::BRANCH;
    else if (ContainsOpcode(INT_BASIC,   op)) instruction_type = INSTRUCTION_TYPE::INT_BASIC;
    else if (ContainsOpcode(INT_MUL,     op)) instruction_type = INSTRUCTION_TYPE::INT_MUL;
    else if (ContainsOpcode(INT_DIV,     op)) instruction_type = INSTRUCTION_TYPE::INT_DIV;
    else if (ContainsOpcode(FLOAT_BASIC, op)) instruction_type = INSTRUCTION_TYPE::FLOAT_BASIC;
    else if (ContainsOpcode(FLOAT_MUL,   op)) instruction_type = INSTRUCTION_TYPE::FLOAT_MUL;
    else if (ContainsOpcode(FLOAT_DIV,   op)) instruction_type = INSTRUCTION_TYPE::FLOAT_DIV;
    else return false;

    ValidateInstruction(tokens);

    std::vector<Register> ex_sources;
    std::vector<Register> mem_sources;
    SetStageAttributes(tokens, ex_sources, mem_sources);
    AddStage(instruction_type, ex_sources, mem_sources);
    return true;
}

// Privado:
void InstructionSimplified::ValidateInstruction(
    const std::vector<std::string>& tokens
){
    const std::string op{ToLower(tokens[0])};

    // Não deveria falhar: SetStages() já garantiu que "op" pertence a uma categoria conhecida.
    const auto it{OPCODE_SHAPE.find(op)};
    if (it == OPCODE_SHAPE.end()) {
        std::cerr << "[ERRO] Opcode reconhecido em SetStages() mas ausente em OPCODE_SHAPE: " << op << '\n';
        std::abort();
    }

    bool ok{false};
    switch (it->second) {
        // Load/Store:
        case OP_SHAPE::LOAD_INT:
        case OP_SHAPE::STORE_INT:
            ok = tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]);
            break;
        case OP_SHAPE::LOAD_FLOAT:
        case OP_SHAPE::STORE_FLOAT:
            ok = tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3]);
            break;
        case OP_SHAPE::LOAD_ANY:
        case OP_SHAPE::STORE_ANY:
            ok = tokens.size() == 4 && (IsIntReg(tokens[1]) || IsFloatReg(tokens[1])) &&
                 IsOffset(tokens[2]) && IsIntReg(tokens[3]);
            break;

        // Branch:
        case OP_SHAPE::BR_2REG_LABEL:
            ok = tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsLabel(tokens[3]);
            break;
        case OP_SHAPE::BR_1REG_LABEL:
        case OP_SHAPE::BR_1REG_LABEL_LINK:
            ok = tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2]);
            break;
        case OP_SHAPE::BR_LABEL:
        case OP_SHAPE::BR_LABEL_LINK:
            ok = tokens.size() == 2 && IsLabel(tokens[1]);
            break;
        case OP_SHAPE::BR_1REG:
            ok = tokens.size() == 2 && IsIntReg(tokens[1]);
            break;
        case OP_SHAPE::BR_JALR:
            ok = (tokens.size() == 2 && IsIntReg(tokens[1])) ||
                 (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]));
            break;

        // Int sem HI/LO:
        case OP_SHAPE::INT_3REG:
        case OP_SHAPE::MUL_3REG:
            ok = tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3]);
            break;
        case OP_SHAPE::INT_2REG_IMM_S:
            ok = tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true);
            break;
        case OP_SHAPE::INT_2REG_IMM_U:
            ok = tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false);
            break;
        case OP_SHAPE::INT_1REG_IMM:
            ok = tokens.size() == 3 && IsIntReg(tokens[1]) && IsImmediate(tokens[2], false);
            break;
        case OP_SHAPE::INT_1REG_LO:
        case OP_SHAPE::INT_1REG_HI:
            ok = tokens.size() == 2 && IsIntReg(tokens[1]);
            break;

        // Int com HI/LO:
        case OP_SHAPE::MULDIV_2REG_HILO:
            ok = tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]);
            break;

        // Float:
        case OP_SHAPE::FLOAT_3REG:
            ok = tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3]);
            break;
        case OP_SHAPE::FLOAT_2REG:
            ok = tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]);
            break;
    }

    if (ok) return;

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
    const INSTRUCTION_TYPE type{GetInstructionType()};

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
void InstructionSimplified::SetStageAttributes(
    const std::vector<std::string>& tokens,
    std::vector<Register>&          ex_sources,
    std::vector<Register>&          mem_sources
){
    const std::string op{ToLower(tokens[0])};

    // Não deveria falhar: "ValidateInstruction()" já garantiu que "op" pertence a uma das categorias conhecidas.
    const auto it{OPCODE_SHAPE.find(op)};
    if (it == OPCODE_SHAPE.end()) {
        std::cerr << "[ERRO] Opcode validado mas ausente em OPCODE_SHAPE: " << op << '\n';
        std::abort();
    }

    switch (it->second) {
        // Load:
        case OP_SHAPE::LOAD_INT:
        case OP_SHAPE::LOAD_FLOAT:
        case OP_SHAPE::LOAD_ANY:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[3]));
            return;

        // Store:
        case OP_SHAPE::STORE_INT:
        case OP_SHAPE::STORE_FLOAT:
        case OP_SHAPE::STORE_ANY:
            ex_sources.push_back(LookupReg(tokens[3]));
            mem_sources.push_back(LookupReg(tokens[1]));
            return;

        // Branch:
        case OP_SHAPE::BR_2REG_LABEL:
            ex_sources.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            return;
        case OP_SHAPE::BR_1REG_LABEL:
            ex_sources.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::BR_1REG_LABEL_LINK:
            ex_sources.push_back(LookupReg(tokens[1]));
            dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
            return;
        case OP_SHAPE::BR_LABEL:
            return; // Sem registradores envolvidos.
        case OP_SHAPE::BR_LABEL_LINK:
            dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
            return;
        case OP_SHAPE::BR_1REG:
            ex_sources.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::BR_JALR:
            if (tokens.size() == 2) {
                // rs apenas -> rd = "ra" implícito.
                ex_sources.push_back(LookupReg(tokens[1]));
                dest_registers.push_back(LookupReg("ra"));
                return;
            }
            // rd, rs explícitos.
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            return;

        // Int sem HI/LO:
        case OP_SHAPE::INT_3REG:
        case OP_SHAPE::MUL_3REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            ex_sources.push_back(LookupReg(tokens[3]));
            return;
        case OP_SHAPE::INT_2REG_IMM_S:
        case OP_SHAPE::INT_2REG_IMM_U:
            // imm/shamt não é registrador -> não gera dependência de dado.
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            return;
        case OP_SHAPE::INT_1REG_IMM:
            dest_registers.push_back(LookupReg(tokens[1]));
            return;

        // Int com HI/LO:
        case OP_SHAPE::INT_1REG_LO:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg("lo"));
            return;
        case OP_SHAPE::INT_1REG_HI:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg("hi"));
            return;
        case OP_SHAPE::MULDIV_2REG_HILO:
            dest_registers.push_back(LookupReg("lo"));
            dest_registers.push_back(LookupReg("hi"));
            ex_sources.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            return;

        // Float:
        case OP_SHAPE::FLOAT_3REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            ex_sources.push_back(LookupReg(tokens[3]));
            return;
        case OP_SHAPE::FLOAT_2REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_sources.push_back(LookupReg(tokens[2]));
            return;
    }

    // Não deveria chegar aqui: todo OP_SHAPE tem um "case" acima.
    std::cerr << "[ERRO] Forma (OP_SHAPE) validada mas não tratada em SetStageAttributes(): " << op << '\n';
    std::abort();
}

} // namespace processor
