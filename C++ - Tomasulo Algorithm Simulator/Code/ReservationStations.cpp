/* ReservationStations.cpp */
#include "headers/ReservationStations.h"

namespace processor {

static bool mesmoRegistrador(const Register& a, const Register& b) {
    return a.GetType() != 'Z' && a.GetType() == b.GetType() && a.GetId() == b.GetId();
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
bool ReservationStation::GetBusy()      const { return busy;                 }

// Público:
int ReservationStation::GetCountdown()  const { return allocation_countdown; }

// Público:
int ReservationStation::GetFUPosition() const { return fu_position;          }

// Público:
INSTRUCTION_PHASE ReservationStation::GetInstructionPhase() const { return phase; }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const std::string& ReservationStation::GetId() const { return id;       }

// Público:
const std::string& ReservationStation::GetQj() const { return Qj.first; }

// Público:
const std::string& ReservationStation::GetQk() const { return Qk.first; }

// Público:
const std::vector<int>& ReservationStation::GetTimes() const { return allocation_times; }

// Público:
const Instruction& ReservationStation::GetCurrentInstruction() const { return current_instruction; }

// Público:
const std::vector<std::string>& ReservationStation::GetInstructions() const { return allocated_instructions; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
ReservationStation::ReservationStation(
    std::string id
) :
id(id)
{}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool ReservationStation::AddIssue(
    Instruction& instruction,
    CDB&         cdb,
    int          cycle
){
    if (busy) return false;

    SetupNewIssue(instruction, cycle);

    Register dest    = instruction.GetDestRegister();
    bool dest_valido = (dest.GetType() != 'Z' && dest.GetId() >= 0);
    Register regJ    = instruction.GetJ();
    Register regK    = instruction.GetK();

    ReadSourceOperand(regJ, cdb, Vj, Qj, dest, dest_valido);
    ReadSourceOperand(regK, cdb, Vk, Qk, dest, dest_valido);

    AllocateDestInCDB(dest, cdb, cycle);
    return true;
}

// Privado:
void ReservationStation::SetupNewIssue(
    const Instruction& instruction,
    int                cycle
){
    busy                 = true;
    current_instruction  = instruction;
    phase                = INSTRUCTION_PHASE::ISSUE;
    allocation_countdown = -1;
    fu_position          = -1;
    Qj = Qk              = {"", -1};
    Vj = Vk              = Register{};
    dest_pending_on_cdb  = false;
    allocated_instructions.push_back(instruction.GetInstructionString());
    allocation_times.push_back(cycle);
}

// Privado:
void ReservationStation::ReadSourceOperand(
    const Register&              src,
    CDB&                         cdb,
    Register&                    V,
    std::pair<std::string, int>& Q,
    const Register&              dest,
    bool                         dest_is_valid
){
    if (src.GetType() == 'Z' || src.GetId() < 0 || src.GetId() >= num_registers)
        return;
    Register& regCDB = (src.GetType() == 'F')
        ? cdb.F[src.GetId()] : cdb.R[src.GetId()];
    std::string tag = regCDB.GetCurrentRS();
    if (tag.empty()) {
        V = src;
    } else if (dest_is_valid && mesmoRegistrador(src, dest) && tag == id) {
        V = src;
    } else {
        Q = {tag, regCDB.GetRSCycleStart(tag)};
    }
}

// Privado:
void ReservationStation::AllocateDestInCDB(
    const Register& dest,
    CDB&            cdb,
    int             cycle
){
    if (dest.GetType() == 'Z' || dest.GetId() < 0 || dest.GetId() >= num_registers)
        return;
    if (dest.GetType() == 'F') cdb.F[dest.GetId()].AllocateRS(id, cycle);
    else                       cdb.R[dest.GetId()].AllocateRS(id, cycle);
}

// Público:
bool ReservationStation::UpdateDependencies(
    CDB&              cdb,
    FUNCTIONAL_UNITS& fu,
    int               cycle
){
    if (!busy || allocation_countdown != -1) return false;

    ResolveBothDependencies(cdb);

    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    if (tipo == INSTRUCTION_TYPE::LOAD || tipo == INSTRUCTION_TYPE::STORE)
        return TryAllocateLoadStore(cdb, fu, cycle);
    return TryAllocateNormal(cdb, fu, cycle);
}

// Privado:
void ReservationStation::ResolveSingleDependency(
    CDB&                         cdb,
    const Register&              reg,
    Register&                    V,
    std::pair<std::string, int>& Q
){
    if (V.GetType() != 'Z' || Q.first.empty()) return;
    if (reg.GetId() < 0 || reg.GetId() >= num_registers) return;
    Register& regCDB = (reg.GetType() == 'F')
        ? cdb.F[reg.GetId()] : cdb.R[reg.GetId()];
    if (regCDB.IsDependencyResolved(Q.first, Q.second)) {
        V = reg;
        Q = {"", -1};
    }
}

// Privado:
void ReservationStation::ResolveBothDependencies(
    CDB& cdb
){
    ResolveSingleDependency(cdb, current_instruction.GetJ(), Vj, Qj);
    ResolveSingleDependency(cdb, current_instruction.GetK(), Vk, Qk);
}

// Privado:
bool ReservationStation::TryAllocateLoadStore(
    CDB&,
    FUNCTIONAL_UNITS& fu,
    int               cycle
){
    if (phase == INSTRUCTION_PHASE::ISSUE && Qk.first.empty()) {
        fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::EX);
        if (fu_position == -1) return false;
        allocation_countdown = current_instruction.GetExLatency();
        phase = INSTRUCTION_PHASE::EX;
        return true;
    }
    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    if (phase == INSTRUCTION_PHASE::MEM && allocation_countdown == -1
        && ((tipo == INSTRUCTION_TYPE::STORE && Qj.first.empty()) || tipo == INSTRUCTION_TYPE::LOAD)) {
        fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::MEM);
        if (fu_position == -1) return false;
        allocation_countdown = current_instruction.GetMemLatency();
        return true;
    }
    return false;
}

// Privado:
bool ReservationStation::TryAllocateNormal(
    CDB&              cdb,
    FUNCTIONAL_UNITS& fu,
    int               cycle
){
    if (!Qj.first.empty() || !Qk.first.empty()) return false;
    fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::EX);
    if (fu_position == -1) return false;
    allocation_countdown = current_instruction.GetExLatency();
    phase = INSTRUCTION_PHASE::EX;
    if (dest_pending_on_cdb) {
        AllocateDestInCDB(current_instruction.GetDestRegister(), cdb, cycle);
        dest_pending_on_cdb = false;
    }
    return true;
}

// Privado:
// Recebe a fase em que a instrução VAI ENTRAR para escolher a UF correta:
// - LOAD/STORE em EX  → cálculo de endereço → ula_int_basico
// - LOAD/STORE em MEM → acesso à memória    → acessar_memoria
int ReservationStation::FindFreeFU(
    FUNCTIONAL_UNITS& fu,
    int               cycle,
    INSTRUCTION_PHASE target_phase
) const {
    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    if (tipo == INSTRUCTION_TYPE::LOAD || tipo == INSTRUCTION_TYPE::STORE) {
        if (target_phase == INSTRUCTION_PHASE::EX)  return AllocateFreeFU(fu.int_basic_alu, cycle);
        else                                        return AllocateFreeFU(fu.memory_access, cycle);
    }
    switch (tipo) {
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:      return AllocateFreeFU(fu.int_mult_div_alu, cycle);
        case INSTRUCTION_TYPE::FLOAT_BASIC:  return AllocateFreeFU(fu.float_basic_alu, cycle);
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:    return AllocateFreeFU(fu.float_mult_div_alu, cycle);
        default:                             return AllocateFreeFU(fu.int_basic_alu, cycle);
    }
}

// Privado:
int ReservationStation::AllocateFreeFU(
    std::vector<FU>& group,
    int              cycle
) const {
    for (int i = 0; i < (int)group.size(); i++) {
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

// Público:
// Retorna true quando uma fase termina. Já avança 'fase' para o próximo estado
// para que executaWrTodos possa distinguir as transições sem ambiguidade.
bool ReservationStation::UpdateCountdown(
    FUNCTIONAL_UNITS& fu,
    int               cycle
){
    if (!busy || allocation_countdown <= 0) return false;

    allocation_countdown--;
    if (allocation_countdown > 0) return false;

    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();

    if (tipo == INSTRUCTION_TYPE::LOAD && phase == INSTRUCTION_PHASE::EX) {
        ReleaseFU(fu, cycle, INSTRUCTION_PHASE::EX);
        allocation_countdown = -1;
        phase = INSTRUCTION_PHASE::MEM;
        return true;
    }

    if (tipo == INSTRUCTION_TYPE::STORE && phase == INSTRUCTION_PHASE::EX) {
        ReleaseFU(fu, cycle, INSTRUCTION_PHASE::EX);
        allocation_countdown = -1;
        phase = INSTRUCTION_PHASE::MEM;
        return true;
    }

    if (phase == INSTRUCTION_PHASE::MEM) {
        ReleaseFU(fu, cycle, INSTRUCTION_PHASE::MEM);
        phase = INSTRUCTION_PHASE::WB;
        return true;
    }
    ReleaseFU(fu, cycle, INSTRUCTION_PHASE::EX);
    phase = INSTRUCTION_PHASE::WB;
    return true;
}

// Privado:
// Recebe a fase que ACABOU para saber de qual grupo liberar:
// - LOAD/STORE saindo de EX  → liberou ula_int_basico
// - LOAD/STORE saindo de MEM → liberou acessar_memoria
void ReservationStation::ReleaseFU(
    FUNCTIONAL_UNITS& fu,
    int               cycle,
    INSTRUCTION_PHASE finished_phase
){
    if (fu_position == -1) return;
    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    if (tipo == INSTRUCTION_TYPE::LOAD || tipo == INSTRUCTION_TYPE::STORE) {
        if (finished_phase == INSTRUCTION_PHASE::EX) DeallocateFUFromGroup(fu.int_basic_alu, cycle);
        else                                         DeallocateFUFromGroup(fu.memory_access, cycle);
    } else {
        switch (tipo) {
            case INSTRUCTION_TYPE::INT_MUL:
            case INSTRUCTION_TYPE::INT_DIV:      DeallocateFUFromGroup(fu.int_mult_div_alu, cycle);   break;
            case INSTRUCTION_TYPE::FLOAT_BASIC:  DeallocateFUFromGroup(fu.float_basic_alu, cycle);    break;
            case INSTRUCTION_TYPE::FLOAT_MUL:
            case INSTRUCTION_TYPE::FLOAT_DIV:    DeallocateFUFromGroup(fu.float_mult_div_alu, cycle); break;
            default:                             DeallocateFUFromGroup(fu.int_basic_alu, cycle);      break;
        }
    }
    fu_position = -1;
}

// Privado:
void ReservationStation::DeallocateFUFromGroup(
    std::vector<FU>& group,
    int              cycle
) {
    if (fu_position < (int)group.size()) {
        group[fu_position].busy = false;
        group[fu_position].current_rs = "";
        group[fu_position].allocation_times.push_back(cycle);
    }
}

// Público:
// Broadcast do CDB: se esta RS está esperando por rs_id com aquele ciclo_inicio, captura o valor agora. O par {rs_id, ciclo_inicio} identifica unicamente o produtor.
void ReservationStation::ResolveDependency(
    const std::string& rs_id,
    const Register& value
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
