/* Instruction.cpp */
#include "headers/Instruction.h"

namespace processor {

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

Instruction::Instruction(const int position) : position(position) {
    if (position < -1) {
        std::cerr << "[ERRO] Valor inválido de posição: " << position << "\n";
        std::abort();
    }
}

void Instruction::Parse(const std::string& str) {
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

int Instruction::GetPosition() const { return position; }
int Instruction::GetExLatency() const { return ex_latency; }
int Instruction::GetMemLatency() const { return mem_latency; }
INSTRUCTION_TYPE Instruction::GetInstructionType() const { return type; }
const std::string& Instruction::GetInstructionString() const { return instruction_string; }
const std::vector<Register>& Instruction::GetDestRegisters() const { return dest_registers; }
const std::vector<Register>& Instruction::GetSourceRegisters() const { return source_registers; }

void Instruction::SetMemLatency(const int latency) { mem_latency = latency; }
void Instruction::SetExLatency(const int latency) { ex_latency = latency; }

void Instruction::SetLatencies() {
    if (type == INSTRUCTION_TYPE::LOAD) {
        mem_latency = base_mem_latencies[0];
    } else if (type == INSTRUCTION_TYPE::STORE) {
        mem_latency = base_mem_latencies[1];
    }
    ex_latency = base_ex_latencies[static_cast<int>(type)];
}

} // namespace processor
