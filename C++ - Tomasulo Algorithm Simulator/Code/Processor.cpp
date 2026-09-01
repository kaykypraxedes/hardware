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
    const std::vector<std::string>&      assembly,
    std::vector<Thread>&                 threads,
    const int                            num_threads,
    const bool                           has_predictor,
    const PIPELINE_CONFIGURATION&        configuration,
    const std::vector<int>&              switch_cycles,
    const std::vector<LATENCY_OVERRIDE>& latency_overrides,
    const ARCHITECTURE                   arch,
    const PROCESSOR_TYPE                 type
){
    PIPELINE_CONFIGURATION thread_configuration{configuration};
    if (type != PROCESSOR_TYPE::TOMASULO_ESPECULATIVE)
        thread_configuration.rob_capacity = 0;

    for (int i{}; i < num_threads; i++){
        threads.push_back(
            Thread(
                assembly,
                thread_configuration,
                latency_overrides,
                switch_cycles,
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
    const int                            num_threads,
    const bool                           has_predictor,
    const PROCESSOR_TYPE                 type,
    const MULTITHREADING_MODEL           mt_model,
    const std::vector<std::string>&      Assembly,
    const PIPELINE_CONFIGURATION&        configuration,
    const std::vector<int>&              switch_instructions,
    const std::vector<LATENCY_OVERRIDE>& latency_overrides,
    const ARCHITECTURE                   arch
) :
    configuration (configuration),
    has_predictor (has_predictor),
    type          (type),
    mt_model      (mt_model)
{
    ValidatePipelineConfiguration(this->configuration);

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
        has_predictor,
        this->configuration,
        switch_instructions,
        latency_overrides,
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

        // 3. Marca o Commit das instruções prontas no ROB.
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
    while (dispatched < configuration.issue_width) {

        // Verificou em todas as threads e nenhuma estava disponível.
        if (thread_turns >= total_threads) break;

        ISSUE_RESULT issue_result{threads[thread_pointer].Issue(current_cycle)};

        // Issue deu certo.
        if (issue_result.outcome != ISSUE_OUTCOME::BLOCKED) {
            dispatched++;
            const bool new_instruction{
                issue_result.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION
            };

            // Verifica se a instrução é um Branch:
            // - Precisa interromper a adição de instruções no Issue se não tem previsor.
            if (new_instruction &&
                issue_result.instruction_type == INSTRUCTION_TYPE::BRANCH &&
                mt_model != MULTITHREADING_MODEL::SMT && // Como SMT rotaciona, instruções após o Branch não são lançadas.
                !has_predictor)
                break;

            // SMT: rotaciona após issue bem-sucedido:
            // - Garante alternância entre threads a cada dispatch.
            if (mt_model == MULTITHREADING_MODEL::SMT) AdvanceRoundRobinPointer(thread_pointer, threads.size());
            // Granulação Grossa: só rotaciona quando a instrução num "stall longo" (switch_instructions):
            // - Simula casos como um cache miss que fariam a rotação na Granulação Grossa.
            else if (new_instruction && mt_model == MULTITHREADING_MODEL::COARSE_GRAINED &&
                threads[thread_pointer].IsSwitchCycle())
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
