/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
static bool IsInvalidRegister(const Register& reg) {
    return reg.GetType() == 'Z'; // É inválido se for igual.
}

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
const std::vector<std::pair<std::string,int>>& RS::GetExQ()  const { return ex_Q; }

// Público:
const std::vector<std::pair<std::string,int>>& RS::GetMemQ() const { return mem_Q; }

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
    CDB&                                cdb,
    const int                           cycle
){
    // RS ocupado.
    if (busy) return false;

    // Aloca a nova instrução no RS.
    SetupNewIssue(instruction, cycle);

    // Define V[i] e Q[i] para cada fonte da instrução.
    std::vector<Register> sources{instruction->GetExSourceRegisters()};
    for (size_t i{}; i < sources.size(); i++)
        ReadSourceOperand(false, i, sources[i], cdb);
    sources = instruction->GetMemSourceRegisters();
    for (size_t i{}; i < sources.size(); i++)
        ReadSourceOperand(true, i, sources[i], cdb);

    // Marca os registradores destino no CDB (pode haver mais de um, ex.: x86 reg + EFLAGS).
    AllocateDestInCDB(instruction->GetDestRegisters(), cdb, cycle);
    return true;
}

// Privado:
void RS::SetupNewIssue(
    const std::shared_ptr<Instruction>& instruction,
    const int                           cycle
){
    // Aloca a instrução:
    // - Valores default.
    busy                 = true;
    current_instruction  = instruction;
    phase                = INSTRUCTION_PHASE_TOMASULO::IS;
    allocation_countdown = -1;
    fu_position          = -1;

    // Insere os V e Q da instrução.
    // - Os vetores estão vazios por padrão e o espaço é alocado com a necessidade.
    size_t sources_size{current_instruction->GetExSourceRegisters().size()};
    ex_V.assign(sources_size, Register{});
    ex_Q.assign(sources_size, {"", -1});
    sources_size = current_instruction->GetMemSourceRegisters().size();
    mem_V.assign(sources_size, Register{});
    mem_Q.assign(sources_size, {"", -1});

    // Marcação da nova alocação no histórico.
    allocated_instructions.push_back(current_instruction->GetInstructionString());
    allocation_times.push_back(cycle);
}

// Privado:
void RS::ReadSourceOperand(
    const bool      is_mem,
    const size_t    idx,
    const Register& src,
    CDB&            cdb
){
    // Não tem fonte.
    if (IsInvalidRegister(src)) return;

    // Acessa o registrador alvo no CDB e verifica se ele está com uma dependencia atualmente.
    const Register& regCDB{GetReg(cdb, src)};
    const std::string& tag{regCDB.GetCurrentRS()};
    Register&                    V_idx{is_mem ? mem_V[idx] : ex_V[idx]};
    std::pair<std::string, int>& Q_idx{is_mem ? mem_Q[idx] : ex_Q[idx]};

    // Define o V ou o Q a depender do estado da alocação:
    // 1. Sem resultado pendente.
    if (tag.empty()) V_idx = src;
    // 2. Resultado pendente.
    else Q_idx = {tag, regCDB.GetRSCycleStart(tag)};
}

// Privado:
void RS::AllocateDestInCDB(
    const std::vector<Register>& dests,
    CDB&                         cdb,
    const int                    cycle
){
    for (const Register& dest : dests) {
        // Não tem destino.
        if (IsInvalidRegister(dest)) continue;

        GetReg(cdb, dest).AllocateRS(id, cycle);
    }
}

// Público:
bool RS::UpdateDependencies(
    CDB&              cdb,
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    // Se o RS estiver vazio ou com sua execução travada por dependencias.
    if (!busy || allocation_countdown != -1) return false;

    // Se já estiver na fase WR (-1, mas não atualiza mais).
    if (phase == INSTRUCTION_PHASE_TOMASULO::WR) return false;

    // Verifica se os Q[i] já desapareceram (se sim, marca o V[i] correspondente).
    for (size_t i{}; i < ex_Q.size(); i++)
        CheckDependency(false, i, cdb);
    for (size_t i{}; i < mem_Q.size(); i++)
        CheckDependency(true, i, cdb);

    // Aloca as FUs a depender da necessidade e fase da instrução.
    return AdvancePhaseAllocation(fu, cycle);
}

// Privado:
void RS::CheckDependency(
    const bool   is_mem,
    const size_t idx,
    CDB&         cdb
){
    // Define os parâmetros da atualização:
    // - Registrador fonte da posição "idx" da instrução.
    const Register& reg{
        is_mem ?
        current_instruction->GetMemSourceRegisters()[idx] :
        current_instruction->GetExSourceRegisters()[idx]
    };
    // - V[idx]/Q[idx] atuais.
    Register& V_idx{is_mem ? mem_V[idx] : ex_V[idx]};
    std::pair<std::string, int>& Q_idx{is_mem ? mem_Q[idx] : ex_Q[idx]};

    // Já existe um V ou se Q já foi resolvido.
    if (V_idx.GetType() != 'Z' || Q_idx.first.empty()) return;

    Register& regCDB{GetReg(cdb, reg)};

    // Verifica se já resolveu nesse cíclo para atualizar.
    if (regCDB.IsDependencyResolved(Q_idx.first, Q_idx.second)) {
        // Mantém o Q_idx para não atrapalhar a organização:
        // - V[i] ainda é correspondente a Q[i], não a Q[i-1].
        V_idx = reg;
        Q_idx = {"", -1};
    }
}

// Privado:
bool RS::AdvancePhaseAllocation(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    INSTRUCTION_TYPE type{current_instruction->GetInstructionType()};
    bool is_load_store{(type == INSTRUCTION_TYPE::LOAD) || (type == INSTRUCTION_TYPE::STORE)};

    // EX -> MEM:
    // - Apenas para LOAD e STORE.
    if (is_load_store && phase == INSTRUCTION_PHASE_TOMASULO::MEM && allocation_countdown == -1) {
        // Verifica se o dado está pronto (sem registrador esperando).
        for(const auto& q : mem_Q){
            if (!q.first.empty()) return false;
        }

        return TryAllocateFU(fu, INSTRUCTION_PHASE_TOMASULO::MEM, cycle, current_instruction->GetMemLatency());
    }

    // IS -> EX:
    // - Para todos os tipos de instrução.
    if (is_load_store && phase != INSTRUCTION_PHASE_TOMASULO::IS) return false;

    // Instrução genérica:
    // Dependências não resolvidas genéricas que impedem o cálculo em EX.
    for (const auto& q : ex_Q){
        if (!q.first.empty()) return false;
    }

    return TryAllocateFU(fu, INSTRUCTION_PHASE_TOMASULO::EX, cycle, current_instruction->GetExLatency());
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
    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction->GetInstructionType(), target_phase)};
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
    INSTRUCTION_TYPE type{current_instruction->GetInstructionType()};

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

    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction->GetInstructionType(), finished_phase)};

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
    const std::string& rs_id,
    const Register&    value
){
    // Não dá Q[i].erase(i) para manter a posição equivalente entre Vn e Qn.
    // - Se fosse removido o Qj, o Qk iria para a posição 0 e o Vk (V[1]) não o acharia.
    for (size_t i{}; i < ex_Q.size(); i++)
        if (ex_Q[i].first == rs_id) { ex_V[i] = value; ex_Q[i] = {"", -1}; }

    for (size_t i{}; i < mem_Q.size(); i++)
        if (mem_Q[i].first == rs_id) { mem_V[i] = value; mem_Q[i] = {"", -1}; }
}

// Público:
void RS::Release(
    const int cycle
){
    allocation_times.push_back(cycle);
    busy                 = false;
    allocation_countdown = -1;
    fu_position          = -1;
    // Vetores empty.
    ex_V.clear();
    ex_Q.clear();
    mem_V.clear();
    mem_Q.clear();
    phase = INSTRUCTION_PHASE_TOMASULO::UNUSED;
}

} // namespace processor
