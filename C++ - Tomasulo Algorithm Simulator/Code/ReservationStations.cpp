/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "header.h".
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
static std::vector<FU>& GetFUGroup(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_TYPE           type,
    const INSTRUCTION_PHASE_TOMASULO phase
) {
    // Retorna a referência ao grupo de FU correto para (tipo, fase).
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE)
        return (phase == INSTRUCTION_PHASE_TOMASULO::EX)
            ? fu.int_basic_alu
            : fu.memory_access;

    switch (type) {
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:
            return fu.int_mult_div_alu;
        case INSTRUCTION_TYPE::FLOAT_BASIC:
            return fu.float_basic_alu;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:
            return fu.float_mult_div_alu;
        default:
            return fu.int_basic_alu;
    }
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
bool RS::IsBusy()       const { return busy; }

// Público:
int RS::GetCountdown()  const { return allocation_countdown; }

// Público:
int RS::GetFUPosition() const { return fu_position; }

// Público:
std::size_t RS::GetCurrentStage() const { return current_stage; }

// Público:
INSTRUCTION_PHASE_TOMASULO RS::GetInstructionPhase()  const { return phase; }

// Público:
const std::string& RS::GetId()                        const { return id; }

// Público:
const std::vector<int>& RS::GetTimes()                const { return allocation_times; }

// Público:
const Instruction& RS::GetCurrentInstruction()        const { return *current_instruction; }

// Público:
const std::vector<std::string>& RS::GetInstructions() const { return allocated_instructions; }

// Público:
const std::vector<Register>& RS::GetExValues() const { return ex_V; }

// Público:
const std::vector<int>& RS::GetExDependencies() const { return ex_Q; }

// Público:
const std::vector<Register>& RS::GetMemValues() const { return mem_V; }

// Público:
const std::vector<int>& RS::GetMemDependencies() const { return mem_Q; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
RS::RS(
    const std::string& id
) :
    id(id){}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool RS::AddIssue(
    const std::shared_ptr<Instruction>& instruction,
    RegisterStatusTable&                register_status,
    const int                           cycle,
    const bool                          new_instruction,
    const std::size_t                   stage
){
    // RS ocupado.
    if (busy) return false;

    // A posição é a identidade lógica obrigatória dentro do pipeline.
    const int producer_position{instruction->GetPosition()};
    if (producer_position < 0) {
        std::cerr << "[ERRO] Instrução sem posição lógica válida no Issue.\n";
        std::abort();
    }

    if (stage >= instruction->GetStageCount()) {
        std::cerr <<
            "[ERRO] Etapa inválida passada para a RS.\n"
            "- Etapa: " << stage << '\n' <<
            "- Quantidade de etapas: " << instruction->GetStageCount() << '\n';
        std::abort();
    }

    // Aloca a nova instrução no RS.
    SetupNewIssue(instruction, cycle, stage);

    // Captura somente as fontes consumidas pela etapa atual.
    CaptureSources(
        instruction->GetExSourceRegisters(stage),
        ex_V,
        ex_Q,
        register_status,
        producer_position
    );
    CaptureSources(
        instruction->GetMemSourceRegisters(stage),
        mem_V,
        mem_Q,
        register_status,
        producer_position
    );

    // Destinos arquiteturais pertencem à primeira admissão lógica.
    if (new_instruction) {
        for (const Register& dest : instruction->GetDestRegisters()) {
            if (dest.GetType() == 'Z') continue;
            register_status.AllocateProducer(dest, producer_position, id, cycle);
        }
    }
    return true;
}

// Privado:
void RS::SetupNewIssue(
    const std::shared_ptr<Instruction>& instruction,
    const int                           cycle,
    const std::size_t                   stage
){
    // Aloca a instrução:
    // - Valores default.
    busy                 = true;
    current_instruction  = instruction;
    current_stage        = stage;
    phase                = INSTRUCTION_PHASE_TOMASULO::IS;
    allocation_countdown = -1;
    fu_position          = -1;

    // Marcação da nova alocação no histórico.
    allocated_instructions.push_back(current_instruction->GetInstructionString());
    allocation_times.push_back(cycle);
}

// Privado:
void RS::CaptureSources(
    const std::vector<Register>& sources,
    std::vector<Register>&       values,
    std::vector<int>&            dependencies,
    const RegisterStatusTable&   register_status,
    const int                    consumer_position
) {
    values.assign(sources.size(), Register{});
    dependencies.assign(sources.size(), -1);

    // Captura o produtor lógico anterior correto para cada fonte.
    for (std::size_t source{}; source < sources.size(); source++) {
        const Register& reference{sources[source]};
        if (reference.GetType() == 'Z') continue;

        const int producer_position{
            register_status.FindLatestProducerBefore(reference, consumer_position)
        };
        if (producer_position == -1 ||
            register_status.IsProducerResolved(reference, producer_position)) {
            values[source] = reference;
        }
        else dependencies[source] = producer_position;
    }
}

// Privado:
void RS::RefreshDependencyGroup(
    const std::vector<Register>& sources,
    std::vector<Register>&       values,
    std::vector<int>&            dependencies,
    const RegisterStatusTable&   register_status
) {
    // Produtores mais novos nunca substituem o Q capturado no Issue.
    for (std::size_t source{}; source < dependencies.size(); source++) {
        const int producer_position{dependencies[source]};
        if (producer_position == -1) continue;

        if (register_status.IsProducerResolved(sources[source], producer_position)) {
            values[source] = sources[source];
            dependencies[source] = -1;
        }
    }
}

// Público:
bool RS::UpdateDependencies(
    const RegisterStatusTable& register_status,
    FUNCTIONAL_UNITS&          fu,
    const int                  cycle
){
    // Se o RS estiver vazio ou com sua execução travada por dependencias.
    if (!busy || allocation_countdown != -1) return false;

    // Se já estiver na fase WR (-1, mas não atualiza mais).
    if (phase == INSTRUCTION_PHASE_TOMASULO::WR) return false;

    // Reconhece produtores concluídos mesmo quando o broadcast não alcançou a RS.
    RefreshDependencyGroup(
        current_instruction->GetExSourceRegisters(current_stage),
        ex_V,
        ex_Q,
        register_status
    );
    RefreshDependencyGroup(
        current_instruction->GetMemSourceRegisters(current_stage),
        mem_V,
        mem_Q,
        register_status
    );

    // Aloca as FUs a depender da necessidade e fase da instrução.
    return AdvancePhaseAllocation(fu, cycle);
}

// Privado:
bool RS::AdvancePhaseAllocation(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    INSTRUCTION_TYPE type{current_instruction->GetInstructionType(current_stage)};
    bool is_load_store{(type == INSTRUCTION_TYPE::LOAD) || (type == INSTRUCTION_TYPE::STORE)};

    // EX -> MEM:
    // - Apenas para LOAD e STORE.
    if (is_load_store && phase == INSTRUCTION_PHASE_TOMASULO::MEM && allocation_countdown == -1) {
        // Verifica se o dado está pronto (sem registrador esperando).
        for (const int producer_position : mem_Q)
            if (producer_position != -1) return false;

        return TryAllocateFU(
            fu,
            INSTRUCTION_PHASE_TOMASULO::MEM,
            cycle,
            current_instruction->GetMemLatency(current_stage)
        );
    }

    // IS -> EX:
    // - Para todos os tipos de instrução.
    if (is_load_store && phase != INSTRUCTION_PHASE_TOMASULO::IS) return false;

    // Instrução genérica:
    // Dependências não resolvidas genéricas que impedem o cálculo em EX.
    for (const int producer_position : ex_Q)
        if (producer_position != -1) return false;

    return TryAllocateFU(
        fu,
        INSTRUCTION_PHASE_TOMASULO::EX,
        cycle,
        current_instruction->GetExLatency(current_stage)
    );
}

// Privado:
bool RS::TryAllocateFU(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_PHASE_TOMASULO target_phase,
    const int                        cycle,
    const int                        latency
){
    // Verifica se a latência é válida.
    if (latency <= 0) {
        std::cerr <<
        "[ERRO] Latência inválida: " << latency << '\n' <<
        "- Instrução: " << current_instruction->GetInstructionString() << '\n' <<
        "- RS: " << id << '\n';
        std::abort();
    }

    // Procura uma unidade funcional livre.
    std::vector<FU>& fu_group{
        GetFUGroup(
            fu,
            current_instruction->GetInstructionType(current_stage),
            target_phase
        )
    };
    fu_position = FindFreeFU(fu_group);

    if (fu_position == -1) return false; // Não encontrou.

    // Aloca a FU:
    fu_group[fu_position].busy       = true;
    fu_group[fu_position].current_rs = id;
    fu_group[fu_position].allocated_rs.push_back(id);
    fu_group[fu_position].allocation_times.push_back(cycle);

    // Atualiza a fase (permanece igual se já era MEM) e define o tempo de alocação.
    phase = target_phase;
    allocation_countdown = latency;
    return true;
}

// Privado:
int RS::FindFreeFU(
    const std::vector<FU>& fu_group
) {
    // Se encontrar uma FU, aloca e retorna a sua posição.
    for (size_t i{}; i < fu_group.size(); i++) {
        if (!fu_group[i].busy) {
            return i;
        }
    }
    // Se não encontrar, retorna -1 (falhou).
    return -1;
}

// Público:
bool RS::UpdateCountdown(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    // RS vazio ou dependências impedindo a execução.
    if (!busy || allocation_countdown == -1) return false;

    allocation_countdown--;
    // Execução incompleta (faltam ciclos para acabar).
    if (allocation_countdown > 0) return false;

    // Instrução acabou de chegar no 0 da sua execução:
    INSTRUCTION_TYPE type{current_instruction->GetInstructionType(current_stage)};

    // Libera a unidade funcional que estava sendo usada.
    ReleaseFU(fu, phase, cycle);

    // Verifica o próximo estágio:
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // EX  -> MEM
        if (phase == INSTRUCTION_PHASE_TOMASULO::EX) phase = INSTRUCTION_PHASE_TOMASULO::MEM;
        // MEM -> WR
        else phase = INSTRUCTION_PHASE_TOMASULO::WR;
    }
    // EX -> WR
    else {
        phase = INSTRUCTION_PHASE_TOMASULO::WR;
    }
    allocation_countdown = -1;
    return true;
}

// Privado:
void RS::ReleaseFU(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_PHASE_TOMASULO finished_phase,
    const int                        cycle
){
    // A FU já estava desalocada (nada a fazer).
    if (fu_position == -1) return;

    std::vector<FU>& fu_group{
        GetFUGroup(
            fu,
            current_instruction->GetInstructionType(current_stage),
            finished_phase
        )
    };

    // Verifica se a posição da FU é válida:
    if (fu_position < -1 || fu_position >= static_cast<int>(fu_group.size())) {
        std::cerr <<
        "[ERRO] Posição inválida de fu: " << fu_position <<
        "- RS: " << id << "\n" <<
        "- Fase: " << static_cast<int>(finished_phase) << "\n";
        std::abort();
    }

    fu_group[fu_position].busy         = false;
    fu_group[fu_position].current_rs   = "";
    fu_group[fu_position].allocation_times.push_back(cycle);
    fu_position = -1;
}

// Público:
void RS::ResolveDependency(
    const int       producer_position,
    const Register& value
){
    ResolveDependencyInGroup(
        current_instruction->GetExSourceRegisters(current_stage),
        ex_V,
        ex_Q,
        producer_position,
        value
    );
    ResolveDependencyInGroup(
        current_instruction->GetMemSourceRegisters(current_stage),
        mem_V,
        mem_Q,
        producer_position,
        value
    );
}

// Privado:
void RS::ResolveDependencyInGroup(
    const std::vector<Register>& sources,
    std::vector<Register>&       values,
    std::vector<int>&            dependencies,
    const int                    producer_position,
    const Register&              value
) {
    for (std::size_t source{}; source < dependencies.size(); source++) {
        if (dependencies[source] != producer_position ||
            !SameReference(sources[source], value)) continue;
        values[source] = value;
        dependencies[source] = -1;
    }
}

// Privado:
bool RS::SameReference(
    const Register& left,
    const Register& right
) {
    return left.GetType() == right.GetType() &&
           left.GetId()   == right.GetId() &&
           left.GetMask() == right.GetMask();
}

// Público:
void RS::Release(
    const int cycle
){
    allocation_times.push_back(cycle);
    current_instruction.reset();
    current_stage = 0;
    ex_V.clear();
    ex_Q.clear();
    mem_V.clear();
    mem_Q.clear();
    busy                 = false;
    allocation_countdown = -1;
    fu_position          = -1;
    phase = INSTRUCTION_PHASE_TOMASULO::UNUSED;
}

} // namespace processor
