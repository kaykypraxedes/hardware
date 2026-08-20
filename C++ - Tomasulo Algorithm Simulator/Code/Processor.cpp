/* Processor.cpp */
#include "headers/Processor.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────
static void InitializeThreads(
    const std::vector<std::string>&             assembly,
    std::vector<Thread>&                        threads,
    const int                                   num_threads,
    const int                                   dispatch_width,
    const bool                                  has_predictor,
    const std::vector<int>&                     num_rs,
    const std::vector<int>&                     num_fus,
    const std::vector<int>&                     switch_cycles,
    const std::vector<std::tuple<int,int,int>>& new_latency,
    const ARCHITECTURE                          arch,
    const PROCESSOR_TYPE                        type
){
    int rob_capacity{(type == PROCESSOR_TYPE::TOMASULO_ESPECULATIVE) ? Processor::base_rob_capacity : 0};
    for (int i{}; i < num_threads; i++){
        threads.push_back(
            Thread(
                assembly,
                new_latency,
                num_rs,
                num_fus,
                switch_cycles,
                dispatch_width,
                rob_capacity,
                has_predictor,
                arch
            )
        );
    }
}

static void AdvanceRoundRobinPointer(
    int&         thread_pointer,
    const size_t threads_size
){
    thread_pointer = (thread_pointer + 1) % static_cast<int>(threads_size);
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── ELEMENTO STATIC ──────────────────────────────────────────────
int base_rob_capacity{32};

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
PROCESSOR_TYPE Processor::GetType() const { return type; }

// Público:
const Thread& Processor::GetThread(
    const int i
) const { return threads[i]; }

// Público:
const std::vector<TABLE_ROW>& Processor::GetThreadTable(
    const int i
) const { return threads[i].GetTable(); }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Processor::Processor(
    const int                                   num_threads,
    const int                                   dispatch_width,
    const bool                                  has_predictor,
    const PROCESSOR_TYPE                        type,
    const MULTITHREADING_MODEL                  mt_model,
    const std::vector<std::string>&             Assembly,
    const std::vector<int>&                     num_rs,
    const std::vector<int>&                     num_fus,
    const std::vector<int>&                     switch_instructions,
    const std::vector<std::tuple<int,int,int>>& new_latency,
    const ARCHITECTURE                          arch
) :
    dispatch_width(dispatch_width),
    has_predictor (has_predictor),
    type          (type),
    mt_model      (mt_model)
{
    // Verifica inconsistência de parâmetros.
    if (mt_model == MULTITHREADING_MODEL::NONE && num_threads > 1) {
        std::cerr << "[ERRO] Para mais de uma thread é obrigatório um modelo de multithreading!\n" <<
        "- Número de threads: " << num_threads << '\n';
        std::abort();
    }

    /**
     * Não precisa verificar se "num_threads == 1 &&
     * mt_model != MULTITHREADING_MODEL::NONE", já que Todos os modelos
     * teriam o mesmo resultado com uma thread.
     */

    // Inicializa as threads.
    InitializeThreads(
        Assembly,
        threads,
        num_threads,
        dispatch_width,
        has_predictor,
        num_rs,
        num_fus,
        switch_instructions,
        new_latency,
        arch,
        type
    );
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool Processor::ExecuteCycle() {

    // Executa as fases "ex", "mem", "wr" e "commit" das instruções já adicionadas.
    if(ExecuteOperations()) return true; // Todas as instruções já foram finalizadas.

    // Adiciona as instruções no RS (Issue).
    AddInstructions();

    current_cycle++;
    return false;
}

// Privado:
bool Processor::ExecuteOperations() {
    bool all_done{true};
    for (Thread& t : threads) {

        // 1. Pega as instruções que foram liberadas no ciclo anterior para começar suas operações.
        bool exmem_done{t.ExMem(current_cycle)};

        // 2. Pega as instruções que foram liberadas nesse ciclo para marcar na tabela.
        // - Não é antes do EX/MEM para não realizar operações liberadas no ciclo atual.
        t.Wr(current_cycle);

        // 3. Marca o commit das instruções disponíveis na tabela, libera as RSs e os regs no CDB.
        // - Só atualiza se o WR já tiver liberado a pelo menos um cíclo (ordem dele com o WR não interfere).
        t.Commit(current_cycle);

        all_done &= exmem_done;
    }
    return all_done;
}

// Privado:
void Processor::AddInstructions() {
    int dispatched{};
    int thread_turns{};
    int total_threads{static_cast<int>(threads.size())};

    // O loop ocorre até faz até:
    // 1. Achar uma thread disponível para o Issue (de acordo com as regras do modelo).
    // 2. Completar o ciclo de threads (nenhuma disponível).
    while (dispatched < dispatch_width) {

        // Verificou em todas as threads e nenhuma estava disponível.
        if (thread_turns >= total_threads) break;

        bool ok{threads[thread_pointer].Issue(current_cycle)};
        // Issue deu certo.
        if (ok) {
            dispatched++;
            Instruction& i{*threads[thread_pointer].GetTable()[threads[thread_pointer].GetCurrentInstructionPosition() - 1].instruction};
            // Verifica se a instrução é um Branch:
            // - Precisa interromper a adição de instruções no Issue se não tem previsor.
            if(i.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
                mt_model != MULTITHREADING_MODEL::SMT && // Como SMT rotaciona, instruções após o Branch não são lançadas.
                !has_predictor)
                break;
            // SMT: rotaciona após issue bem-sucedido:
            // - Garante alternância entre threads a cada dispatch.
            if (mt_model == MULTITHREADING_MODEL::SMT) AdvanceRoundRobinPointer(thread_pointer, threads.size());
            // Granulação Grossa: só rotaciona quando a instrução num "stall longo" (switch_instructions):
            // - Simula casos como um cache miss que fariam a rotação na Granulação Grossa.
            else if (mt_model == MULTITHREADING_MODEL::COARSE_GRAINED && threads[thread_pointer].IsSwitchCycle())
                AdvanceRoundRobinPointer(thread_pointer, threads.size());
            // Granulação Fina: Mantém a mesma thread (sem alteração).
        }
        // Issue deu errado.
        else {
            // Se a thread inicial apontada não está disponível, faz o round robin para a próxima.
            thread_turns++;
            if (mt_model == MULTITHREADING_MODEL::SMT || dispatched == 0)
                AdvanceRoundRobinPointer(thread_pointer, threads.size());
            // Se já realizou uma operação e não pode emitir outra instrução de outra thread no ciclo, encerra.
            else break;
        }
    }
}

} // namespace processor
