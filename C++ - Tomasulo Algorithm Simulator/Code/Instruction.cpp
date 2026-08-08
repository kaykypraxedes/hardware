/* Instruction.cpp */
#include "headers/Instruction.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
std::vector<int> Instruction::base_ex_latencies
{
    0,  // INVALID
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

std::vector<int> Instruction::base_mem_latencies
{
    1,  // LOAD
    1   // STORE
};

// ─── HELPERS ───────────────────────────────────────────────────────
void FillCDB(
    CDB& cdb,
    const char reg_class,
    const int id_base,
    const int count,
    const int mask
){
    for (int i = 0; i < count; i++)
       cdb.registers.push_back(Register(reg_class, id_base + i, mask));
}

// Única validação de nome do sistema: aborta com o nome e a instrução que o gerou.
Register LookupRegister(
    const std::string& name,
    const std::string& context, // Puramente para debugging, mas não é obrigatório
    const std::unordered_map<std::string, Register>& table
){
    auto it = table.find(name);
    if (it == table.end()) {
        std::cerr << "[ERRO] Registrador inválido: '" << name << "' (instrução: " << context << ")\n";
        std::abort();
    }
    return it->second;
}

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
// Público:
void Instruction::Parse(
    const std::string& str
){
    // Verifica se foi passado uma string vazia.
    if (str.empty()) {
        std::cerr << "[ERRO] String vazia passada como instrução!\n";
        std::abort();
    }

    // Verifica se a instrução é suportada pela arquitetura.
    std::vector<std::string> tokens = SplitInstruction(str);

    // Verifica se os tokens passados são válidos.
    // - Pode não estar vazio, mas ser apenas um conjunto de espaços, vírgulas, etc.
    if (tokens.empty()) {
        std::cerr << "[ERRO] Nenhum token válido extraído: '" << str << "'\n";
        std::abort();
    }
    if (!IdentifyType(tokens)) {
        std::cerr << "[ERRO] Instrução não suportada por essa arquitetura: "
            << tokens[0] << "\n";
        std::abort();
    }

    // Termina de salvar suas informações.
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

// Métodos utilizados para settar um valor diferente do padrão de latência.
// - Simular um cache miss, por exemplo.

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
