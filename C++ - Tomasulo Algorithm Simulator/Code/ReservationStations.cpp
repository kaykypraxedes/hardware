/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static bool InvalidRegister(const Register& reg) {
    return reg.GetType() == 'Z'; // É inválido se for igual
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
    FUNCTIONAL_UNITS&   fu,
    const INSTRUCTION_TYPE  type,
    const INSTRUCTION_PHASE phase
) {
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE)
        return (phase == INSTRUCTION_PHASE::EX)
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
INSTRUCTION_PHASE ReservationStation::GetInstructionPhase() const { return phase; }

// Público:
const std::string& ReservationStation::GetId() const { return id; }

// Público:
const std::string& ReservationStation::GetQj() const { return Qj.first; }

// Público:
const std::string& ReservationStation::GetQk() const { return Qk.first; }

// Público:
const std::vector<int>& ReservationStation::GetTimes()                const { return allocation_times; }

// Público:
const Instruction& ReservationStation::GetCurrentInstruction()        const { return current_instruction; }

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
    const Instruction& instruction,
    CDB&               cdb,
    const int          cycle
){
    // RS ocupado
    if (busy) return false;
    // Reseta o RS
    SetupNewIssue(instruction, cycle);
    // Define Vj, Vk, Qj e Qk:
    Register dest    = instruction.GetDestRegister();
    Register regJ    = instruction.GetJ();
    Register regK    = instruction.GetK();
    // 1. Marca no RS.
    ReadSourceOperand('J', regJ, cdb);
    ReadSourceOperand('K', regK, cdb);
    // 2. Marca no CDB.
    AllocateDestInCDB(dest, cdb, cycle);
    return true;
}

// Privado:
// Apenas faz a limpeza e redefinição dos dadospara o novo issue (e marca a nova alocação nos vetores).
void ReservationStation::SetupNewIssue(
    const Instruction& instruction,
    const int          cycle
){
    // RS está sendo usada
    busy                 = true;
    current_instruction  = instruction;
    phase                = INSTRUCTION_PHASE::ISSUE;
    // Valores default
    allocation_countdown = -1;
    fu_position          = -1;
    Qj = Qk              = {"", -1};
    Vj = Vk              = Register{};
    // Marcação da nova alocação
    allocated_instructions.push_back(instruction.GetInstructionString());
    allocation_times.push_back(cycle);
}

// Privado:
// Faz a leitura efetiva dos registradores e verifica se a alocação é em Vn (dado pronto) ou em Qn (dependente).
void ReservationStation::ReadSourceOperand(
    const char      type,
    const Register& src,
    const CDB&      cdb
){
    // Não tem J e/ou K.
    if (InvalidRegister(src)) return;

    // Acessa diretamente o registrador alvo dentro do CDB.
    Register regCDB = (src.GetType() == 'F')
        ? cdb.F[src.GetId()]
        : cdb.R[src.GetId()];
    // Verifica se ele está com uma dependencia atualmente.
    std::string tag = regCDB.GetCurrentRS();
    Register                    &V = type == 'J' ? Vj : Vk;
    std::pair<std::string, int> &Q = type == 'J' ? Qj : Qk;
    // Define o V ou o Q a depender do estado da alocação:
    // 1. Sem resultado pendente
    if (tag.empty()) V = src;
    // 2. Resultado pendente
    else Q = {tag, regCDB.GetRSCycleStart(tag)};
}

// Privado:
void ReservationStation::AllocateDestInCDB(
    const Register& dest,
    CDB&            cdb,
    const int       cycle
){
    // Não tem destino
    if (InvalidRegister(dest)) return;

    if (dest.GetType() == 'F') cdb.F[dest.GetId()].AllocateRS(id, cycle);
    else                       cdb.R[dest.GetId()].AllocateRS(id, cycle);
}

// Público:
bool ReservationStation::UpdateDependencies(
    CDB&              cdb,
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    // Se o RS estiver vazio ou com sua execução travada por dependencias.
    if (!busy || allocation_countdown != -1) return false;
    // Se já estiver na fase WR (-1, mas não atualiza mais)
    if (phase == INSTRUCTION_PHASE::WR) return false;

    // Verifica se os Qj e/ou Qk já desapareceram (se sim, marca o Vj/Vk).
    CheckDependency('J', cdb);
    CheckDependency('K', cdb);

    // Aloca as FUs a depender da necessidade e fase da instrução.
    return AdvancePhaseAllocation(fu, cycle);
}

// Privado:
void ReservationStation::CheckDependency(
    const char type,
    CDB&       cdb
){
    // Define os parâmetros da atualização:
    bool isJ{type == 'J'};
    // - Instruction J/K.
    const Register& reg{isJ ? current_instruction.GetJ() : current_instruction.GetK()};
    // - Vj/Vk atual
    Register& V{isJ ? Vj : Vk};
    // - Qj/Qk atual
    std::pair<std::string, int>& Q{isJ ? Qj : Qk};

    // Já existe um V ou se Q já foi resolvido
    if (V.GetType() != 'Z' || Q.first.empty()) return;

    Register& regCDB = (reg.GetType() == 'F')
        ? cdb.F[reg.GetId()]
        : cdb.R[reg.GetId()];

    // Verifica se já resolveu nesse cíclo para atualizar
    if (regCDB.IsDependencyResolved(Q.first, Q.second)) {
        V = reg;
        Q = {"", -1};
    }
}

// Privado:
bool ReservationStation::AdvancePhaseAllocation(
    FUNCTIONAL_UNITS& fu,
    const int         cycle
){
    INSTRUCTION_TYPE type = current_instruction.GetInstructionType();
    bool is_load_store = (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE);

    // EX -> MEM:
    // - Apenas para LOAD e STORE
    if (is_load_store && phase == INSTRUCTION_PHASE::MEM && allocation_countdown == -1) {
        // O dado ainda não está pronto
        if (type == INSTRUCTION_TYPE::STORE && !Qj.first.empty()) return false;

        return TryAllocateFU(fu, INSTRUCTION_PHASE::MEM, cycle, current_instruction.GetMemLatency());
    }

    // IS -> EX:
    // - Para todos os tipos de instrução.
    if (is_load_store) { // LOAD ou STORE:
        // Cálculo de endereço só depende de Qk (Qj é assunto do MEM).
        if (phase != INSTRUCTION_PHASE::ISSUE || !Qk.first.empty()) return false;
    } // Instrução genérica:
    else {
        // Dependência não resolvida genérica que impede o cálculo em EX.
        if (!Qj.first.empty() || !Qk.first.empty()) return false;
    }

    return TryAllocateFU(fu, INSTRUCTION_PHASE::EX, cycle, current_instruction.GetExLatency());
}

// Privado:
bool ReservationStation::TryAllocateFU(
    FUNCTIONAL_UNITS&       fu,
    const INSTRUCTION_PHASE new_phase,
    const int               cycle,
    const int               latency
){
    // Evita latências inválidas de instrução
    if (latency <= 0) {
        std::cerr <<
        "[ERRO] Latência inválida: " << latency << '\n' <<
        "- Instrução: " << current_instruction.GetInstructionString() << '\n' <<
        "- RS: " << id << '\n';
        std::abort();
    }
    // Procura uma unidade funcional livre.
    fu_position = FindFreeFU(fu, new_phase, cycle);
    if (fu_position == -1) return false; // Não encontrou

    // Atualiza a fase (permanece igual se já era MEM) e define o tempo de alocação.
    phase = new_phase;
    allocation_countdown = latency;
    return true;
}

// Privado:
// Recebe a fase em que a instrução VAI ENTRAR para escolher a UF correta.
int ReservationStation::FindFreeFU(
    FUNCTIONAL_UNITS&       fu,
    const INSTRUCTION_PHASE target_phase,
    const int               cycle
) {
    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction.GetInstructionType(), target_phase)};
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
    INSTRUCTION_TYPE type = current_instruction.GetInstructionType();
    // Libera a unidade funcional que estava sendo usada.
    ReleaseFU(fu, phase, cycle);
    // Verifica o próximo estágio:
    if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
        // EX  -> MEM
        if (phase == INSTRUCTION_PHASE::EX) phase = INSTRUCTION_PHASE::MEM;
        // MEM -> WR
        else                                phase = INSTRUCTION_PHASE::WR;
    }   // EX  -> WR
    else {
        phase = INSTRUCTION_PHASE::WR;
    }
    allocation_countdown = -1;
    return true;
}

// Privado:
// Recebe a fase que ACABOU para saber de qual grupo liberar.
void ReservationStation::ReleaseFU(
    FUNCTIONAL_UNITS&       fu,
    const INSTRUCTION_PHASE finished_phase,
    const int               cycle
){
    if (fu_position == -1) return;
    std::vector<FU>& fu_group{GetFUGroup(fu, current_instruction.GetInstructionType(),finished_phase)};

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
// Resolve dependência de Qj/Qk: se esta RS estiver esperando pelo produtor 'rs_id', captura o valor e limpa a pendência.
// - Escolha de implementação: apenas o rs_id é verificado (não o start_cycle) porque uma RS ocupada sempre tem suas dependências resolvidas antes de ser liberada, não havendo Qj/Qk stale de alocações anteriores.
void ReservationStation::ResolveDependency(
    const std::string& rs_id,
    const Register&    value
){
    if (Qj.first == rs_id) { Vj = value; Qj = {"", -1}; }
    if (Qk.first == rs_id) { Vk = value; Qk = {"", -1}; }
}

// Público:
void ReservationStation::Release(
    int cycle
){
    allocation_times.push_back(cycle);
    busy                 = false;
    allocation_countdown = -1;
    fu_position          = -1;
    Qj = Qk              = {"", -1};
    Vj = Vk              = Register{};
    phase                = INSTRUCTION_PHASE::ISSUE;
}

} // namespace processor
