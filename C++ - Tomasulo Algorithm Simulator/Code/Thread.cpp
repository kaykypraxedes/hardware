/* Thread.cpp */
#include "headers/Thread.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int NUM_RS_GROUPS = 6;
static const int NUM_FU_GROUPS = 6;

static int PCOfRS(
    ReservationStation* r
){
    return r->GetCurrentInstruction().GetPosition();
}

static int PCOfEvent(
    const EVENT& e
){
    return e.pc;
}

static void SortCandidatesByPC(
    std::vector<ReservationStation*>& candidates
){
    sort_utils::insertionSort(candidates, PCOfRS);
}

static void ResolveDependencyInGroup(
    std::vector<ReservationStation>& group,
    const std::string& rs_id,
    const Register& dest
){
   for (ReservationStation& dep : group)
        if (dep.GetBusy()) dep.ResolveDependency(rs_id, dest);
}

static void SortEventsByPC(
    std::vector<EVENT>& events
){
    sort_utils::insertionSort(events, PCOfEvent);
}

static void ReleaseRSByRegister(
    std::vector<ReservationStation>& rs,
    Register      dest_reg,
    int              cycle
){
    for(ReservationStation& r : rs){
        if(!r.GetBusy()) continue;
        Register aux{r.GetCurrentInstruction().GetDestRegister()};
        if(r.GetInstructionPhase() == INSTRUCTION_PHASE::WB &&
            aux.GetType() == dest_reg.GetType() &&
            aux.GetId() == dest_reg.GetId())
            r.Release(cycle);
    }
}

static void ReleaseRSByPC(
    std::vector<ReservationStation>& group,
    int              position,
    int              cycle
){
    for (ReservationStation& r : group) {
        if (r.GetBusy() && r.GetInstructionPhase() == INSTRUCTION_PHASE::WB
            && r.GetCurrentInstruction().GetPosition() == position)
            r.Release(cycle);
    }
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Thread::GetCurrentInstructionPosition() const { return current_instruction_position; }

// Público:
int Thread::GetNumStalls()                  const { return num_stalls;                   }

// Público:
THREAD_STATE Thread::GetThreadState()       const {return state;                         }

// Público:
const CDB& Thread::GetCDB()                      const { return cdb;               }

// Público:
const RESERVATION_STATIONS& Thread::GetRS()      const { return rs;                }

// Público:
const FUNCTIONAL_UNITS& Thread::GetFU()          const { return fu;                }

// Público:
const std::vector<TABLE_ROW>& Thread::GetTable() const { return instruction_table; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Thread::Thread(
    const std::vector<std::string>&             assembly,
    const std::vector<std::tuple<int,int,int>>& new_latency,
    const std::vector<int>&                     num_rs,
    const std::vector<int>&                     num_fus,
    const std::vector<int>&                     switch_cycles,
    const int                                   dispatch_width,
    const int                                   rob_capacity,
    const bool                                  has_predictor
):
    has_rob        (rob_capacity > 0),
    rob_capacity   (rob_capacity > 0 ? rob_capacity : 1),
    has_predictor  (has_predictor),
    switch_cycles  (switch_cycles)
{
    // Verifica inconsistência de input
    if (has_predictor && !has_rob) {
        std::cerr << "[ERRO] Para ter previsor de desvios é obrigatório ter ROB\n";
        std::abort();
    }
    // Passa as instruções para a tabela.
    int i{};
    for (const std::string& instr : assembly)
        // Ignora propositalmente os outros valores para que eles recebam o default.
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        instruction_table.push_back({Instruction(i++, instr)}); // Warning ignorado.
        #pragma GCC diagnostic pop
    // Inicializa os RSs e as FUs.
    InitializeComponents(num_rs, num_fus, dispatch_width);
    // Passa as novas latências às instruções.
    for (const auto& [pc, ex, mem] : new_latency) {
        if (static_cast<size_t>(pc) < instruction_table.size()) {
            instruction_table[pc].instruction.SetExLatency(ex);
            if (mem > 0) instruction_table[pc].instruction.SetMemLatency(mem);
        }
    }
}
// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Thread::InitializeComponents(
    const std::vector<int>& num_rs,
    const std::vector<int>& num_fus,
    int                     dispatch_width
){
    // Declara os registradores do banco:
    for(int i{}; i < num_registers; i++){
        cdb.R.push_back(Register("R" + std::to_string(i))); // R: R0, R1, ..., R31
        cdb.F.push_back(Register("F" + std::to_string(i))); // F: F0, F1, ..., F31
    }
    std::vector<int> aux;
    // Declara os componententes do RS:
    // Verifica se a quantidade de valores para RSs passados é válido:
    if(num_rs.empty()) aux = {5,5,5,4,3,2}; // Valores arbitrários de default.
    // Verifica se o valor passado é válido:
    else if(num_rs.size() != NUM_RS_GROUPS){
        std::cerr << "[ERRO] Quantidade inválida de valores para RSs: " << num_rs.size() << "\n";
        std::abort();
    }
    else aux = num_rs;
    for(int i{}; i < aux[0]; i++) rs.load.push_back(ReservationStation("load" + std::to_string(i)));
    for(int i{}; i < aux[1]; i++) rs.store.push_back(ReservationStation("store" + std::to_string(i)));
    for(int i{}; i < aux[2]; i++) rs.int_basic.push_back(ReservationStation("int_basic" + std::to_string(i)));
    for(int i{}; i < aux[3]; i++) rs.int_mult_div.push_back(ReservationStation("int_mult_div" + std::to_string(i)));
    for(int i{}; i < aux[4]; i++) rs.float_basic.push_back(ReservationStation("float_basic" + std::to_string(i)));
    for(int i{}; i < aux[5]; i++) rs.float_mult_div.push_back(ReservationStation("float_mult_div" + std::to_string(i)));
    // Verifica se o número de valores para FUs passados é válido:
    if(num_fus.empty()) aux = {1,1,1,1,1,2}; // Valores arbitrários de default.
    // Verifica se o valor passado é válido:
    else if(num_fus.size() != NUM_FU_GROUPS){
        std::cerr << "[ERRO] Quantidade inválida de valores para FUs: " << num_fus.size() << "\n";
        std::abort();
    } else aux = num_fus;
    for(int i{}; i < aux[0]; i++) fu.memory_access.push_back(FU{});
    for(int i{}; i < aux[1]; i++) fu.int_basic_alu.push_back(FU{});
    for(int i{}; i < aux[2]; i++) fu.int_mult_div_alu.push_back(FU{});
    for(int i{}; i < aux[3]; i++) fu.float_basic_alu.push_back(FU{});
    for(int i{}; i < aux[4]; i++) fu.float_mult_div_alu.push_back(FU{});
    fu.wr     = aux[5];
    if(has_rob) fu.commit = dispatch_width; // Só inicializa commit se tem ROB
}

// Público:
bool Thread::IsSwitchCycle() {
    if (switch_cycles.empty()) return false;
    if (current_instruction_position != switch_cycles.front()) return false;
    // Apaga o valor atual, já que o ciclo já passou.
    switch_cycles.erase(switch_cycles.begin());
    return true;
}

// ─── ISSUE ────────────────────────────────────────────────────────
// Público:
bool Thread::Issue(
    const int cycle
){
    if (current_instruction_position >= static_cast<int>(instruction_table.size())) return false;
    if (rob.size() >= static_cast<size_t>(rob_capacity)) return false;

    Instruction& instruction = instruction_table[current_instruction_position].instruction;
    INSTRUCTION_TYPE type    = instruction.GetInstructionType();
    if (type == INSTRUCTION_TYPE::NONEXISTENT) {
        std::cerr << "[ERRO] Tentativa de adicionar instrução inválida no issue: \n" <<
        "- Instrução = " << instruction_table[current_instruction_position].instruction.GetInstructionString()
        << "\n" << "- Position = " << current_instruction_position << "\n";
        std::abort();
    }

    std::vector<ReservationStation>* group = nullptr;
    switch (type) {
        case INSTRUCTION_TYPE::LOAD:         group = &rs.load;          break;
        case INSTRUCTION_TYPE::STORE:        group = &rs.store;         break;
        case INSTRUCTION_TYPE::FLOAT_BASIC:  group = &rs.float_basic;   break;
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:      group = &rs.int_mult_div;  break;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:    group = &rs.float_mult_div;break;
        default:                             group = &rs.int_basic;     break;
    }

    for (ReservationStation& r : *group) {
        if (r.AddIssue(instruction, cdb, cycle)) {
            RegisterIssue(cycle);
            if(has_rob) rob.push_back(instruction);
            current_instruction_position++;
            if (type == INSTRUCTION_TYPE::BRANCH && !has_rob)
                unresolved_branch_pc = current_instruction_position - 1;
            return true;
        }
    }
    return false;
}

// Privado:
void Thread::RegisterIssue(
    int cycle
){
    instruction_table[current_instruction_position].issue_cycle = cycle;
}

// ─── EX/MEM ───────────────────────────────────────────────────────
// Público:
bool Thread::ExMem(
    const int cycle
){
    if(static_cast<size_t>(num_committed_instructions) == instruction_table.size() ||
       (!has_rob && static_cast<size_t>(num_finished_instructions) == instruction_table.size()))
       return true;
    if (state == THREAD_STATE::BRANCH_RESOLVING)
        state = THREAD_STATE::ACTIVE;
    if(static_cast<size_t>(num_finished_instructions) != instruction_table.size())
        StartExOrMemPhase(cycle);
    return false;
}

// Privado:
void Thread::StartExOrMemPhase(
    int cycle
){
    std::vector<ReservationStation*> candidates;
    CollectCandidatesToAdvance(candidates);
    SortCandidatesByPC(candidates);

    for (ReservationStation* rp : candidates) {
        ReservationStation& r = *rp;
        TryAdvanceRS(r, cycle);
    }
}

// Privado:
void Thread::CollectCandidatesToAdvance(
    std::vector<ReservationStation*>& candidates
){
    CollectCandidatesFromGroup(rs.load, candidates);
    CollectCandidatesFromGroup(rs.store, candidates);
    CollectCandidatesFromGroup(rs.int_basic, candidates);
    CollectCandidatesFromGroup(rs.int_mult_div, candidates);
    CollectCandidatesFromGroup(rs.float_basic, candidates);
    CollectCandidatesFromGroup(rs.float_mult_div, candidates);
}

// Privado:
void Thread::CollectCandidatesFromGroup(
    std::vector<ReservationStation>& group,
    std::vector<ReservationStation*>& candidates
){
    for (ReservationStation& r : group) {
        if (!r.GetBusy()) continue;
        int inst_pc = r.GetCurrentInstruction().GetPosition();
        if (unresolved_branch_pc >= 0 && inst_pc > unresolved_branch_pc) continue;
        INSTRUCTION_TYPE type = r.GetCurrentInstruction().GetInstructionType();
        if (type == INSTRUCTION_TYPE::STORE && has_rob && r.GetInstructionPhase() == INSTRUCTION_PHASE::MEM)
            continue;
        candidates.push_back(&r);
    }
}

// Privado:
void Thread::TryAdvanceRS(
    ReservationStation& r,
    int cycle
){
    if (r.UpdateDependencies(cdb, fu, cycle)) {
        int pc = r.GetCurrentInstruction().GetPosition();
        if (r.GetInstructionPhase() == INSTRUCTION_PHASE::EX)
            AddExCycle(pc, cycle);
        else if (r.GetInstructionPhase() == INSTRUCTION_PHASE::MEM)
            AddMemCycle(pc, cycle);
        if (pc == unresolved_branch_pc && r.GetInstructionPhase() == INSTRUCTION_PHASE::EX)
            unresolved_branch_pc = -1;
    }
}

// Privado: (também usado em WR/ProcessTransition)
void Thread::AddExCycle(
    int pc,
    int cycle
){
    instruction_table[pc].ex_cycles.push_back(cycle);
}

// Privado: (também usado em WR/ProcessTransition)
void Thread::AddMemCycle(
    int pc,
    int cycle
){
    instruction_table[pc].mem_cycles.push_back(cycle);
}

// ─── WR ───────────────────────────────────────────────────────────
// Público:
void Thread::Wr(
    const int cycle
){
    FlushPendingWBBuffer();
    PerformWriteResult(cycle);
    DetectPhaseTransitions(cycle);
    if (!has_rob) {
        for (int i : pending_wb_buffer) {
            if (instruction_table[i].instruction.GetInstructionType() == INSTRUCTION_TYPE::BRANCH)
                state = THREAD_STATE::BRANCH_RESOLVING;
        }
    }
}

// Privado:
void Thread::FlushPendingWBBuffer() {
    for (int pc : pending_wb_buffer)
        wb_buffer.push_back(pc);
    pending_wb_buffer.clear();
}

// Privado:
void Thread::PerformWriteResult(
    int cycle
){
    SortWBBuffer();

    int writes{};
    while (!wb_buffer.empty() && writes < fu.wr) {
        int pc = NextWB();
        INSTRUCTION_TYPE instr_type{instruction_table[pc].instruction.GetInstructionType()};
        bool store_with_rob = (instr_type == INSTRUCTION_TYPE::STORE && has_rob);

        if (store_with_rob) {
            WriteBackStoreWithROB(pc, cycle);
            continue;
        }

        WriteBackNormal(pc, cycle);
        RemoveWB();
        num_finished_instructions++;
        if(instr_type != INSTRUCTION_TYPE::BRANCH) writes++;
    }
}

// Privado:
void Thread::SortWBBuffer() {
    sort_utils::insertionSort(wb_buffer);
}

// Privado:
int Thread::NextWB() const {
    return wb_buffer.front();
}

// Privado:
void Thread::RemoveWB() {
    wb_buffer.erase(wb_buffer.begin());
}

// Privado:
void Thread::AddWB(
    int pc
){
    wb_buffer.push_back(pc);
}

// Privado:
void Thread::WriteBackStoreWithROB(
    int pc,
    int cycle
){
    for (ReservationStation& r : rs.store)
        if (r.GetBusy() && r.GetCurrentInstruction().GetPosition() == pc)
            r.Release(cycle);
    RemoveWB();
    num_finished_instructions++;
}

// Privado:
// 3 etapas: (1) marca WR para instruções c/ destino; (2) broadcast CDB p/ liberar
// dependências; (3) switch por type p/ grupo RS correto (LOAD -> rs.load, etc.)
void Thread::WriteBackNormal(
    int pc,
    int cycle
){
    if(instruction_table[pc].instruction.GetInstructionType() != INSTRUCTION_TYPE::STORE &&
       instruction_table[pc].instruction.GetInstructionType() != INSTRUCTION_TYPE::BRANCH)
        SetWR(pc, cycle);

    Register dest = instruction_table[pc].instruction.GetDestRegister();
    BroadcastCDB(pc, dest, cycle);

    INSTRUCTION_TYPE t{instruction_table[pc].instruction.GetInstructionType()};
    if(t == INSTRUCTION_TYPE::LOAD)
        ReleaseRSByRegister(rs.load, dest, cycle);
    else if(t == INSTRUCTION_TYPE::STORE)
        ReleaseRSByPC(rs.store, pc, cycle);
    else if(t == INSTRUCTION_TYPE::BRANCH)
        ReleaseRSByPC(rs.int_basic, pc, cycle);
    else if(t == INSTRUCTION_TYPE::FLOAT_BASIC)
        ReleaseRSByRegister(rs.float_basic, dest, cycle);
    else if(t == INSTRUCTION_TYPE::INT_MUL || t == INSTRUCTION_TYPE::INT_DIV)
        ReleaseRSByRegister(rs.int_mult_div, dest, cycle);
    else if(t == INSTRUCTION_TYPE::FLOAT_MUL || t == INSTRUCTION_TYPE::FLOAT_DIV)
        ReleaseRSByRegister(rs.float_mult_div, dest, cycle);
    else
        ReleaseRSByRegister(rs.int_basic, dest, cycle);
}

// Privado: (usado em WriteBackNormal)
void Thread::SetWR(
    int pc,
    int cycle
){
    instruction_table[pc].wr_cycle = cycle;
}

// Privado:
// Percorre todos os 6 grupos de RS resolvendo dependências de Qj/Qk que
// apontam para esta instrução (rs_id), e desaloca o produtor do CDB.
void Thread::BroadcastCDB(
    int pc,
    const Register& dest,
    int cycle
){
    FindWBInGroup(rs.load, pc, dest, cycle);
    FindWBInGroup(rs.store, pc, dest, cycle);
    FindWBInGroup(rs.int_basic, pc, dest, cycle);
    FindWBInGroup(rs.int_mult_div, pc, dest, cycle);
    FindWBInGroup(rs.float_basic, pc, dest, cycle);
    FindWBInGroup(rs.float_mult_div, pc, dest, cycle);
}

// Privado:
void Thread::FindWBInGroup(
    std::vector<ReservationStation>& group,
    int pc,
    const Register& dest,
    int cycle
){
    for (ReservationStation& r : group) {

        if (!r.GetBusy() || r.GetInstructionPhase() != INSTRUCTION_PHASE::WB) continue;

        if (r.GetCurrentInstruction().GetPosition() != pc) continue;

        if (dest.GetType() == 'Z' || dest.GetId() < 0 || dest.GetId() >= num_registers) continue;

        // Cria uma referência do vetor de registradores a ser operado no CDB
        std::vector<Register>& regs = (dest.GetType() == 'F') ? cdb.F : cdb.R;
        std::string rs_id = r.GetId();
        int start_cycle = regs[dest.GetId()].GetRSCycleStart(rs_id);
        // Tenta desalocar
        if(!regs[dest.GetId()].DeallocateRS(rs_id, start_cycle, cycle)){
            std::cerr << "[ERRO] Falha na desalocação do RS:" <<
            " rs_id( " << rs_id
            << " ), start_cycle( " << start_cycle
            << " ), end_cycle( " << cycle << " )\n";
            std::abort();
        }
        ResolveDependencyInGroup(rs.load, rs_id, dest);
        ResolveDependencyInGroup(rs.store, rs_id, dest);
        ResolveDependencyInGroup(rs.int_basic, rs_id, dest);
        ResolveDependencyInGroup(rs.int_mult_div, rs_id, dest);
        ResolveDependencyInGroup(rs.float_basic, rs_id, dest);
        ResolveDependencyInGroup(rs.float_mult_div, rs_id, dest);
    }
}

// Privado:
void Thread::DetectPhaseTransitions(
    int cycle
){
    std::vector<EVENT> events;
    CollectTransitionEvents(events, cycle);

    SortEventsByPC(events);

    for (const EVENT& e : events)
        ProcessTransition(e, cycle);
}

// Privado:
void Thread::CollectTransitionEvents(
    std::vector<EVENT>& events,
    int cycle
){
    CollectEventsFromGroup(rs.load, events, cycle);
    CollectEventsFromGroup(rs.store, events, cycle);
    CollectEventsFromGroup(rs.int_basic, events, cycle);
    CollectEventsFromGroup(rs.int_mult_div, events, cycle);
    CollectEventsFromGroup(rs.float_basic, events, cycle);
    CollectEventsFromGroup(rs.float_mult_div, events, cycle);
}

// Privado:
// Privado:
void Thread::CollectEventsFromGroup(std::vector<ReservationStation>& group, std::vector<EVENT>& events, int cycle) {
    for (ReservationStation& r : group) {
        if (!r.GetBusy()) continue;
        INSTRUCTION_PHASE phase_before = r.GetInstructionPhase();
        if (r.UpdateCountdown(fu, cycle)) {
            events.push_back({
                r.GetCurrentInstruction().GetPosition(),
                phase_before,
                r.GetInstructionPhase(),
                r.GetCurrentInstruction().GetInstructionType()
            });
        }
    }
}

// Privado:
void Thread::ProcessTransition(
    const EVENT& e,
    int cycle
){
    int pc              = e.pc;
    bool has_mem        = (e.type == INSTRUCTION_TYPE::LOAD || e.type == INSTRUCTION_TYPE::STORE);
    bool store_with_rob = (e.type == INSTRUCTION_TYPE::STORE && has_rob);

    if (e.phase_before == INSTRUCTION_PHASE::EX && e.phase_after == INSTRUCTION_PHASE::MEM) {
        instruction_table[pc].ex_cycles.push_back(cycle);
        if (store_with_rob)
            AddPendingWB(pc);
    } else if (e.phase_after == INSTRUCTION_PHASE::WB) {
        if (has_mem && !store_with_rob)
            AddMemCycle(pc, cycle);
        else if (!has_mem)
            AddExCycle(pc, cycle);
        AddPendingWB(pc);
    }
}

// Privado: (usado em ProcessTransition)
void Thread::AddPendingWB(
    int pc
){
    pending_wb_buffer.push_back(pc);
}

// ─── COMMIT ───────────────────────────────────────────────────────
// Público:
// Apenas c/ ROB. Itera ROB em ordem (commit_pointer). Lógica de "pronto"
// varia por type: STORE simula latência MEM, BRANCH espera 2 ciclos EX,
// demais aguardam WR < ciclo atual. BRANCH s/ previsor trava no ciclo.
void Thread::Commit(
    const int cycle
){
    if (!has_rob) return;

    int writes{};
    while (!rob.empty() && writes < fu.commit){
        TABLE_ROW& row{instruction_table[commit_pointer]};
        INSTRUCTION_TYPE type = row.instruction.GetInstructionType();
        bool store_with_rob = (type == INSTRUCTION_TYPE::STORE && has_rob);
        bool pronto = false;

        if (store_with_rob) {
            if (row.store_commit_state == STORE_COMMIT_STATE::PENDING) {
                row.store_commit_state = STORE_COMMIT_STATE::WAITING_MEM;
                row.mem_cycles.push_back(cycle);
            }
            if (row.store_commit_state == STORE_COMMIT_STATE::WAITING_MEM) {
                int mem_end = row.mem_cycles.back() + row.instruction.GetMemLatency() - 1;
                if (cycle >= mem_end) {
                    row.store_commit_state = STORE_COMMIT_STATE::READY;
                    row.mem_cycles.pop_back();
                    pronto = true;
                }
            }
        } else if (type == INSTRUCTION_TYPE::BRANCH) {
            pronto = (row.ex_cycles.size() == 2);
        } else {
            pronto = (row.wr_cycle > 0 && row.wr_cycle < cycle);
        }

        if (pronto) {
            row.commit_cycle = cycle;
            num_committed_instructions++;
            writes++;
            commit_pointer++;
            rob.erase(rob.begin());
            if (type == INSTRUCTION_TYPE::BRANCH && !(has_predictor && has_rob)) break;
        }
        else break;
    }
}

} // namespace processor
