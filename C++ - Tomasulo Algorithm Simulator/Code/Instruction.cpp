/* Instruction.cpp */
#include "headers/Instruction.h"
#include <cstddef>

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
std::vector<int> Instruction::base_ex_latencies  = {
    0,  // NONEEXISTENT
    1,  // LOAD
    1,  // STORE
    1,  // BRANCH
    1,  // INT_BASIC
    4,  // INT_MUL
    10, // INT_DIV
    9,  // FLOAT_BASIC
    14, // FLOAT_MUL
    40  // FLOAT_DIV
};
std::vector<int> Instruction::base_mem_latencies = {
    1, // LOAD
    1  // BRANCH
};

static bool IsRegister(
    const std::string& token
){
    // R0 - R32 (pelo menos 2 caracteres).
    if (token.size() < 2) return false;
    // Se não começa com 'R' ou com 'F' não é registrador
    char first = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (first != 'R' && first != 'F') return false;
    // Se não começa com 'R' ou 'F' mas não for apenas números no final, não é token (R44A).
    std::string number;
    for (size_t i = 1; i < token.size(); ++i)
        if (!std::isdigit(static_cast<unsigned char>(token[i]))) return false;
    // Não precisa verificar se os números estão no RANGE pois isso é dever do construtor de registradores
    return true;
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Instruction::GetPosition()          const { return position; }

// Público:
int Instruction::GetExLatency()         const { return ex_latency; }

// Público:
int Instruction::GetMemLatency()        const { return mem_latency; }

// Público:
INSTRUCTION_TYPE Instruction::GetInstructionType()     const { return type; }

// Público:
const Register& Instruction::GetDestRegister()         const { return dest_register; }

// Público:
const Register& Instruction::GetJ()                    const { return reg_j; }

// Público:
const Register& Instruction::GetK()                    const { return reg_k; }

// Público:
const std::string& Instruction::GetInstructionString() const { return instruction_string; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Instruction::Instruction(
    const int position,
    const std::string& instruction_string
) {
    // Verifica se é uma posição válida (>= 0).
    if(position < -1){ // -1 é o valor de instrução não iniciada
        std::cerr << "[ERRO] Valor inválido de posição: " << position << "\n";
        std::abort();
    }
    this->position = position;
    // Só define a instrução se ela for válida (foi passado com PC)
    if(position != -1 && !instruction_string.empty()) ParseInstruction(instruction_string);
    else this->position = -1;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Instruction::ParseInstruction(
    const std::string& instruction_string
) {
    std::vector<std::string> tokens{SplitInstruction(instruction_string)};
    // Tenta identificar o tipo da instrução:
    if(!IdentifyType(tokens[0])){
        std::cerr << "[ERRO] Instrução não suportada: " << tokens[0] << "\n";
        std::abort();
    }
    NormalizeInstruction(tokens);
    SetAttributes(tokens);
    SetLatencies();
}

// Privado:
std::vector<std::string> Instruction::SplitInstruction(
    const std::string& instruction_string
) const {
    // ADD   R1 R2 R3 => tokens[0] = ADD,   tokens[1] = R1, tokens[2] = R2,   tokens[3] = R3
    // LOAD  R1 n(R2) => tokens[0] = LOAD,  tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // STORE R1 n(R2) => tokens[0] = STORE, tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // BNEZ  R1 label => tokens[0] = BNEZ,  tokens[1] = R1, tokens[2] = label
    // JR    R1       => tokens[0] = JR,    tokens[1] = R1
    // JUMP  label    => tokens[0] = JUMP,  tokens[1] = label
    std::vector<std::string> tokens;
    std::string current_token;
    for (char c : instruction_string) {
        if (c == ',' || c == ' ' || c == '(' || c == ')' || c == '\t') {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token = "";
            }
        } else current_token += c;
    }
    if (!current_token.empty()) tokens.push_back(current_token);
    return tokens;
}

// Privado:
bool Instruction::IdentifyType(
    const std::string& prev_op
){
    // Normaliza a string da operação antes de fazer a comparação.
    std::string op;
    for(char c : prev_op) op.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    // LOAD
    if (op=="LOAD"||op=="LW"||op=="LB"||op=="LH"||op=="LBU"||op=="LHU"||op=="L.D"||op=="L.S"||op=="LD"||op=="LWU"||op=="LL")
        type = INSTRUCTION_TYPE::LOAD;
    // STORE
    else if (op=="STORE"||op=="SW"||op=="SB"||op=="SH"||op=="S.D"||op=="S.S"|| op=="SD"||op=="SC")
        type = INSTRUCTION_TYPE::STORE;
    // INT_BASICO (inclui adição, subtração, shifts, operações lógicas e comparações sem saltos)
    else if (
        op=="ADD" ||op=="ADDI" ||op=="ADDU" ||op=="ADDIU"||op=="DADDU"||op=="DADDIU"||op=="SUB" ||op=="SUBI" ||op=="SUBU" ||op=="DSUBU" ||
        op=="AND" ||op=="ANDI" ||op=="OR" ||op=="ORI" ||op=="XOR" ||op=="XORI" || op=="NOR" ||op=="LUI" ||
        op=="SLL" ||op=="SRL" ||op=="SRA" ||op=="SLLV" ||op=="SRLV" ||op=="SRAV" ||op=="DSLL" ||op=="DSRL" ||op=="DSRA" ||
        op=="SLT" ||op=="SLTI" ||op=="SLTU" ||op=="SLTIU"||op=="DSLT" ||op=="DSLTI"||op=="DSLTU"||op=="DSLTIU")
        type = INSTRUCTION_TYPE::INT_BASIC;
    // BRANCH
    else if (
        op=="BEQ" ||op=="BNE" ||op=="BNEZ" ||op=="BEQZ" ||op=="BGTZ" ||op=="BLTZ"||op=="BGEZ" ||op=="BLEZ" ||op=="BLTZAL"||op=="BGEZAL"||
        op=="J" ||op=="JAL" ||op=="JR" ||op=="JALR")
        type = INSTRUCTION_TYPE::BRANCH;
    // INT_MUL
    else if (op=="MULT"||op=="MULTU"||op=="MUL"||op=="DMULT"||op=="DMULTU")
        type = INSTRUCTION_TYPE::INT_MUL;
    // INT_DIV
    else if (op=="DIV" ||op=="DIVU" ||op=="DDIV"||op=="DDIVU")
        type = INSTRUCTION_TYPE::INT_DIV;
    // FLOAT_BASICO
    else if (op=="ADD.D"||op=="ADD.S"|| op=="SUB.D"||op=="SUB.S")
        type = INSTRUCTION_TYPE::FLOAT_BASIC;
    // FLOAT_MUL
    else if (op=="MUL.D"||op=="MUL.S")
        type = INSTRUCTION_TYPE::FLOAT_MUL;
    // FLOAT_DIV
    else if (op=="DIV.D"||op=="DIV.S")
        type = INSTRUCTION_TYPE::FLOAT_DIV;
    // Instrução não suportada
    else return false;
    return true;
}

// Privado:
void Instruction::NormalizeInstruction(
    std::vector<std::string>& tokens
){
    // Uppercase em todos os tokens (opcode, registradores, offsets/labels).
    for (std::string& token : tokens)
        for (char& c : token) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    std::string normalized{tokens[0]};

    // Descobre o número de caracteres do opcode da instrução.
    size_t n{};
    for (unsigned char c : normalized)
        if ((c & 0xC0) != 0x80) n++;   // ignora bytes de continuação do UTF-8

    // Deixa todos os registradores começando no mesmo ponto.
    for (size_t i{}; i < 7 - n; i++) normalized += ' ';

    // Load e Store:
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // tokens: [OP, REG, OFFSET, BASE_REG] -> "OP REG, OFFSET(BASE_REG)"
        normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";
    } // Salto (pode possuir label, que deve permanecer minúsculo por convenção):
    else if (type == INSTRUCTION_TYPE::BRANCH) {
        // BEQZ R1, LABEL -> BEQZ R1, label
        // BEQ R1, R2, LABEL -> BEQ R1, R2, label
        // J LABEL -> J label
        // JR F4 -> JR R4 (sem alterações)
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string token{tokens[i]};
            if (!IsRegister(token))
                for (char& c : token) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            normalized += (i == 1 ? "" : ", ") + token;
        }
    }  // Resto
    else {
        for (size_t i = 1; i < tokens.size(); ++i)
            normalized += (i == 1 ? "" : ", ") + tokens[i];
    }
    instruction_string = normalized;
}

// Privado:
void Instruction::SetLatencies(){
    if(type == INSTRUCTION_TYPE::LOAD){
        mem_latency = base_mem_latencies[0];
    } else if(type == INSTRUCTION_TYPE::STORE){
        mem_latency = base_mem_latencies[1];
    }
    ex_latency = base_ex_latencies[static_cast<int>(type)]; // Enums podem ser convertidos em int
}

// Privado:
void Instruction::SetAttributes(
    std::vector<std::string>& tokens
){
    // LOAD  R1 n(R2)    => dest_register = R1, J = -,   K = R2
    // STORE R1 n(R2)    => dest_register = -,  J = R1,  K = R2
    // BEQ   R1 R2 label => dest_register = -,  J = R1,  K = R2
    // BNEZ  R1 label    => dest_register = -,  J = R1,  K = -
    // JR    R1          => dest_register = -,  J = R1,  K = -
    // J     label       => dest_register = -,  J = -,   K = -
    // ADD   R1 R2 R3    => dest_register = R1, J = R2,  K = R3
    // ADDI  R1 R2 n     => dest_register = R1, J = R2,  K = -
    if (type == INSTRUCTION_TYPE::LOAD){
        dest_register = Register(tokens[1]);
        reg_k         = Register(tokens[3]);
    } else if (type == INSTRUCTION_TYPE::STORE){
        reg_j         = Register(tokens[1]);
        reg_k         = Register(tokens[3]);
    } else if (type == INSTRUCTION_TYPE::BRANCH){
        if (IsRegister(tokens[1]))                      // (J    label)
            reg_j = Register(tokens[1]);                // (BEQZ R1, label)
        if(tokens.size() > 3 && IsRegister(tokens[2]))  // K nem sempre existe
            reg_k = Register(tokens[2]);                // (BEQ  R1, R2, label)
    } else {
        dest_register = Register(tokens[1]);
        // Verificação de validade:
        if(tokens.size() > 2 && (std::toupper(tokens[2][0]) == 'F' || std::toupper(tokens[2][0]) == 'R'))
            reg_j = Register(tokens[2]);
        if(tokens.size() > 3 && (std::toupper(tokens[3][0]) == 'F' || std::toupper(tokens[3][0]) == 'R'))
            reg_k = Register(tokens[3]);
    }
}

// Funções para definir manualmente algumas latências diferentes e deixar a simulação mais realista
// - Simular "cache miss", onde a instrução que demorava 15 cíclos no MEM agora demora 100

// Público:
void Instruction::SetMemLatency(
    int latency
){
    mem_latency = latency;
}

// Público:
void Instruction::SetExLatency(
    int latency
){
    ex_latency = latency;
}

} // namespace processor
