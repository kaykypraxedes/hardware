/* Instruction.cpp */
#include "headers/Architecture.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
void FillRegisterLayout(
    REGISTER_LAYOUT& layout,
    const char       reg_class,
    const int        id_base,
    const int        count,
    const int        mask
){
    for (int i{}; i < count; i++)
        layout.references.push_back(Register(reg_class, id_base + i, mask));
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

int GetExecutionLatency(
    const EXECUTION_LATENCIES& latencies,
    const INSTRUCTION_TYPE     type
) {
    switch (type) {
        case INSTRUCTION_TYPE::INVALID: return latencies.invalid;
        case INSTRUCTION_TYPE::LOAD: return latencies.load;
        case INSTRUCTION_TYPE::STORE: return latencies.store;
        case INSTRUCTION_TYPE::BRANCH: return latencies.branch;
        case INSTRUCTION_TYPE::INT_BASIC: return latencies.int_basic;
        case INSTRUCTION_TYPE::INT_MUL: return latencies.int_mult;
        case INSTRUCTION_TYPE::INT_DIV: return latencies.int_div;
        case INSTRUCTION_TYPE::FLOAT_BASIC: return latencies.float_basic;
        case INSTRUCTION_TYPE::FLOAT_MUL: return latencies.float_mult;
        case INSTRUCTION_TYPE::FLOAT_DIV: return latencies.float_div;
    }
    std::cerr << "[ERRO] Tipo de instrução inválido para latência EX.\n";
    std::abort();
}

int GetMemoryLatency(
    const MEMORY_LATENCIES& latencies,
    const INSTRUCTION_TYPE  type
) {
    switch (type) {
        case INSTRUCTION_TYPE::LOAD: return latencies.load;
        case INSTRUCTION_TYPE::STORE: return latencies.store;
        default: return 0;
    }
}

RESERVATION_STATION_GROUP GetReservationStationGroup(
    const INSTRUCTION_TYPE type
) {
    switch (type) {
        case INSTRUCTION_TYPE::LOAD: return RESERVATION_STATION_GROUP::LOAD;
        case INSTRUCTION_TYPE::STORE: return RESERVATION_STATION_GROUP::STORE;
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:
            return RESERVATION_STATION_GROUP::INT_MULT_DIV;
        case INSTRUCTION_TYPE::FLOAT_BASIC:
            return RESERVATION_STATION_GROUP::FLOAT_BASIC;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:
            return RESERVATION_STATION_GROUP::FLOAT_MULT_DIV;
        case INSTRUCTION_TYPE::BRANCH:
        case INSTRUCTION_TYPE::INT_BASIC:
            return RESERVATION_STATION_GROUP::INT_BASIC;
        case INSTRUCTION_TYPE::INVALID:
            break;
    }
    std::cerr << "[ERRO] Tipo de instrução inválido para grupo de RS.\n";
    std::abort();
}

FUNCTIONAL_UNIT_GROUP GetFunctionalUnitGroup(
    const INSTRUCTION_TYPE           type,
    const INSTRUCTION_PHASE_TOMASULO phase
) {
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE)
        return phase == INSTRUCTION_PHASE_TOMASULO::EX
            ? FUNCTIONAL_UNIT_GROUP::INT_BASIC
            : FUNCTIONAL_UNIT_GROUP::MEMORY_ACCESS;

    switch (type) {
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:
            return FUNCTIONAL_UNIT_GROUP::INT_MULT_DIV;
        case INSTRUCTION_TYPE::FLOAT_BASIC:
            return FUNCTIONAL_UNIT_GROUP::FLOAT_BASIC;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:
            return FUNCTIONAL_UNIT_GROUP::FLOAT_MULT_DIV;
        case INSTRUCTION_TYPE::BRANCH:
        case INSTRUCTION_TYPE::INT_BASIC:
            return FUNCTIONAL_UNIT_GROUP::INT_BASIC;
        default:
            break;
    }
    std::cerr << "[ERRO] Tipo de instrução inválido para grupo de FU.\n";
    std::abort();
}

void ValidatePipelineConfiguration(
    const PIPELINE_CONFIGURATION& configuration
) {
    const RESERVATION_STATION_CAPACITIES& rs{configuration.reservation_station_capacities};
    const FUNCTIONAL_UNIT_CAPACITIES& fu{configuration.functional_unit_capacities};
    const EXECUTION_LATENCIES& ex{configuration.execution_latencies};
    const MEMORY_LATENCIES& mem{configuration.memory_latencies};

    if (rs.load < 0 || rs.store < 0 || rs.int_basic < 0 ||
        rs.int_mult_div < 0 || rs.float_basic < 0 || rs.float_mult_div < 0 ||
        fu.memory_access < 0 || fu.int_basic < 0 || fu.int_mult_div < 0 ||
        fu.float_basic < 0 || fu.float_mult_div < 0) {
        std::cerr << "[ERRO] Capacidades de RS e FU não podem ser negativas.\n";
        std::abort();
    }
    if (ex.invalid != 0 || ex.load <= 0 || ex.store <= 0 || ex.branch <= 0 ||
        ex.int_basic <= 0 || ex.int_mult <= 0 || ex.int_div <= 0 ||
        ex.float_basic <= 0 || ex.float_mult <= 0 || ex.float_div <= 0 ||
        mem.load <= 0 || mem.store <= 0) {
        std::cerr << "[ERRO] Latências da configuração são inválidas.\n";
        std::abort();
    }
    if (configuration.issue_width <= 0 || configuration.write_result_width <= 0 ||
        configuration.commit_width <= 0 || configuration.rob_capacity < 0) {
        std::cerr << "[ERRO] Larguras e capacidade do ROB são inválidas.\n";
        std::abort();
    }
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Instruction::GetPosition()   const { return position; }

// Público:
int Instruction::GetExLatency()  const {
    return GetExLatency(0);
}

// Público:
int Instruction::GetMemLatency() const {
    return GetMemLatency(0);
}

// Público:
INSTRUCTION_TYPE Instruction::GetInstructionType() const {
    return GetInstructionType(0);
}

// Público:
std::size_t Instruction::GetStageCount() const {
    ValidateStageVectors();
    return instruction_types.size();
}

// Público:
INSTRUCTION_TYPE Instruction::GetInstructionType(
    const std::size_t stage
) const {
    ValidateStageIndex(stage);
    return instruction_types[stage];
}

// Público:
int Instruction::GetExLatency(
    const std::size_t stage
) const {
    ValidateStageIndex(stage);
    return ex_latencies[stage];
}

// Público:
int Instruction::GetMemLatency(
    const std::size_t stage
) const {
    ValidateStageIndex(stage);
    return mem_latencies[stage];
}

// Público:
const std::string& Instruction::GetInstructionString()            const { return instruction_string; }

// Público:
const std::vector<Register>& Instruction::GetDestRegisters()      const { return dest_registers; }

// Público:
const std::vector<Register>& Instruction::GetExSourceRegisters()  const {
    return GetExSourceRegisters(0);
}

// Público:
const std::vector<Register>& Instruction::GetMemSourceRegisters() const {
    return GetMemSourceRegisters(0);
}

// Público:
const std::vector<Register>& Instruction::GetExSourceRegisters(
    const std::size_t stage
) const {
    ValidateStageIndex(stage);
    return ex_source_registers[stage];
}

// Público:
const std::vector<Register>& Instruction::GetMemSourceRegisters(
    const std::size_t stage
) const {
    ValidateStageIndex(stage);
    return mem_source_registers[stage];
}

// Público:
const std::vector<INSTRUCTION_TYPE>& Instruction::GetInstructionTypes() const {
    ValidateStageVectors();
    return instruction_types;
}

// Público:
const std::vector<int>& Instruction::GetExLatencies() const {
    ValidateStageVectors();
    return ex_latencies;
}

// Público:
const std::vector<int>& Instruction::GetMemLatencies() const {
    ValidateStageVectors();
    return mem_latencies;
}

// Público:
const std::vector<std::vector<Register>>& Instruction::GetAllExSourceRegisters() const {
    ValidateStageVectors();
    return ex_source_registers;
}

// Público:
const std::vector<std::vector<Register>>& Instruction::GetAllMemSourceRegisters() const {
    ValidateStageVectors();
    return mem_source_registers;
}

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
    const std::string&            str,
    const PIPELINE_CONFIGURATION& configuration
){
    // Limpa qualquer dado de instrução que poderia existir.
    // - Reutilização de instruções em contextos como reescrita de registradores.
    instruction_string.clear();
    instruction_types.clear();
    ex_latencies.clear();
    mem_latencies.clear();
    dest_registers.clear();
    ex_source_registers.clear();
    mem_source_registers.clear();

    // Verifica se foi passado uma string vazia.
    if (str.empty()) {
        std::cerr << "[ERRO] String vazia passada como instrução!\n";
        std::abort();
    }
    instruction_string = str;

    std::vector<std::string> tokens{SplitInstruction(str)};

    // Verifica se os tokens passados são válidos.
    // - Pode não estar vazio, mas ser apenas um conjunto de espaços, vírgulas, etc.
    if (tokens.empty()) {
        std::cerr <<
            "[ERRO] Nenhum token válido extraído!\n"
            "- Instrução: " << str << '\n';
        std::abort();
    }
    if (!SetStages(tokens)) {
        std::cerr << "[ERRO] Instrução não suportada por essa arquitetura: " << tokens[0] << '\n';
        std::abort();
    }
    MaterializeLatencies(configuration);
    ValidateStageVectors();
    ValidateLatencies(ex_latencies, mem_latencies);
    NormalizeInstruction(tokens);
}

// Público:
void Instruction::SetLatencies(
    const std::vector<int>& new_ex_latencies,
    const std::vector<int>& new_mem_latencies
) {
    ValidateStageVectors();

    // Confirma o contrato do override antes de montar o estado efetivo.
    if (new_ex_latencies.size() != instruction_types.size() ||
        new_mem_latencies.size() != instruction_types.size()) {
        std::cerr << "[ERRO] Quantidade de latências diferente da quantidade de etapas.\n";
        std::abort();
    }

    // Zero em MEM preserva a latência já materializada pela configuração.
    std::vector<int> effective_mem_latencies;
    effective_mem_latencies.reserve(instruction_types.size());
    for (std::size_t i{}; i < instruction_types.size(); i++) {
        const int override_latency{new_mem_latencies[i]};
        effective_mem_latencies.push_back(
            override_latency == 0 ? mem_latencies[i] : override_latency
        );
    }

    // Valida o plano completo antes de modificar qualquer vetor interno.
    ValidateLatencies(new_ex_latencies, effective_mem_latencies);
    ex_latencies = new_ex_latencies;
    mem_latencies = effective_mem_latencies;
}

// Protegido:
void Instruction::AddStage(
    const INSTRUCTION_TYPE       instruction_type,
    const std::vector<Register>& ex_sources,
    const std::vector<Register>& mem_sources
){
    if (instruction_type == INSTRUCTION_TYPE::INVALID) {
        std::cerr << "[ERRO] Não é possível adicionar uma etapa inválida.\n";
        std::abort();
    }

    instruction_types.push_back(instruction_type);
    ex_latencies.push_back(0);
    mem_latencies.push_back(0);
    ex_source_registers.push_back(ex_sources);
    mem_source_registers.push_back(mem_sources);
}

// Privado:
void Instruction::MaterializeLatencies(
    const PIPELINE_CONFIGURATION& configuration
) {
    ValidatePipelineConfiguration(configuration);
    for (std::size_t stage{}; stage < instruction_types.size(); stage++) {
        ex_latencies[stage] = GetExecutionLatency(
            configuration.execution_latencies,
            instruction_types[stage]
        );
        mem_latencies[stage] = GetMemoryLatency(
            configuration.memory_latencies,
            instruction_types[stage]
        );
    }
}

// Privado:
void Instruction::ValidateStageVectors() const {
    const std::size_t stage_count{instruction_types.size()};
    if (stage_count == 0 ||
        ex_latencies.size()         != stage_count ||
        mem_latencies.size()        != stage_count ||
        ex_source_registers.size()  != stage_count ||
        mem_source_registers.size() != stage_count) {
        std::cerr << "[ERRO] Vetores de etapas da instrução estão desalinhados.\n";
        std::abort();
    }
}

// Privado:
void Instruction::ValidateStageIndex(
    const std::size_t stage
) const {
    ValidateStageVectors();
    if (stage >= instruction_types.size()) {
        std::cerr <<
            "[ERRO] Índice de etapa fora da descrição da instrução.\n"
            "- Índice: " << stage << '\n' <<
            "- Quantidade de etapas: " << instruction_types.size() << '\n';
        std::abort();
    }
}

// Privado:
void Instruction::ValidateLatencies(
    const std::vector<int>& ex_values,
    const std::vector<int>& mem_values
) const {
    if (ex_values.size() != instruction_types.size() ||
        mem_values.size() != instruction_types.size()) {
        std::cerr << "[ERRO] Vetores de latências desalinhados com as etapas.\n";
        std::abort();
    }

    for (std::size_t i{}; i < instruction_types.size(); i++) {
        if (ex_values[i] <= 0) {
            std::cerr << "[ERRO] Latência de EX deve ser positiva.\n";
            std::abort();
        }

        const bool uses_memory{
            instruction_types[i] == INSTRUCTION_TYPE::LOAD ||
            instruction_types[i] == INSTRUCTION_TYPE::STORE
        };
        if ((uses_memory && mem_values[i] <= 0) ||
            (!uses_memory && mem_values[i] != 0)) {
            std::cerr << "[ERRO] Latência de MEM incompatível com o tipo da etapa.\n";
            std::abort();
        }
    }
}

} // namespace processor
