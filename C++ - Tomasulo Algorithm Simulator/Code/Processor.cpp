/* Processor.cpp */
#include "headers/Processor.h"

namespace processor {

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
PROCESSOR_TYPE Processor::GetType() const { return type; }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const Thread& Processor::GetThread(int i) const { return threads[i]; }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const std::vector<TABLE_ROW>& Processor::GetThreadTable(int i) const { return threads[i].GetTable(); }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Processor::Processor(
    int                             num_threads,
    int                             dispatch_width,
    bool                            has_predictor,
    PROCESSOR_TYPE                  type,
    MULTITHREADING_MODEL            model,
    const std::vector<std::string>& Assembly,
    const std::vector<int>&         num_rs,
    const std::vector<int>&         num_fus,
    const std::vector<int>&         switch_instructions
):
    dispatch_width(dispatch_width),
    has_predictor (has_predictor),
    type          (type),
    mt_model      (model)
{
    bool has_rob{type == PROCESSOR_TYPE::TOMASULO_WITH_ROB};
    if (switch_instructions.empty())
        InitializeThreads(Assembly, has_rob, num_threads, num_rs, num_fus);
    else
        InitializeThreads(Assembly, has_rob, num_threads, num_rs, num_fus, switch_instructions);
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Processor::InitializeThreads(
    const std::vector<std::string>& Assembly,
    bool                            has_rob,
    int                             num_threads,
    const std::vector<int>&         num_rs,
    const std::vector<int>&         num_fus,
    const std::vector<int>&         switch_instructions
){
    for (int i = 0; i < num_threads; i++) {
        if (mt_model == MULTITHREADING_MODEL::COARSE_GRAINED)
            threads.push_back(Thread(switch_instructions, Assembly, has_rob, num_rs, num_fus, dispatch_width, ROB_CAPACITY_DEFAULT, has_predictor));
        else
            threads.push_back(Thread(Assembly, has_rob, num_rs, num_fus, dispatch_width, ROB_CAPACITY_DEFAULT, has_predictor));
    }
}

// Público:
bool Processor::ExecuteCycle() {
    if(ExecuteExMemWr()) return true;
    // Reservation Stations são liberadas no mesmo ciclo em que a instrução deixa a estação, podendo ser reutilizadas imediatamente por novas instruções emitidas no mesmo ciclo
    ExecuteIssue();
    current_cycle++;
    return false;
}

// Privado:
// ExMem antes de Wr: a contagem é decrementada depois de registrar o início.
bool Processor::ExecuteExMemWr() {
    bool executou_todos{true};
    for (Thread& t : threads) {
        bool exmem_done = t.ExMem(current_cycle);
        t.Wr(current_cycle);
        t.Commit(current_cycle); // commit após WR: ciclo_WR já preenchido
        executou_todos &= exmem_done;
    }
    return executou_todos;
}

// Privado:
// Loop de dispatch até dispatch_width. A política de escalonamento varia conforme mt_model:
// - FINE_GRAINED mantém thread até falhar;
// - SMT rotaciona a cada despacho,
// - BRANCH s/ previsor interrompe o ciclo.
void Processor::ExecuteIssue() {
    int despachadas   = 0;
    int voltas        = 0;
    int total_threads = static_cast<int>(threads.size());

    while (despachadas < dispatch_width) {
        if (voltas >= total_threads) break;

        bool ok = threads[thread_pointer].Issue(current_cycle);
        if (ok) {
            despachadas++;
            if(threads[thread_pointer].GetTable()[threads[thread_pointer].GetPC() - 1].instruction.GetInstructionType() ==
                INSTRUCTION_TYPE::BRANCH && mt_model != MULTITHREADING_MODEL::SMT
                && !has_predictor) // Instrução é um branch
                break;
            // SMT: rotaciona após issue bem-sucedido
            // (garante alternância entre threads a cada despacho)
            if (mt_model == MULTITHREADING_MODEL::SMT) AdvanceRoundRobinPointer();
        } else {
            voltas++;
            if (mt_model == MULTITHREADING_MODEL::SMT || despachadas == 0)
                AdvanceRoundRobinPointer();
            else
                break;
        }
    }
}

// Privado:
void Processor::AdvanceRoundRobinPointer() {
    thread_pointer = (thread_pointer + 1) % static_cast<int>(threads.size());
}

} // namespace processor
