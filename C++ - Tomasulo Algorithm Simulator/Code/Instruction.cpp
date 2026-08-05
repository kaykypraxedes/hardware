/* Instruction.cpp */
#include "headers/Instruction.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
std::vector<int> Instruction::base_ex_latencies = {
    0,   // INVALID / NONE
    1,   // LOAD
    1,   // STORE
    1,   // BRANCH
    1,   // INT_BASIC
    4,   // INT_MUL
    10,  // INT_DIV
    9,   // FLOAT_BASIC
    14,  // FLOAT_MUL
    40   // FLOAT_DIV
};

std::vector<int> Instruction::base_mem_latencies = {
    1,  // LOAD
    1   // STORE / BRANCH
};

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Instruction::GetPosition()   const { return position; }

// Público:
int Instruction::GetExLatency()  const { return ex_latency; }

// Público:
int Instruction::GetMemLatency() const { return mem_latency; }

// Público:
INSTRUCTION_TYPE Instruction::GetInstructionType() const { return type; }

// Público:
const std::vector<Register>& Instruction::GetDestRegisters()   const { return dest_registers; }

// Público:
const std::vector<Register>& Instruction::GetSourceRegisters() const { return source_registers; }

// Público:
const std::string& Instruction::GetInstructionString()         const { return instruction_string; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Instruction::Instruction(
    const int position
) :
    position(position)
{
    if (position < -1) {
        std::cerr << "[ERRO] Valor inválido de posição: " << position << "\n";
        std::abort();
    }
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Instruction::Parse(
    const std::string& str
){
    if (str.empty()) {
        std::cerr << "[ERRO] String vazia passada como instrução!\n";
        std::abort();
    }

    std::vector<std::string> tokens = SplitInstruction(str);

    if (tokens.empty() || !IdentifyType(tokens[0])) {
        std::cerr << "[ERRO] Instrução não suportada: " << (tokens.empty() ? "" : tokens[0]) << "\n";
        std::abort();
    }

    NormalizeInstruction(tokens);
    SetAttributes(tokens);
    SetLatencies();
}

// Privado:
void Instruction::SetLatencies() {
    if (type == INSTRUCTION_TYPE::LOAD) {
        mem_latency = base_mem_latencies[0];
    } else if (type == INSTRUCTION_TYPE::STORE) {
        mem_latency = base_mem_latencies[1];
    }
    ex_latency = base_ex_latencies[static_cast<int>(type)];
}

// Público:
void Instruction::SetExLatency(
    const int latency
){
    ex_latency = latency;
}

// Público:
void Instruction::SetMemLatency(
    const int latency
){
    mem_latency = latency;
}

} // namespace processor
