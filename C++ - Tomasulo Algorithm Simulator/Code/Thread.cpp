/* Thread.cpp */
#include "headers/Thread.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int NUM_RS_GROUPS = 6;
static const int NUM_FU_GROUPS = 6;

static int pcDeRS(
    ReservationStation* r
){
    return r->GetCurrentInstruction().GetPC();
}

static int pcDeEvento(
    const EVENT& e
){
    return e.pc;
}

static void SortCandidatesByPC(
    std::vector<ReservationStation*>& candidates
){
    sort_utils::insertionSort(candidates, pcDeRS);
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
    sort_utils::insertionSort(events, pcDeEvento);
}

static void ReleaseRSByRegister(
    std::vector<ReservationStation>& rs,
    Register      reg_destino,
    int              cycle
){
    for(ReservationStation& r : rs){
        if(!r.GetBusy()) continue;
        Register aux{r.GetCurrentInstruction().GetDestRegister()};
        if(r.GetInstructionPhase() == INSTRUCTION_PHASE::WB &&
            aux.GetType() == reg_destino.GetType() &&
            aux.GetId() == reg_destino.GetId())
            r.Release(cycle);
    }
}

static void ReleaseRSByPC(
    std::vector<ReservationStation>& group,
    int              pc,
    int              cycle
){
    for (ReservationStation& r : group) {
        if (r.GetBusy() && r.GetInstructionPhase() == INSTRUCTION_PHASE::WB
            && r.GetCurrentInstruction().GetPC() == pc)
            r.Release(cycle);
    }
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Thread::GetPC()                   const { return PC;         }

// Público:
int Thread::GetNumStalls()            const { return num_stalls; }

// Público:
THREAD_STATE Thread::GetThreadState() const { return state;      }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const CDB& Thread::GetCDB() const { return cdb; }

// Público:
const RESERVATION_STATIONS& Thread::GetRS()      const { return rs;                }

// Público:
const FUNCTIONAL_UNITS& Thread::GetFU()          const { return fu;                }

// Público:
const std::vector<TABLE_ROW>& Thread::GetTable() const { return instruction_table; }

// ─── CONSTRUTORES ─────────────────────────────────────────────────
// Público:
Thread::Thread(
    const std::vector<std::string>& assembly,
    bool                            has_rob,
    const std::vector<int>&         num_rs,
    const std::vector<int>&         num_fus,
    int                             dispatch_width,
    int                             rob_capacity,
    bool                            has_predictor
):
    has_rob      (has_rob),
    rob_capacity (has_rob ? rob_capacity : 1),
    has_predictor(has_predictor)
{
    int i{};
    for (const std::string& instr : assembly)
        instruction_table.push_back({Instruction(i++, instr), 0, 0, {}, {}, 0, 0});
    InitializeComponents(num_rs, num_fus, dispatch_width);
}

// Público:
Thread::Thread(
    const std::vector<int>&         switch_instructions,
    const std::vector<std::string>& assembly,
    bool                            has_rob,
    const std::vector<int>&         num_rs,
    const std::vector<int>&         num_fus,
    int                             dispatch_width,
    int                             rob_capacity,
    bool                            has_predictor
):
    has_rob            (has_rob),
    rob_capacity       (has_rob ? rob_capacity : 1),
    has_predictor      (has_predictor),
    switch_instructions(switch_instructions)
{
    int i{};
    for (const std::string& instr : assembly)
        instruction_table.push_back({Instruction(i++, instr), 0, 0, {}, {}, 0, 0});
    InitializeComponents(num_rs, num_fus, dispatch_width);
}

// ─── INICIALIZAÇÃO ────────────────────────────────────────────────
// Privado:
void Thread::InitializeComponents(
    const std::vector<int>& num_rs,
    const std::vector<int>& num_fus,
    int                     dispatch_width
){
    for(int i{}; i < num_registers; i++){
        cdb.R.push_back(Register("R" + std::to_string(i)));
        cdb.F.push_back(Register("F" + std::to_string(i)));
    }
    std::vector<int> aux;
    if(num_rs.size() >= NUM_RS_GROUPS) aux = num_rs;
    else aux = {5,5,5,4,3,2};
    for(int i{}; i < aux[0]; i++) rs.load.push_back(ReservationStation("load" + std::to_string(i)));
    for(int i{}; i < aux[1]; i++) rs.store.push_back(ReservationStation("store" + std::to_string(i)));
    for(int i{}; i < aux[2]; i++) rs.int_basic.push_back(ReservationStation("int_basic" + std::to_string(i)));
    for(int i{}; i < aux[3]; i++) rs.int_mult_div.push_back(ReservationStation("int_mult_div" + std::to_string(i)));
    for(int i{}; i < aux[4]; i++) rs.float_basic.push_back(ReservationStation("float_basic" + std::to_string(i)));
    for(int i{}; i < aux[5]; i++) rs.float_mult_div.push_back(ReservationStation("float_mult_div" + std::to_string(i)));
    if(num_fus.size() >= NUM_FU_GROUPS) aux = num_fus;
    else aux = {1,1,1,1,1,2};
    for(int i{}; i < aux[0]; i++) fu.memory_access.push_back(FU{});
    for(int i{}; i < aux[1]; i++) fu.int_basic_alu.push_back(FU{});
    for(int i{}; i < aux[2]; i++) fu.int_mult_div_alu.push_back(FU{});
    for(int i{}; i < aux[3]; i++) fu.float_basic_alu.push_back(FU{});
    for(int i{}; i < aux[4]; i++) fu.float_mult_div_alu.push_back(FU{});
    fu.wr     = aux[5];
    fu.commit = dispatch_width;
}

// Público:
void Thread::SetCustomLatency(
    int position,
    int ex_latency,
    int mem_latency
){
    if (position < 0 || static_cast<size_t>(position) >= instruction_table.size()) return;
    instruction_table[position].instruction.SetExLatency(ex_latency);
    if (mem_latency > 0)
        instruction_table[position].instruction.SetMemLatency(mem_latency);
}

// ─── ISSUE ────────────────────────────────────────────────────────
// Público:
bool Thread::Issue(
    int cycle
){
    if (state == THREAD_STATE::BLOCKED) return false;
    if (PC >= static_cast<int>(instruction_table.size())) return false;
    if (rob.size() >= static_cast<size_t>(rob_capacity)) return false;

    Instruction& instruction = instruction_table[PC].instruction;
    INSTRUCTION_TYPE tipo   = instruction.GetInstructionType();
    if (tipo == INSTRUCTION_TYPE::NONEXISTENT) { PC++; return false; }

    std::vector<ReservationStation>* group = nullptr;
    switch (tipo) {
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
            RegisterIssue(PC, cycle);
            if(has_rob) rob.push_back(instruction);
            PC++;
            if (tipo == INSTRUCTION_TYPE::BRANCH && !has_rob)
                unresolved_branch_pc = PC - 1;
            return true;
        }
    }
    return false;
}

// Privado:
void Thread::RegisterIssue(
    int pc,
    int cycle
){
    instruction_table[pc].issue_cycle = cycle;
    instruction_table[pc].pc_position  = pc;
}

// ─── EX/MEM ───────────────────────────────────────────────────────
// Público:
bool Thread::ExMem(
    int cycle
){
    if(static_cast<size_t>(num_committed_instructions) == instruction_table.size() ||
       (!has_rob && static_cast<size_t>(num_finished_instructions) == instruction_table.size()))
       return true;
    if (state == THREAD_STATE::WAITING)
        state = THREAD_STATE::FREE;
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
        int inst_pc = r.GetCurrentInstruction().GetPC();
        if (unresolved_branch_pc >= 0 && inst_pc > unresolved_branch_pc) continue;
        INSTRUCTION_TYPE tipo = r.GetCurrentInstruction().GetInstructionType();
        if (tipo == INSTRUCTION_TYPE::STORE && has_rob && r.GetInstructionPhase() == INSTRUCTION_PHASE::MEM)
            continue;
        candidates.push_back(&r);
    }
}

// Privado:
// Privado:
void Thread::TryAdvanceRS(
    ReservationStation& r,
    int cycle
){
    if (r.UpdateDependencies(cdb, fu, cycle)) {
        int pc = r.GetCurrentInstruction().GetPC();
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
    int cycle
){
    FlushPendingWBBuffer();
    PerformWriteResult(cycle);
    DetectPhaseTransitions(cycle);
    if (!has_rob) {
        for (int i : pending_wb_buffer) {
            if (instruction_table[i].instruction.GetInstructionType() == INSTRUCTION_TYPE::BRANCH)
                state = THREAD_STATE::WAITING;
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

    int escritas{};
    while (!wb_buffer.empty() && escritas < fu.wr) {
        int pc = NextWB();
        INSTRUCTION_TYPE auxTipo{instruction_table[pc].instruction.GetInstructionType()};
        bool store_with_rob = (auxTipo == INSTRUCTION_TYPE::STORE && has_rob);

        if (store_with_rob) {
            WriteBackStoreWithROB(pc, cycle);
            continue;
        }

        WriteBackNormal(pc, cycle);
        RemoveWB();
        num_finished_instructions++;
        if(auxTipo != INSTRUCTION_TYPE::BRANCH) escritas++;
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
        if (r.GetBusy() && r.GetCurrentInstruction().GetPC() == pc)
            r.Release(cycle);
    RemoveWB();
    num_finished_instructions++;
}

// Privado:
// 3 etapas: (1) marca WR para instruções c/ destino; (2) broadcast CDB p/ liberar
// dependências; (3) switch por tipo p/ grupo RS correto (LOAD → rs.load, etc.)
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
        if (r.GetCurrentInstruction().GetPC() != pc) continue;
        if (dest.GetType() == 'Z' || dest.GetId() < 0 || dest.GetId() >= num_registers) continue;
        std::string rs_id = r.GetId();
        if (dest.GetType() == 'F') {
            int start_cycle = cdb.F[dest.GetId()].GetRSCycleStart(rs_id);
            cdb.F[dest.GetId()].DeallocateRS(rs_id, start_cycle, cycle);
        } else if (dest.GetType() == 'R') {
            int start_cycle = cdb.R[dest.GetId()].GetRSCycleStart(rs_id);
            cdb.R[dest.GetId()].DeallocateRS(rs_id, start_cycle, cycle);
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
                r.GetCurrentInstruction().GetPC(),
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
// varia por tipo: STORE simula latência MEM, BRANCH espera 2 ciclos EX,
// demais aguardam WR < ciclo atual. BRANCH s/ previsor trava no ciclo.
void Thread::Commit(
    int cycle
){
    if (!has_rob) return;

    int escritas{};
    while (!rob.empty() && escritas < fu.commit){
        TABLE_ROW& linha{instruction_table[commit_pointer]};
        INSTRUCTION_TYPE tipo = linha.instruction.GetInstructionType();
        bool store_with_rob = (tipo == INSTRUCTION_TYPE::STORE && has_rob);
        bool pronto = false;

        if (store_with_rob) {
            if (linha.store_commit_state == STORE_COMMIT_STATE::IDLE) {
                linha.store_commit_state = STORE_COMMIT_STATE::WAITING_MEM;
                linha.mem_cycles.push_back(cycle);
            }
            if (linha.store_commit_state == STORE_COMMIT_STATE::WAITING_MEM) {
                int fim_mem = linha.mem_cycles.back() + linha.instruction.GetMemLatency() - 1;
                if (cycle >= fim_mem) {
                    linha.store_commit_state = STORE_COMMIT_STATE::READY;
                    linha.mem_cycles.pop_back();
                    pronto = true;
                }
            }
        } else if (tipo == INSTRUCTION_TYPE::BRANCH) {
            pronto = (linha.ex_cycles.size() == 2);
        } else {
            pronto = (linha.wr_cycle != 0 && linha.wr_cycle < cycle);
        }

        if (pronto) {
            linha.commit_cycle = cycle;
            num_committed_instructions++;
            escritas++;
            commit_pointer++;
            rob.erase(rob.begin());
            if (tipo == INSTRUCTION_TYPE::BRANCH && !(has_predictor && has_rob)) break;
        }
        else break;
    }
}

} // namespace processor
