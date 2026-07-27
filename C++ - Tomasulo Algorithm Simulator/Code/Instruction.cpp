/* Instruction.cpp */
#include "headers/Instruction.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
std::vector<int> Instruction::base_ex_latencies  = {0,1,1,1,1,4,10,9,14,40};
std::vector<int> Instruction::base_mem_latencies = {1,1};

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Instruction::GetPC()                const { return PC;            }

// Público:
int Instruction::GetExLatency()         const { return ex_latency;    }

// Público:
int Instruction::GetMemLatency()        const { return mem_latency;   }

// Público:
Register Instruction::GetDestRegister() const { return dest_register; }

// Público:
Register Instruction::GetJ()            const { return reg_j;         }

// Público:
Register Instruction::GetK()            const { return reg_k;         }

// Público:
INSTRUCTION_TYPE Instruction::GetInstructionType()     const { return type;               }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const std::string& Instruction::GetInstructionString() const { return instruction_string; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Instruction::Instruction(
    int PC, std::string instruction_string
):
    instruction_string(instruction_string),
    PC(PC)
{
    ParseInstruction();
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Instruction::ParseInstruction(){
    std::vector<std::string> tokens{SplitInstruction()};
    IdentifyType(tokens[0]);
    SetLatencies();
    SetAttributes(tokens);
}

// Privado:
std::vector<std::string> Instruction::SplitInstruction(){
    // ADD   R1 R2 R3 => tokens[0] = ADD,   tokens[1] = R1, tokens[2] = R2,   tokens[3] = R3
    // LOAD  R1 n(R2) => tokens[0] = LOAD,  tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // STORE R1 n(R2) => tokens[0] = STORE, tokens[1] = R1, tokens[2] = n,    tokens[3] = R2
    // BNEZ  R1 label => tokens[0] = BNEZ,  tokens[1] = R1, tokens[2] = label
    // JR    R1       => tokens[0] = JR,    tokens[1] = R1
    // JUMP  label    => tokens[0] = JUMP,  tokens[1] = label
    std::vector<std::string> tokens{};
    std::string current_token{};
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
void Instruction::IdentifyType(
    const std::string& op
){
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
    else type = INSTRUCTION_TYPE::NONEXISTENT;
}

// Privado:
void Instruction::SetLatencies(){
    if(type == INSTRUCTION_TYPE::LOAD){
        mem_latency = base_mem_latencies[0];
    } else if(type == INSTRUCTION_TYPE::STORE){
        mem_latency = base_mem_latencies[1];
    }
    ex_latency = base_ex_latencies[static_cast<int>(type)]; // Enums podem ser convertidos em int ()
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
    if (type == INSTRUCTION_TYPE::LOAD){
        dest_register = Register(tokens[1]);
        reg_k         = Register(tokens[3]);
    } else if (type == INSTRUCTION_TYPE::STORE){
        reg_j         = Register(tokens[1]);
        reg_k         = Register(tokens[3]);
    } else if (type == INSTRUCTION_TYPE::BRANCH){
        if(std::toupper(tokens[1][0]) == 'F' || std::toupper(tokens[1][0]) == 'R')
            reg_j   = Register(tokens[1]);
        if(tokens.size() > 3) if(std::toupper(tokens[2][0]) == 'F' || std::toupper(tokens[2][0]) == 'R')
            reg_k   = Register(tokens[2]);
    } else {
        dest_register = Register(tokens[1]);
        reg_j         = Register(tokens[2]);
        reg_k         = Register(tokens[3]);
    }
}

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
