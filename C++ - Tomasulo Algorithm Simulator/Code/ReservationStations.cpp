/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static bool InvalidRegister(const Register& reg) {
    return reg.GetType() == 'Z'; // É inválido se for igual.
}

// Retorna a posição da FU que vai ser alocada (-1 se não encontrar).
static int AllocateFreeFU(
    std::vector<FU>&   group,
    const int          cycle,
    const std::string& id
) {
    for (size_t i = 0; i < group.size(); i++) {
        if (!group[i].busy) {
            group[i].busy       = true;
            group[i].current_rs = id;
            group[i].allocated_rs.push_back(id);
            group[i].allocation_times.push_back(cycle);
            return i;
        }
    }
    return -1;
}

// Desaloca a FU na posição dentro do grupo (retorna false se a posição for inválida).
static bool DeallocateFU(
    std::vector<FU>& group,
    const int        position,
    const int        cycle
) {
    if (position < 0 || position >= static_cast<int>(group.size())) return false;
    group[position].busy         = false;
    group[position].current_rs   = "";
    group[position].allocation_times.push_back(cycle);
    return true;
}

// Retorna a referência ao grupo de FU correto para (tipo, fase).
static std::vector<FU>& GetFUGroup(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_TYPE           type,
    const INSTRUCTION_PHASE_TOMASULO phase
) {
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

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
bool ReservationStation::IsBusy()       const { return busy; }

// Público:
int ReservationStation::GetCountdown()  const { return allocation_countdown; }

// Público:
int ReservationStation::GetFUPosition() const { return fu_position; }

// Público:
INSTRUCTION_PHASE_TOMASULO ReservationStation::GetInstructionPhase() const { return phase; }

// Público:
const std::string& ReservationStation::GetId() const { return id; }

// Público:
// Qj = Q[0] (se existir).
std::string ReservationStation::GetQj() const { return Q.empty() ? "" : Q[0].first; }

// Público:
// Qk = Q[1] (se existir).
std::string ReservationStation::GetQk() const { return Q.size() < 2 ? "" : Q[1].first; }

// Público:
const std::vector<int>& ReservationStation::GetTimes()                const { return allocation_times; }

// Público:
const Instruction& ReservationStation::GetCurrentInstruction()        const { return *current_instruction; }

// Público:
const std::vector<std::string>& ReservationStation::GetInstructions() const { return allocated_instructions; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
ReservationStation::ReservationStation(
    const std::string& id
) :
    id(id){}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool ReservationStation::AddIssue(
    const std::shared_ptr<Instruction>& instruction,
    CDB&               cdb,
    const int          cycle
){
    // RS ocupado.
    if (busy) return false;

    // Aloca a nova instrução no RS.
    // - Passa suas informações básicas.
    SetupNewIssue(instruction, cycle);

    // Define V[i] e Q[i] para cada fonte da instrução.
    const std::vector<Register>& sources = instruction->GetSourceRegisters();
    for (size_t i = 0; i < sources.size(); i++)
        ReadSourceOperand(i, sources[i], cdb);

    // Marca os registradores destino no CDB (pode haver mais de um, ex.: x86 reg + EFLAGS).
    AllocateDestInCDB(instruction->GetDestRegisters(), cdb, cycle);
    return true;
}

// Privado:
// Apenas faz a limpeza e redefinição dos dadospara o novo issue (e marca a nova alocação nos vetores).
void ReservationStation::SetupNewIssue(
    const std::shared_ptr<Instruction>& instruction,
    const int                           cycle
){
    // Aloca a instrução.
    busy                 = true;
    current_instruction  = instruction;
    phase                = INSTRUCTION_PHASE_TOMASULO::IS;
    // Valores default:
    allocation_countdown = -1;
    fu_position          = -1;

    // Insere os V e Q da instrução.
    // - Os vetores estão vazios por padrão e o espaço é alocado com a necessidade:
    // - Apenas Vj: V.size() == 1; Vj e Vk: V.size() == 2;
    V.assign(current_instruction->GetSourceRegisters().size(), Register{});
    Q.assign(current_instruction->GetSourceRegisters().size(), {"", -1});

    // Marcação da nova alocação no histórico.
    allocated_instructions.push_back(current_instruction->GetInstructionString());
    allocation_times.push_back(cycle);
}

// Privado:
// Faz a leitura efetiva do registrador da fonte 'idx'
// - Verifica se a alocação é em V[idx] (dado pronto) ou em Q[idx] (dependente).
void ReservationStation::ReadSourceOperand(
    const size_t    idx,
    const Register& src,
    const CDB&      cdb
){
    // Não tem fonte.
    if (InvalidRegister(src)) return;

    // Acessa diretamente o registrador alvo dentro do CDB.
    Register regCDB = (src.GetType() == 'F')
        ? cdb.F[src.GetId()]
        : cdb.R[src.GetId()];
    // Verifica se ele está com uma dependencia atualmente.
    std::string tag = regCDB.GetCurrentRS();
    Register&                    V_idx = V[idx];
    std::pair<std::string, int>& Q_idx = Q[idx];
    // Define o V ou o Q a depender do estado da alocação:
    // 1. Sem resultado pendente.
    if (tag.empty()) V_idx = src;
    // 2. Resultado pendente.
    else Q_idx = {tag, regCDB.GetRSCycleStart(tag)};
}

// Privado:
// Marca no CDB todos os registradores de destino da instrução como pendentes desta RS.
void ReservationStation::AllocateDestInCDB(
    const std::vector<Register>& dests,
    CDB&            cdb,
    const int       cycle
){
    for (const Register& dest : dests) {
        // Não tem destino.
        if (InvalidRegister(dest)) continue;

        if (dest.GetType() == 'F') cdb.F[dest.GetId()].AllocateRS(id, cycle);
        else                       cdb.R[dest.GetId()].AllocateRS(id, cycle);
    }
}

// Público:
bool ReservationStation::UpdateDependencies(
    CDB&              cdb,
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    // Se o RS estiver vazio ou com sua execução travada por dependencias.
    if (!busy || allocation_countdown != -1) return false;
    // Se já estiver na fase WR (-1, mas não atualiza mais).
    if (phase == INSTRUCTION_PHASE_TOMASULO::WR) return false;

    // Verifica se os Q[i] já desapareceram (se sim, marca o V[i] correspondente).
    for (size_t i = 0; i < Q.size(); i++)
        CheckDependency(i, cdb);

    // Aloca as FUs a depender da necessidade e fase da instrução.
    return AdvancePhaseAllocation(fu, cycle);
}

// Privado:
void ReservationStation::CheckDependency(
    const size_t idx,
    CDB&       cdb
){
    // Define os parâmetros da atualização:
    // - Registrador fonte da posição 'idx' da instrução.
    const Register& reg = current_instruction->GetSourceRegisters()[idx];
    // - V[idx]/Q[idx] atuais.
    Register& V_idx = V[idx];
    std::pair<std::string, int>& Q_idx = Q[idx];

    // Já existe um V ou se Q já foi resolvido.
    if (V_idx.GetType() != 'Z' || Q_idx.first.empty()) return;

    Register& regCDB = (reg.GetType() == 'F')
        ? cdb.F[reg.GetId()]
        : cdb.R[reg.GetId()];

    // Verifica se já resolveu nesse cíclo para atualizar.
    if (regCDB.IsDependencyResolved(Q_idx.first, Q_idx.second)) {
        V_idx = reg;
        Q_idx = {"", -1};
    }
}

// Privado:
bool ReservationStation::AdvancePhaseAllocation(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    INSTRUCTION_TYPE type = current_instruction->GetInstructionType();
    bool is_load_store = (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE);

    // EX -> MEM:
    // - Apenas para LOAD e STORE.
    if (is_load_store && phase == INSTRUCTION_PHASE_TOMASULO::MEM && allocation_countdown == -1) {
        // O dado ainda não está pronto (fonte 0 = dado; demais = endereço).
        if (type == INSTRUCTION_TYPE::STORE && !Q.empty() && !Q[0].first.empty()) return false;

        return TryAllocateFU(fu, INSTRUCTION_PHASE_TOMASULO::MEM, cycle, current_instruction->GetMemLatency());
    }

    // IS -> EX:
    // - Para todos os tipos de instrução.
    if (is_load_store) { // LOAD ou STORE:
        // Cálculo de endereço depende apenas da ÚLTIMA fonte (base/endereço);
        // o dado (fonte 0) é assunto do MEM.
        if (phase != INSTRUCTION_PHASE_TOMASULO::IS) return false;
        if (!Q.empty() && !Q.back().first.empty()) return false;
    } else { // Instrução genérica:
        // Dependências não resolvidas genéricas que impedem o cálculo em EX.
        for (const auto& q : Q)
            if (!q.first.empty()) return false;
    }

    return TryAllocateFU(fu, INSTRUCTION_PHASE_TOMASULO::EX, cycle, current_instruction->GetExLatency());
}

// Privado:
bool ReservationStation::TryAllocateFU(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_PHASE_TOMASULO new_phase,
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
    fu_position = FindFreeFU(fu, new_phase, cycle);
    if (fu_position == -1) return false; // Não encontrou.

    // Atualiza a fase (permanece igual se já era MEM) e define o tempo de alocação.
    phase = new_phase;
    allocation_countdown = latency;
    return true;
}

// Privado:
// Recebe a fase em que a instrução VAI ENTRAR para escolher a UF correta.
int ReservationStation::FindFreeFU(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_PHASE_TOMASULO target_phase,
    const int                        cycle
) {
    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction->GetInstructionType(), target_phase)};
    return AllocateFreeFU(fu_group, cycle, id);
}

// Público:
// Decrementa o contador de ciclos da fase atual.
bool ReservationStation::UpdateCountdown(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    // RS vazio ou dependências impedindo a execução.
    if (!busy || allocation_countdown == -1) return false;
    allocation_countdown--;
    // Execução incompleta (faltam ciclos para acabar).
    if (allocation_countdown > 0) return false;

    // Instrução acabou de chegar no 0 da sua execução:
    INSTRUCTION_TYPE type = current_instruction->GetInstructionType();
    // Libera a unidade funcional que estava sendo usada.
    ReleaseFU(fu, phase, cycle);
    // Verifica o próximo estágio:
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // EX  -> MEM
        if (phase == INSTRUCTION_PHASE_TOMASULO::EX) phase = INSTRUCTION_PHASE_TOMASULO::MEM;
        // MEM -> WR
        else phase = INSTRUCTION_PHASE_TOMASULO::WR;
    }   // EX  -> WR
    else {
        phase = INSTRUCTION_PHASE_TOMASULO::WR;
    }
    allocation_countdown = -1;
    return true;
}

// Privado:
// Recebe a fase que ACABOU para saber de qual grupo liberar.
void ReservationStation::ReleaseFU(
    FUNCTIONAL_UNITS&                fu,
    const INSTRUCTION_PHASE_TOMASULO finished_phase,
    const int                        cycle
){
    if (fu_position == -1) return;
    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction->GetInstructionType(),finished_phase)};

    if (!DeallocateFU(fu_group, fu_position, cycle)) {
        std::cerr <<
        "[ERRO] Posição inválida de fu: " << fu_position <<
        "- RS: " << id << "\n" <<
        "- Fase: " << static_cast<int>(finished_phase) << "\n";
        std::abort();
    }
    fu_position = -1;
}

// Público:
// Resolve dependências: se esta RS estiver esperando pelo produtor 'rs_id', captura o valor e limpa a pendência.
// - Escolha de implementação: apenas o rs_id é verificado (não o start_cycle).
// - Uma RS ocupada sempre tem suas dependências resolvidas antes de ser liberada, não havendo Q[i] stale de alocações anteriores.
void ReservationStation::ResolveDependency(
    const std::string& rs_id,
    const Register&    value
){
    for (size_t i = 0; i < Q.size(); i++)
        // Não dá Q[i].erase(i) para manter a posição equivalente entre Vn e Qn.
        // - Se fosse removido o Qj, o Qk iria para a posição 0 e o Vk (V[1]) não o acharia.
        if (Q[i].first == rs_id) { V[i] = value; Q[i] = {"", -1}; }
}

// Público:
void ReservationStation::Release(
    const int cycle
){
    allocation_times.push_back(cycle);
    busy                 = false;
    allocation_countdown = -1;
    fu_position          = -1;
    // Vetores empty.
    V.clear();
    Q.clear();
    phase                = INSTRUCTION_PHASE_TOMASULO::UNUSED;
}

} // namespace processor
