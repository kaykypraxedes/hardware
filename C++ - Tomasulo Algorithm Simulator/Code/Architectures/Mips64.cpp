/* Architectures/Mips64.cpp */
#include "headers/Mips64.h"

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
        t.emplace("fcc", Register('C', 66)); // Flag de condicao do coproc 1
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

    // Suporte para imediatos com ou sem "#"
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
std::vector<std::string> InstructionMips64::SplitInstruction(const std::string& str) const {
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

bool InstructionMips64::IdentifyType(const std::vector<std::string>& tokens) {
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

void InstructionMips64::ValidateInstruction(const std::vector<std::string>& tokens) {
    const std::string op{ToLower(tokens[0])};
    switch (type) {
        case INSTRUCTION_TYPE::LOAD: {
            if (op == "lw") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "lb") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "lh") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "lbu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "lhu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "ld") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "lwu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "ll") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "l.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "l.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            if (op == "sw") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "sb") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "sh") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "sd") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "sc") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "s.d") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "s.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsOffset(tokens[2]) && IsIntReg(tokens[3])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            if (op == "beq") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsLabel(tokens[3])) return;
            } else if (op == "bne") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsLabel(tokens[3])) return;
            } else if (op == "bnez" || op == "beqz" || op == "bgtz" || op == "bltz" || op == "bgez" || op == "blez" || op == "bltzal" || op == "bgezal") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsLabel(tokens[2])) return;
            } else if (op == "j" || op == "jal") {
                if (tokens.size() == 2 && IsLabel(tokens[1])) return;
            } else if (op == "jr") {
                if (tokens.size() == 2 && IsIntReg(tokens[1])) return;
            } else if (op == "jalr") {
                if (tokens.size() == 2 && IsIntReg(tokens[1])) return;
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            if (op == "add" || op == "addu" || op == "daddu" || op == "sub" || op == "subu" || op == "dsubu" || op == "and" || op == "or" || op == "xor" || op == "nor" || op == "slt" || op == "sltu" || op == "dslt" || op == "dsltu" || op == "sllv" || op == "srlv" || op == "srav") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3])) return;
            } else if (op == "addi" || op == "addiu" || op == "daddiu" || op == "slti" || op == "sltiu" || op == "dslti" || op == "dsltiu") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], true)) return;
            } else if (op == "andi" || op == "ori" || op == "xori" || op == "sll" || op == "srl" || op == "sra" || op == "dsll" || op == "dsrl" || op == "dsra") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsImmediate(tokens[3], false)) return;
            } else if (op == "lui") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsImmediate(tokens[2], false)) return;
            } else if (op == "mflo" || op == "mfhi" || op == "mtlo" || op == "mthi") {
                if (tokens.size() == 2 && IsIntReg(tokens[1])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL: {
            if (op == "mult" || op == "multu" || op == "dmult" || op == "dmultu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2])) return;
            } else if (op == "mul") {
                if (tokens.size() == 4 && IsIntReg(tokens[1]) && IsIntReg(tokens[2]) && IsIntReg(tokens[3])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_DIV: {
            if (op == "div" || op == "divu" || op == "ddiv" || op == "ddivu") {
                if (tokens.size() == 3 && IsIntReg(tokens[1]) && IsIntReg(tokens[2])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC: {
            if (op == "add.d" || op == "add.s" || op == "sub.d" || op == "sub.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3])) return;
            } else if (op == "abs.s" || op == "abs.d" || op == "neg.s" || op == "neg.d" || op == "sqrt.s" || op == "sqrt.d" || op == "cvt.s.w" || op == "cvt.d.w" || op == "cvt.s.d" || op == "cvt.d.s" || op == "cvt.w.s" || op == "cvt.w.d") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2])) return;
            } else if (op == "c.le.s" || op == "c.lt.s" || op == "c.eq.s" || op == "c.le.d" || op == "c.lt.d" || op == "c.eq.d") {
                if (tokens.size() == 3 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_MUL: {
            if (op == "mul.d" || op == "mul.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3])) return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            if (op == "div.d" || op == "div.s") {
                if (tokens.size() == 4 && IsFloatReg(tokens[1]) && IsFloatReg(tokens[2]) && IsFloatReg(tokens[3])) return;
            }
            break;
        }
        default: break;
    }
    std::cerr << "[ERRO] Sintaxe inválida para a instrução:\n- Instrução: ";
    for (size_t i{}; i < tokens.size(); i++) std::cerr << tokens[i] << ' ';
    std::cerr << '\n';
    std::abort();
}

void InstructionMips64::NormalizeInstruction(std::vector<std::string>& tokens) {
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

void InstructionMips64::SetAttributes(const std::vector<std::string>& tokens) {
    const std::string& op{tokens[0]};
    switch (type) {
        case INSTRUCTION_TYPE::LOAD: {
            if (op == "lw" || op == "lb" || op == "lh" || op == "lbu" || op == "lhu" || op == "ld" || op == "lwu" || op == "ll" || op == "l.d" || op == "l.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::STORE: {
            if (op == "sw" || op == "sb" || op == "sh" || op == "sd" || op == "sc" || op == "s.d" || op == "s.s") {
                ex_source_registers.push_back(LookupReg(tokens[3]));
                mem_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::BRANCH: {
            if (op == "beq" || op == "bne") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            } else if (op == "bnez" || op == "beqz" || op == "bgtz" || op == "bltz" || op == "bgez" || op == "blez") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            } else if (op == "bltzal" || op == "bgezal") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                dest_registers.push_back(LookupReg("ra"));
                return;
            } else if (op == "j") {
                return;
            } else if (op == "jal") {
                dest_registers.push_back(LookupReg("ra"));
                return;
            } else if (op == "jr") {
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            } else if (op == "jalr") {
                if (tokens.size() == 2) {
                    ex_source_registers.push_back(LookupReg(tokens[1]));
                    dest_registers.push_back(LookupReg("ra"));
                } else {
                    dest_registers.push_back(LookupReg(tokens[1]));
                    ex_source_registers.push_back(LookupReg(tokens[2]));
                }
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_BASIC: {
            if (op == "add" || op == "addu" || op == "daddu" || op == "sub" || op == "subu" || op == "dsubu" || op == "and" || op == "or" || op == "xor" || op == "nor" || op == "slt" || op == "sltu" || op == "dslt" || op == "dsltu" || op == "sllv" || op == "srlv" || op == "srav") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            } else if (op == "addi" || op == "addiu" || op == "daddiu" || op == "slti" || op == "sltiu" || op == "dslti" || op == "dsltiu" || op == "andi" || op == "ori" || op == "xori" || op == "sll" || op == "srl" || op == "sra" || op == "dsll" || op == "dsrl" || op == "dsra") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            } else if (op == "lui") {
                dest_registers.push_back(LookupReg(tokens[1]));
                return;
            } else if (op == "mflo") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg("lo"));
                return;
            } else if (op == "mfhi") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg("hi"));
                return;
            } else if (op == "mtlo") {
                dest_registers.push_back(LookupReg("lo"));
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            } else if (op == "mthi") {
                dest_registers.push_back(LookupReg("hi"));
                ex_source_registers.push_back(LookupReg(tokens[1]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_MUL: {
            if (op == "mult" || op == "multu" || op == "dmult" || op == "dmultu") {
                dest_registers.push_back(LookupReg("lo"));
                dest_registers.push_back(LookupReg("hi"));
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            } else if (op == "mul") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::INT_DIV: {
            if (op == "div" || op == "divu" || op == "ddiv" || op == "ddivu") {
                dest_registers.push_back(LookupReg("lo"));
                dest_registers.push_back(LookupReg("hi"));
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_BASIC: {
            if (op == "add.d" || op == "add.s" || op == "sub.d" || op == "sub.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            } else if (op == "abs.s" || op == "abs.d" || op == "neg.s" || op == "neg.d" || op == "sqrt.s" || op == "sqrt.d" || op == "cvt.s.w" || op == "cvt.d.w" || op == "cvt.s.d" || op == "cvt.d.s" || op == "cvt.w.s" || op == "cvt.w.d") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            } else if (op == "c.le.s" || op == "c.lt.s" || op == "c.eq.s" || op == "c.le.d" || op == "c.lt.d" || op == "c.eq.d") {
                dest_registers.push_back(LookupReg("fcc")); // Escreve implicitamente na flag de condição
                ex_source_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_MUL: {
            if (op == "mul.d" || op == "mul.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        case INSTRUCTION_TYPE::FLOAT_DIV: {
            if (op == "div.d" || op == "div.s") {
                dest_registers.push_back(LookupReg(tokens[1]));
                ex_source_registers.push_back(LookupReg(tokens[2]));
                ex_source_registers.push_back(LookupReg(tokens[3]));
                return;
            }
            break;
        }
        default: break;
    }
    std::cerr << "[ERRO] Opcode validado mas não tratado em SetAttributes(): " << op << '\n';
    std::abort();
}

} // namespace processor
