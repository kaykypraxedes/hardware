/* Instruction.cpp */
#include "headers/Architecture.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
void FillCDB(
    CDB& cdb,
    const char reg_class,
    const int  id_base,
    const int  count,
    const int  mask
){
    for (int i{}; i < count; i++)
        cdb.registers.push_back(Register(reg_class, id_base + i, mask));
}

bool IsRegister(
    const std::string&                               token,
    const std::unordered_map<std::string, Register>& table
){
    std::string lower{token};
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    // unordered_map::count() retorna a quantidade de elementos com aquela chave:
    // - Se for um registrador, irá retornar 1 (> 0 => true), se não, retorna 0 (false).
    return table.count(lower) > 0;
}

bool ContainsOpcode(
    const std::vector<std::string>& vec,
    const std::string&              op
){
    return std::find(vec.begin(), vec.end(), op) != vec.end();
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

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
const std::string& Instruction::GetInstructionString()            const { return instruction_string; }

// Público:
const std::vector<Register>& Instruction::GetDestRegisters()      const { return dest_registers; }

// Público:
const std::vector<Register>& Instruction::GetExSourceRegisters()  const { return ex_source_registers; }

// Público:
const std::vector<Register>& Instruction::GetMemSourceRegisters() const { return mem_source_registers; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Instruction::Instruction(
    const int position
) :
    position(position)
{
    if (position < -1) {
        std::cerr << "[ERRO] Valor inválido de posição: " << position << '\n';
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

    std::vector<std::string> tokens{SplitInstruction(str)};

    // Verifica se os tokens passados são válidos.
    // - Pode não estar vazio, mas ser apenas um conjunto de espaços, vírgulas, etc.
    if (tokens.empty()) {
        std::cerr <<
            "[ERRO] Nenhum token válido extraído!\n"
            "- Instrução: " << str << '\n';
        std::abort();
    }
    if (!IdentifyType(tokens)) {
        std::cerr << "[ERRO] Instrução não suportada por essa arquitetura: " << tokens[0] << '\n';
        std::abort();
    }

    // Camada de validação:
    // - Verifica se a sintaxe da instrução está correta.
    // - Busca reduzida por restringir o tipo com "IndentifyType()".
    ValidateInstruction(tokens);

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
    // "enums" tradicionais (sem um valor atribuido) podem ser convertidos para int (ordem -> valor int).
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
