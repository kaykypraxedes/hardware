/* Architectures/Mips64.cpp */
#include "headers/Mips64.h"

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
    // LOAD:
    LOAD_INT,            // IREG,        OFF(IREG)   [lw,lb,lh,lbu,lhu,ld,lwu,ll]
    LOAD_FLOAT,          // FREG,        OFF(IREG)   [l.d,l.s]

    // STORE:
    STORE_INT,           // IREG,        OFF(IREG)   [sw,sb,sh,sd,sc]
    STORE_FLOAT,         // FREG,        OFF(IREG)   [s.d,s.s]

    // BRANCH:
    BR_2REG_LABEL,       // rs, rt, label               [beq,bne]
    BR_1REG_LABEL,       // rs, label                   [bnez,beqz,bgtz,bltz,bgez,blez]
    BR_1REG_LABEL_LINK,  // rs, label   (+ "ra" dest)   [bltzal,bgezal]
    BR_LABEL,            // label                       [j]
    BR_LABEL_LINK,       // label       (+ "ra" dest)   [jal]
    BR_1REG,             // rs                          [jr]
    BR_JALR,             // (rs) OU (rd, rs) — duas sintaxes  [jalr]

    // INT_BASIC:
    INT_3REG,            // rd, rs, rt                  [add..dsltu, sllv/srlv/srav — 17 opcodes]
    INT_2REG_IMM_S,      // rt, rs, imm (com sinal)     [addi..dsltiu — 7 opcodes]
    INT_2REG_IMM_U,      // rt, rs, imm (sem sinal / shamt) [andi,ori,xori,sll,srl,sra,dsll,dsrl,dsra — 9 opcodes]
    INT_1REG_IMM,        // rt, imm  (sem rs)           [lui]
    INT_1REG_LO,         // rd  (fonte implícita LO)    [mflo]
    INT_1REG_HI,         // rd  (fonte implícita HI)    [mfhi]
    INT_1REG_MTLO,       // rs  (destino implícito LO)  [mtlo]
    INT_1REG_MTHI,       // rs  (destino implícito HI)  [mthi]

    // INT_MUL / INT_DIV:
    MULDIV_2REG_HILO,    // rs, rt  -> dest implícito HI/LO  [mult,multu,dmult,dmultu,div,divu,ddiv,ddivu]
    MUL_3REG,            // rd, rs, rt                  [mul]

    // FLOAT:
    FLOAT_3REG,          // fd, fs, ft                  [add.d,add.s,sub.d,sub.s,mul.d,mul.s,div.d,div.s]
    FLOAT_2REG,          // fd, fs (unário/conversão)   [abs.s,abs.d,neg.s,neg.d,sqrt.s,sqrt.d,cvt.* — 12 opcodes]
    FLOAT_COND,          // fs, ft (escreve em "fcc")   [c.le.s,c.lt.s,c.eq.s,c.le.d,c.lt.d,c.eq.d]
};

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const size_t biggest_instruction{7};

// Opcodes da arquitetura MIPS64:
static const std::vector<std::string> LOADS
    {"lw", "lb", "lh", "lbu", "lhu", "ld", "lwu", "ll", "l.d", "l.s"};

static const std::vector<std::string> STORES
    {"sw", "sb", "sh", "sd", "sc", "s.d", "s.s"};

static const std::vector<std::string> INT_BASIC
    {"add", "addu", "daddu", "sub", "subu", "dsubu", "and", "or", "xor", "nor",
     "sllv", "srlv", "srav", "slt", "sltu", "dslt", "dsltu", "addi", "addiu",
     "daddiu", "slti", "sltiu", "dslti", "dsltiu", "andi", "ori", "xori",
     "sll", "srl", "sra", "dsll", "dsrl", "dsra", "lui", "mflo", "mfhi", "mtlo", "mthi"};

static const std::vector<std::string> BRANCHES
    {"beq", "bne", "bnez", "beqz", "bgtz", "bltz", "bgez", "blez", "bltzal", "bgezal", "j", "jal", "jr", "jalr"};

static const std::vector<std::string> INT_MUL
    {"mul", "mult", "multu", "dmult", "dmultu"};

static const std::vector<std::string> INT_DIV
    {"div", "divu", "ddiv", "ddivu"};

static const std::vector<std::string> FLOAT_BASIC
    {"add.d", "add.s", "sub.d", "sub.s", "abs.s", "abs.d", "neg.s", "neg.d",
     "sqrt.s", "sqrt.d", "cvt.s.w", "cvt.d.w", "cvt.s.d", "cvt.d.s", "cvt.w.s",
     "cvt.w.d", "c.le.s", "c.lt.s", "c.eq.s", "c.le.d", "c.lt.d", "c.eq.d"};

static const std::vector<std::string> FLOAT_MUL
    {"mul.d", "mul.s"};

static const std::vector<std::string> FLOAT_DIV
    {"div.d", "div.s"};

const std::unordered_map<std::string, Register>& RegisterTable() {
    static std::unordered_map<std::string, Register> t;

    if (t.empty()) {
        for (int i = 0; i < 32; i++) {
            t.emplace("r" + std::to_string(i), Register('R', i));
            t.emplace("f" + std::to_string(i), Register('F', 32 + i));
        }
        t.emplace("ra", Register('R', 31)); // Alias
        t.emplace("hi", Register('M', 64)); // Mult/Div
        t.emplace("lo", Register('M', 65));
        t.emplace("fcc", Register('C', 66)); // Flag de condicao
    }
    return t;
}

// ─── HELPERS ──────────────────────────────────────────────────────
static std::string ToLower(const std::string& token) {
    std::string lower{token};
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower;
}

static bool IsIntReg(const std::string& token) {
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};
    return (it != table.end() && it->second.GetType() == 'R');
}

static bool IsFloatReg(const std::string& token) {
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};
    return (it != table.end() && it->second.GetType() == 'F');
}

static bool IsImmediate(const std::string& token, const bool is_signed) {
    if (token.empty()) return false;
    std::size_t i{0};

    if (token[i] == '#') i++;
    if (i < token.size() && token[i] == '-') {
        if (!is_signed) return false;
        i++;
    }
    if (i >= token.size()) return false;
    for (; i < token.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;

    return true;
}

static bool IsOffset(const std::string& token) {
    if (token.empty()) return false;
    std::size_t i{0};
    if (token[0] == '-') i = 1;
    if (i >= token.size()) return false;
    for (; i < token.size(); i++)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    return true;
}

static bool IsLabel(const std::string& token) {
    if (token.empty()) return false;
    if (IsIntReg(token) || IsFloatReg(token)) return false;
    if (IsImmediate(token, true)) return false;
    if (IsImmediate(token, false)) return false;

    if (!std::isalpha(static_cast<unsigned char>(token[0])) && token[0] != '_') return false;
    for (const char c : token)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

static const Register& LookupReg(const std::string& token) {
    const auto& table{RegisterTable()};
    const auto  it{table.find(ToLower(token))};
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

    // STORE
    {"sw", OP_SHAPE::STORE_INT},    {"sb", OP_SHAPE::STORE_INT}, {"sh", OP_SHAPE::STORE_INT},
    {"sd", OP_SHAPE::STORE_INT},    {"sc", OP_SHAPE::STORE_INT},
    {"s.d", OP_SHAPE::STORE_FLOAT}, {"s.s", OP_SHAPE::STORE_FLOAT},

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
    {"nor", OP_SHAPE::INT_3REG},  {"sllv", OP_SHAPE::INT_3REG}, {"srlv", OP_SHAPE::INT_3REG},
    {"srav", OP_SHAPE::INT_3REG}, {"slt", OP_SHAPE::INT_3REG},  {"sltu", OP_SHAPE::INT_3REG},
    {"dslt", OP_SHAPE::INT_3REG}, {"dsltu", OP_SHAPE::INT_3REG},

    {"addi", OP_SHAPE::INT_2REG_IMM_S},  {"addiu", OP_SHAPE::INT_2REG_IMM_S},
    {"daddiu", OP_SHAPE::INT_2REG_IMM_S},
    {"slti", OP_SHAPE::INT_2REG_IMM_S},  {"sltiu", OP_SHAPE::INT_2REG_IMM_S},
    {"dslti", OP_SHAPE::INT_2REG_IMM_S}, {"dsltiu", OP_SHAPE::INT_2REG_IMM_S},

    {"andi", OP_SHAPE::INT_2REG_IMM_U}, {"ori", OP_SHAPE::INT_2REG_IMM_U},  {"xori", OP_SHAPE::INT_2REG_IMM_U},
    {"sll", OP_SHAPE::INT_2REG_IMM_U},  {"srl", OP_SHAPE::INT_2REG_IMM_U},  {"sra", OP_SHAPE::INT_2REG_IMM_U},
    {"dsll", OP_SHAPE::INT_2REG_IMM_U}, {"dsrl", OP_SHAPE::INT_2REG_IMM_U}, {"dsra", OP_SHAPE::INT_2REG_IMM_U},

    {"lui", OP_SHAPE::INT_1REG_IMM},
    {"mflo", OP_SHAPE::INT_1REG_LO},   {"mfhi", OP_SHAPE::INT_1REG_HI},
    {"mtlo", OP_SHAPE::INT_1REG_MTLO}, {"mthi", OP_SHAPE::INT_1REG_MTHI},

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
    {"abs.s", OP_SHAPE::FLOAT_2REG},   {"abs.d", OP_SHAPE::FLOAT_2REG},
    {"neg.s", OP_SHAPE::FLOAT_2REG},   {"neg.d", OP_SHAPE::FLOAT_2REG},
    {"sqrt.s", OP_SHAPE::FLOAT_2REG},  {"sqrt.d", OP_SHAPE::FLOAT_2REG},
    {"cvt.s.w", OP_SHAPE::FLOAT_2REG}, {"cvt.d.w", OP_SHAPE::FLOAT_2REG},
    {"cvt.s.d", OP_SHAPE::FLOAT_2REG}, {"cvt.d.s", OP_SHAPE::FLOAT_2REG},
    {"cvt.w.s", OP_SHAPE::FLOAT_2REG}, {"cvt.w.d", OP_SHAPE::FLOAT_2REG},
    {"c.le.s", OP_SHAPE::FLOAT_COND},  {"c.lt.s", OP_SHAPE::FLOAT_COND}, {"c.eq.s", OP_SHAPE::FLOAT_COND},
    {"c.le.d", OP_SHAPE::FLOAT_COND},  {"c.lt.d", OP_SHAPE::FLOAT_COND}, {"c.eq.d", OP_SHAPE::FLOAT_COND},
};

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
CDB InstructionMips64::MakeCDB() {
    CDB cdb;
    FillCDB(cdb, 'R', 0,  32); // Int
    FillCDB(cdb, 'F', 32, 32); // Float
    FillCDB(cdb, 'M', 64, 2);  // HI/LO
    FillCDB(cdb, 'C', 66, 1);  // Flags (fcc)
    cdb.print_banks = {{'R', 0, 32}, {'F', 32, 32}, {'M', 64, 2}, {'C', 66, 1}};
    return cdb;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
InstructionMips64::InstructionMips64(
    const int position
) : Instruction(position) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
std::vector<std::string> InstructionMips64::SplitInstruction(
    const std::string& str
) const {

    std::vector<std::string> tokens;
    std::string current;
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
    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

bool InstructionMips64::IdentifyType(
    const std::vector<std::string>& tokens
) {
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

void InstructionMips64::ValidateInstruction(
    const std::vector<std::string>& tokens
) {
    const std::string op{ToLower(tokens[0])};

    // Não deveria falhar: "IdentifyType()" já garantiu que "op" pertence a uma das categorias conhecidas.
    const auto it{OPCODE_SHAPE.find(op)};
    if (it == OPCODE_SHAPE.end()) {
        std::cerr << "[ERRO] Opcode reconhecido em IdentifyType() mas ausente em OPCODE_SHAPE: " << op << '\n';
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
        case OP_SHAPE::INT_1REG_MTLO:
        case OP_SHAPE::INT_1REG_MTHI:
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
        case OP_SHAPE::FLOAT_COND:
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

void InstructionMips64::NormalizeInstruction(
    std::vector<std::string>& tokens
) {
    tokens[0] = ToLower(tokens[0]);
    instruction_string = tokens[0];
    for (size_t i{tokens[0].size()}; i < biggest_instruction; i++) instruction_string += " ";

    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        if (tokens.size() < 4) {
            std::cerr << "[ERRO] Instrução incompleta:\n" << "Instrução: "<< tokens[0] << " ...\n";
            std::abort();
        }
        instruction_string += ToLower(tokens[1]) + ", " + tokens[2] + "(" + ToLower(tokens[3]) + ")";
    } else {
        for (size_t i{1}; i < tokens.size(); i++)
            instruction_string += (i == 1 ? "" : ", ") + (IsLabel(tokens[i]) ? tokens[i] : ToLower(tokens[i]));
    }
}

void InstructionMips64::SetAttributes(
    const std::vector<std::string>& tokens
) {
    const std::string& op{tokens[0]};

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
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[3]));
            return;

        // Store:
        case OP_SHAPE::STORE_INT:
        case OP_SHAPE::STORE_FLOAT:
            ex_source_registers.push_back(LookupReg(tokens[3]));
            mem_source_registers.push_back(LookupReg(tokens[1]));
            return;

        // Branch:
        case OP_SHAPE::BR_2REG_LABEL:
            ex_source_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            return;
        case OP_SHAPE::BR_1REG_LABEL:
            ex_source_registers.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::BR_1REG_LABEL_LINK:
            ex_source_registers.push_back(LookupReg(tokens[1]));
            dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
            return;
        case OP_SHAPE::BR_LABEL:
            return; // Sem registradores envolvidos.
        case OP_SHAPE::BR_LABEL_LINK:
            dest_registers.push_back(LookupReg("ra")); // Escreve o endereço de retorno implicitamente.
            return;
        case OP_SHAPE::BR_1REG:
            ex_source_registers.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::BR_JALR:
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

        // Int sem HI/LO:
        case OP_SHAPE::INT_3REG:
        case OP_SHAPE::MUL_3REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            ex_source_registers.push_back(LookupReg(tokens[3]));
            return;
        case OP_SHAPE::INT_2REG_IMM_S:
        case OP_SHAPE::INT_2REG_IMM_U:
            // imm/shamt não é registrador (não gera dependência de dado).
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            return;
        case OP_SHAPE::INT_1REG_IMM:
            dest_registers.push_back(LookupReg(tokens[1]));
            return;

        // Int com HI/LO:
        case OP_SHAPE::INT_1REG_LO:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg("lo"));
            return;
        case OP_SHAPE::INT_1REG_HI:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg("hi"));
            return;
        case OP_SHAPE::INT_1REG_MTLO:
            dest_registers.push_back(LookupReg("lo"));
            ex_source_registers.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::INT_1REG_MTHI:
            dest_registers.push_back(LookupReg("hi"));
            ex_source_registers.push_back(LookupReg(tokens[1]));
            return;
        case OP_SHAPE::MULDIV_2REG_HILO:
            dest_registers.push_back(LookupReg("lo"));
            dest_registers.push_back(LookupReg("hi"));
            ex_source_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            return;

        // Float:
        case OP_SHAPE::FLOAT_3REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            ex_source_registers.push_back(LookupReg(tokens[3]));
            return;
        case OP_SHAPE::FLOAT_2REG:
            dest_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            return;
        case OP_SHAPE::FLOAT_COND:
            dest_registers.push_back(LookupReg("fcc")); // Escreve na flag de condição
            ex_source_registers.push_back(LookupReg(tokens[1]));
            ex_source_registers.push_back(LookupReg(tokens[2]));
            return;
    }

    // Não deveria chegar aqui: todo OP_SHAPE tem um "case" acima.
    std::cerr << "[ERRO] Forma (OP_SHAPE) validada mas não tratada em SetAttributes(): " << op << '\n';
    std::abort();
}

} // namespace processor
