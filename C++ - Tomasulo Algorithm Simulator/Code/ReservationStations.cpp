/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "header.h".
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
static FUNCTIONAL_UNIT_GROUP GetFunctionalUnitGroup(
    const INSTRUCTION_TYPE           type,
    const INSTRUCTION_PHASE_TOMASULO phase
) {
    // Traduz a necessidade da RS para a identidade física aceita pelo banco.
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE)
        return (phase == INSTRUCTION_PHASE_TOMASULO::EX)
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
        default:
            return FUNCTIONAL_UNIT_GROUP::INT_BASIC;
    }
}

// Entrega um broadcast somente às células ocupadas de um grupo.
static void ResolveBroadcastInGroup(
    std::vector<RS>&     group,
    const CDB_BROADCAST& broadcast
) {
    for (RS& current : group) {
        if (!current.IsBusy()) continue;
        current.ResolveDependency(
            broadcast.producer_position,
            broadcast.destination
        );
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

    // Solicita ao banco uma unidade do grupo correspondente.
    fu_position = fu.Allocate(
        GetFunctionalUnitGroup(
            current_instruction->GetInstructionType(current_stage),
            target_phase
        ),
        id,
        cycle
    );

    if (fu_position == -1) return false; // Não encontrou.

    // Atualiza a fase (permanece igual se já era MEM) e define o tempo de alocação.
    phase = target_phase;
    allocation_countdown = latency;
    return true;
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

    // O banco valida a posição e a associação antes de finalizar a timeline.
    fu.Release(
        GetFunctionalUnitGroup(
            current_instruction->GetInstructionType(current_stage),
            finished_phase
        ),
        fu_position,
        id,
        cycle
    );
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


// ==================================================================
// === RESERVATION_STATION ==========================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────
// Público:
RESERVATION_STATION::RESERVATION_STATION(
    const std::vector<int>& capacities
) {
    if (capacities.size() != 6) {
        std::cerr <<
            "[ERRO] Quantidade inválida de RSs: " << capacities.size() << '\n';
        std::abort();
    }

    const std::vector<std::string> names{
        "load",
        "store",
        "int_basic",
        "int_mult_div",
        "float_basic",
        "float_mult_div"
    };
    const std::vector<std::vector<RS>*> groups{GetGroups()};
    for (std::size_t group{}; group < groups.size(); group++)
        for (int position{}; position < capacities[group]; position++)
            groups[group]->push_back(RS(names[group] + std::to_string(position)));
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
const std::vector<RS>& RESERVATION_STATION::GetLoadStations() const {
    return load;
}

// Público:
const std::vector<RS>& RESERVATION_STATION::GetStoreStations() const {
    return store;
}

// Público:
const std::vector<RS>& RESERVATION_STATION::GetIntBasicStations() const {
    return int_basic;
}

// Público:
const std::vector<RS>& RESERVATION_STATION::GetIntMultDivStations() const {
    return int_mult_div;
}

// Público:
const std::vector<RS>& RESERVATION_STATION::GetFloatBasicStations() const {
    return float_basic;
}

// Público:
const std::vector<RS>& RESERVATION_STATION::GetFloatMultDivStations() const {
    return float_mult_div;
}

// Privado:
std::vector<RS>& RESERVATION_STATION::GetGroupForType(
    const INSTRUCTION_TYPE type
) {
    switch (type) {
        case INSTRUCTION_TYPE::LOAD: return load;
        case INSTRUCTION_TYPE::STORE: return store;
        case INSTRUCTION_TYPE::FLOAT_BASIC: return float_basic;
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV: return int_mult_div;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV: return float_mult_div;
        default: return int_basic;
    }
}

// Privado:
std::vector<std::vector<RS>*> RESERVATION_STATION::GetGroups() {
    return {
        &load,
        &store,
        &int_basic,
        &int_mult_div,
        &float_basic,
        &float_mult_div
    };
}

// Privado:
std::vector<const std::vector<RS>*> RESERVATION_STATION::GetGroups() const {
    return {
        &load,
        &store,
        &int_basic,
        &int_mult_div,
        &float_basic,
        &float_mult_div
    };
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool RESERVATION_STATION::AddIssue(
    const std::shared_ptr<Instruction>& instruction,
    RegisterStatusTable&                register_status,
    const int                           cycle,
    const bool                          new_instruction,
    const std::size_t                   stage
) {
    if (stage >= instruction->GetStageCount()) {
        std::cerr <<
            "[ERRO] Etapa inválida passada para o banco de RS.\n" <<
            "- Etapa: " << stage << '\n' <<
            "- Quantidade de etapas: " << instruction->GetStageCount() << '\n';
        std::abort();
    }

    const int position{instruction->GetPosition()};
    if (IsPositionAllocated(position)) {
        std::cerr << "[ERRO] Posição já alocada em uma RS: " << position << '\n';
        std::abort();
    }

    std::vector<RS>& group{
        GetGroupForType(instruction->GetInstructionType(stage))
    };
    for (RS& station : group)
        if (station.AddIssue(
            instruction,
            register_status,
            cycle,
            new_instruction,
            stage
        )) return true;
    return false;
}

// Público:
bool RESERVATION_STATION::IsPositionAllocated(
    const int position
) const {
    for (const std::vector<RS>* group : GetGroups())
        for (const RS& station : *group)
            if (station.IsBusy() &&
                station.GetCurrentInstruction().GetPosition() == position) return true;
    return false;
}

// Público:
void RESERVATION_STATION::ReleaseByPosition(
    const int position,
    const int cycle
) {
    for (std::vector<RS>* group : GetGroups()) {
        for (RS& station : *group) {
            if (!station.IsBusy() ||
                station.GetCurrentInstruction().GetPosition() != position) continue;
            station.Release(cycle);
            return;
        }
    }

    std::cerr <<
        "[ERRO] RS da instrução não encontrada para liberação.\n" <<
        "- Posição: " << position << '\n' <<
        "- Ciclo: " << cycle << '\n';
    std::abort();
}

// Público:
std::vector<RS*> RESERVATION_STATION::CollectReadyCandidates() {
    std::vector<RS*> candidates;
    for (std::vector<RS>* group : GetGroups())
        for (RS& station : *group)
            if (station.IsBusy() &&
                station.GetCountdown() == -1 &&
                station.GetInstructionPhase() != INSTRUCTION_PHASE_TOMASULO::WR)
                candidates.push_back(&station);

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [](const RS* left, const RS* right) {
            return left->GetCurrentInstruction().GetPosition() <
                   right->GetCurrentInstruction().GetPosition();
        }
    );
    return candidates;
}

// Público:
std::vector<RS*> RESERVATION_STATION::CollectBusyStations() {
    std::vector<RS*> stations;
    for (std::vector<RS>* group : GetGroups())
        for (RS& station : *group)
            if (station.IsBusy()) stations.push_back(&station);
    return stations;
}

// Público:
void RESERVATION_STATION::ResolveBroadcast(
    const CDB_BROADCAST& broadcast
) {
    for (std::vector<RS>* group : GetGroups())
        ResolveBroadcastInGroup(*group, broadcast);
}

} // namespace processor
