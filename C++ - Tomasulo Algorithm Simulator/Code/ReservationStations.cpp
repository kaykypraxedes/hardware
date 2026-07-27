/* ReservationStations.cpp */
#include "headers/ReservationStations.h"
#include "headers/Components.h"
#include "headers/Instruction.h"
#include <string>

static bool mesmoRegistrador(const Register& a, const Register& b) {
    return a.GetType() != 'Z' && a.GetType() == b.GetType() && a.GetId() == b.GetId();
}

ReservationStation::ReservationStation(std::string id) : id(id){}
// Códigos pequenos:
bool                     ReservationStation::GetBusy()                const { return busy; }
int                      ReservationStation::GetCountdown()           const { return allocation_countdown; }
int                      ReservationStation::GetFUPosition()          const { return fu_position; }
INSTRUCTION_PHASE        ReservationStation::GetInstructionPhase()    const { return phase; }
const Instruction&       ReservationStation::GetCurrentInstruction()  const { return current_instruction; } // const & para evitar cópia
const std::string&       ReservationStation::GetId()                  const { return id; }              // const & para evitar cópia
const std::string&       ReservationStation::GetQj()                  const { return Qj.first; }        // const & para evitar cópia
const std::string&       ReservationStation::GetQk()                  const { return Qk.first; }        // const & para evitar cópia
const std::vector<int>&  ReservationStation::GetTimes()               const { return allocation_times; } // const & para evitar cópia
const std::vector<std::string>& ReservationStation::GetInstructions() const { return allocated_instructions; } // const & para evitar cópia

// Códigos grandes:
// Lê Qj/Qk do CDB ANTES de marcar o destino para não se auto-bloquear.
// Qj/Qk são agora std::pair<std::string,int> = {rs_id, ciclo_inicio_no_cdb}.
// Par {"", -1} significa operando disponível (sem dependência pendente).
bool ReservationStation::AddIssue(
    Instruction& instruction,
    CDB&       cdb,
    int        cycle
){
    if (busy) return false;
    busy            = true;
    current_instruction = instruction;
    phase           = INSTRUCTION_PHASE::ISSUE;
    allocation_countdown = -1;
    fu_position                 = -1;
    Qj = Qk = {"", -1};
    Vj = Vk = Register{};
    dest_pending_in_cdb = false;
    allocated_instructions.push_back(instruction.GetInstructionString());
    allocation_times.push_back(cycle);

    Register dest = instruction.GetDestRegister();
    bool dest_valido = (dest.GetType() != 'Z' && dest.GetId() >= 0);

    Register regJ = instruction.GetJ();
    Register regK = instruction.GetK();

    if (regJ.GetType() != 'Z' && regJ.GetId() >= 0 && regJ.GetId() < num_registers) {
        Register& regCDBj = (regJ.GetType() == 'F')
            ? cdb.F[regJ.GetId()] : cdb.R[regJ.GetId()];
        std::string tag = regCDBj.GetCurrentRS();
        if (tag.empty()) {
            Vj = regJ;
        } else if (dest_valido && mesmoRegistrador(regJ, dest) && tag == id) {
            Vj = regJ;
        } else {
            Qj = {tag, regCDBj.GetRSCycleStart(tag)};
        }
    }
    if (regK.GetType() != 'Z' && regK.GetId() >= 0 && regK.GetId() < num_registers) {
        Register& regCDBk = (regK.GetType() == 'F')
            ? cdb.F[regK.GetId()] : cdb.R[regK.GetId()];
        std::string tag = regCDBk.GetCurrentRS();
        if (tag.empty()) {
            Vk = regK;
        } else if (dest_valido && mesmoRegistrador(regK, dest) && tag == id) {
            Vk = regK;
        } else {
            Qk = {tag, regCDBk.GetRSCycleStart(tag)};
        }
    }

    if (dest_valido && dest.GetId() < num_registers) {
        if      (dest.GetType() == 'F') cdb.F[dest.GetId()].AllocateRS(id, cycle);
        else if (dest.GetType() == 'R') cdb.R[dest.GetId()].AllocateRS(id, cycle);
    }
    return true;
}

// Resolve Qj/Qk consultando o CDB via dependenciaResolvida(rs_id, ciclo_inicio).
// Quando prontos, aloca UF e inicia contagem.
bool ReservationStation::UpdateDependencies(
    CDB&                cdb,
    FUNCTIONAL_UNITS&   fu,
    int                 cycle
){
    if (!busy || allocation_countdown != -1) return false;

    Register regJ = current_instruction.GetJ();
    Register regK = current_instruction.GetK();

    // Resolver Vj / Qj
    if (Vj.GetType() == 'Z' && !Qj.first.empty()
        && regJ.GetId() >= 0 && regJ.GetId() < num_registers) {
        Register& regCDB = (regJ.GetType() == 'F')
            ? cdb.F[regJ.GetId()] : cdb.R[regJ.GetId()];
        if (regCDB.IsDependencyResolved(Qj.first, Qj.second)) {
            Vj = regJ;
            Qj = {"", -1};
        }
    }
    // Resolver Vk / Qk
    if (Vk.GetType() == 'Z' && !Qk.first.empty()
        && regK.GetId() >= 0 && regK.GetId() < num_registers) {
        Register& regCDB = (regK.GetType() == 'F')
            ? cdb.F[regK.GetId()] : cdb.R[regK.GetId()];
        if (regCDB.IsDependencyResolved(Qk.first, Qk.second)) {
            Vk = regK;
            Qk = {"", -1};
        }
    }

    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    bool load_store = (tipo == INSTRUCTION_TYPE::LOAD || tipo == INSTRUCTION_TYPE::STORE);

    if (load_store) {
        if (phase == INSTRUCTION_PHASE::ISSUE && Qk.first.empty()) {
            fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::EX);
            if (fu_position == -1) return false;
            allocation_countdown = current_instruction.GetExLatency();
            phase = INSTRUCTION_PHASE::EX;
            return true;
        }
        if (phase == INSTRUCTION_PHASE::MEM && allocation_countdown == -1
            && ((tipo == INSTRUCTION_TYPE::STORE && Qj.first.empty()) || tipo == INSTRUCTION_TYPE::LOAD)) {
            fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::MEM);
            if (fu_position == -1) return false;
            allocation_countdown = current_instruction.GetMemLatency();
            return true;
        }
        return false;
    }

    // Instrução comum: precisa de Qj e Qk resolvidos
    if (Qj.first.empty() && Qk.first.empty()) {
        fu_position = FindFreeFU(fu, cycle, INSTRUCTION_PHASE::EX);
        if (fu_position == -1) return false;
        allocation_countdown = current_instruction.GetExLatency();
        phase = INSTRUCTION_PHASE::EX;
        if (dest_pending_in_cdb) {
            Register dest = current_instruction.GetDestRegister();
            if (dest.GetId() >= 0 && dest.GetId() < num_registers) {
                if (dest.GetType() == 'F') cdb.F[dest.GetId()].AllocateRS(id, cycle);
                else if (dest.GetType() == 'R') cdb.R[dest.GetId()].AllocateRS(id, cycle);
            }
            dest_pending_in_cdb = false;
        }
        return true;
    }
    return false;
}

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

// Broadcast do CDB: se esta RS está esperando por rs_id com aquele ciclo_inicio,
// captura o valor agora. O par {rs_id, ciclo_inicio} identifica unicamente o produtor.
void ReservationStation::ResolveDependency(
    const std::string& rs_id,
    const Register& value
){
    if (Qj.first == rs_id) { Vj = value; Qj = {"", -1}; }
    if (Qk.first == rs_id) { Vk = value; Qk = {"", -1}; }
}

void ReservationStation::Release(
    int cycle
){
    allocation_times.push_back(cycle);
    busy                         = false;
    allocation_countdown = -1;
    fu_position                   = -1;
    Qj = Qk                      = {"", -1};
    Vj = Vk                      = Register{};
    phase                         = INSTRUCTION_PHASE::ISSUE;
}

// Recebe a fase em que a instrução VAI ENTRAR para escolher a UF correta:
//   LOAD/STORE em EX  → cálculo de endereço → ula_int_basico
//   LOAD/STORE em MEM → acesso à memória    → acessar_memoria
int ReservationStation::AllocateFreeFU(
    std::vector<FU>& group,
    int              cycle
) const {
    for (int i = 0; i < (int)group.size(); i++) {
        if (!group[i].busy) {
            group[i].busy     = true;
            group[i].current_rs = id;
            group[i].allocated_rs.push_back(id);
            group[i].allocation_times.push_back(cycle);
            return i;
        }
    }
    return -1;
}

int ReservationStation::FindFreeFU(
    FUNCTIONAL_UNITS& fu,
    int               cycle,
    INSTRUCTION_PHASE       target_phase
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

// Recebe a fase que ACABOU para saber de qual grupo liberar:
//   LOAD/STORE saindo de EX  → liberou ula_int_basico
//   LOAD/STORE saindo de MEM → liberou acessar_memoria
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

void ReservationStation::ReleaseFU(
    FUNCTIONAL_UNITS& fu,
    int               cycle,
    INSTRUCTION_PHASE finished_phase
){
    if (fu_position == -1) return;
    INSTRUCTION_TYPE tipo = current_instruction.GetInstructionType();
    if (tipo == INSTRUCTION_TYPE::LOAD || tipo == INSTRUCTION_TYPE::STORE) {
        if (finished_phase == INSTRUCTION_PHASE::EX) DeallocateFUFromGroup(fu.int_basic_alu, cycle);
        else                                        DeallocateFUFromGroup(fu.memory_access, cycle);
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
