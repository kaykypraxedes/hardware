/* Thread.cpp */
#include "headers/Thread.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int num_rs_groups{6};
static const int num_fu_groups{6};

// ─── HELPERS ──────────────────────────────────────────────────────

/**
 * @brief Retorna um vetor com os ponteiros para todos os grupos de
 * RS para permitir navegar por todos os grupos de maneira
 * simplificada.
 *
 * @details Esse retorno em um vetor de ponteiros garante um grande
 * resumo de código (visto que ações que exigiriam uma operação para
 * cada um agora podem ser feitos em um loop).
 *
 * @param RESERVATION_STATION& rs - A Reservation Station inteira.
 *
 * @return std::vector<std::vector<RS>*> - Vetor com ponteiros para
 * cada grupo de RS.
 */
static std::vector<std::vector<RS>*> GetAllRSGroups(
    RESERVATION_STATION& rs
) {
    return {
        &rs.load,
        &rs.store,
        &rs.int_basic,
        &rs.int_mult_div,
        &rs.float_basic,
        &rs.float_mult_div
    };
}

/**
 * @brief Retorna um vetor com os ponteiros para todos os grupos de
 * FU para permitir navegar por todos os grupos de maneira
 * simplificada.
 *
 * @details Esse retorno em um vetor de ponteiros garante um grande
 * resumo de código (visto que ações que exigiriam uma operação para
 * cada um agora podem ser feitos em um loop).
 *
 * Os únicos elementos de "FUNCTIONAL_UNITS" que não são retornados
 * são o "wr" e o "commit" (que são "int"), exigindo operações
 * individuais para cada um.
 *
 * @param FUNCTIONAL_UNITS& fu - Todas as unidades funcionais.
 *
 * @return std::vector<std::vector<FU>*> - Vetor com
 * ponteiros para cada grupo de FU.
 */
static std::vector<std::vector<FU>*> GetAllFUGroups(
    FUNCTIONAL_UNITS& fu
) {
    return {
        &fu.memory_access,
        &fu.int_basic_alu,
        &fu.int_mult_div_alu,
        &fu.float_basic_alu,
        &fu.float_mult_div_alu
    };
}

static std::vector<RS>& GetRSGroupForType(
    RESERVATION_STATION&   rs,
    const INSTRUCTION_TYPE type
) {
    // Retorna a referência para o vetor do grupo de RS correto da instrução.
    switch (type) {
        case INSTRUCTION_TYPE::LOAD:
            return rs.load;
        case INSTRUCTION_TYPE::STORE:
            return rs.store;
        case INSTRUCTION_TYPE::FLOAT_BASIC:
            return rs.float_basic;
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:
            return rs.int_mult_div;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:
            return rs.float_mult_div;
        default: // Cobre BRANCH também.
            return rs.int_basic;
    }
}

static bool ComparePositionOnRS(
    const RS* a,
    const RS* b
) {
    // indicar ao "std::stable_sort()" o padrão de organização desejado (nesse caso, crescente).
    return a->GetCurrentInstruction().GetPosition() < b->GetCurrentInstruction().GetPosition();
}

static void ResolveDependencyInGroup(
    std::vector<RS>& group,
    const int        producer_position,
    const Register&  dest
){
    // Passa em um grupo de RS resolvendo dependências do produtor lógico.
    for (RS& dep : group)
        if (dep.IsBusy()) dep.ResolveDependency(producer_position, dest);
}

static void BroadcastOnCDBAndRS(
    CDB&                 cdb,
    RESERVATION_STATION& rs,
    const Register&      dest,
    const int            position,
    const int            cycle
){
    // Instrução sem registrador de destino válido.
    if (dest.GetType() == 'Z') return;

    // Desaloca diretamente a identidade lógica, sem depender da RS física atual.
    if (!cdb.register_status.DeallocateProducer(dest, position, cycle)) {
        std::cerr <<
            "[ERRO] Falha na desalocação do produtor!\n"
            "- Posição: " << position << '\n' <<
            "- Ciclo final: " << cycle << '\n';
        std::abort();
    }

    // Atualiza todos os Qs que aguardavam exatamente esse produtor.
    for (std::vector<RS>* group : GetAllRSGroups(rs))
        ResolveDependencyInGroup(*group, position, dest);
}

static void ReleaseRSByPosition(
    RESERVATION_STATION& rs,
    const int            position,
    const int            cycle
) {
    // Libera a única RS ocupada pela identidade lógica.
    for (std::vector<RS>* group : GetAllRSGroups(rs)) {
        for (RS& current : *group) {
            if (current.IsBusy() &&
                current.GetCurrentInstruction().GetPosition() == position) {
                current.Release(cycle);
                return;
            }
        }
    }

    std::cerr <<
        "[ERRO] RS da instrução não encontrada para liberação.\n"
        "- Posição: " << position << '\n' <<
        "- Ciclo: " << cycle << '\n';
    std::abort();
}

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Thread::GetCurrentInstructionPosition()      const { return current_instruction_position; }

// Público:
const CDB& Thread::GetCDB()                      const { return cdb; }

// Público:
const RESERVATION_STATION& Thread::GetRS()       const { return rs; }

// Público:
const FUNCTIONAL_UNITS& Thread::GetFU()          const { return fu; }

// Público:
const std::vector<TABLE_ROW>& Thread::GetTable() const { return instruction_table; }

// Público:
const std::vector<IN_FLIGHT_ENTRY>& Thread::GetInFlightEntries() const {
    return in_flight_entries;
}

// Público:
const std::deque<ROB_ENTRY>& Thread::GetROBEntries() const {
    return rob;
}

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Thread::Thread(
    const std::vector<std::string>&      assembly,
    const std::vector<LATENCY_OVERRIDE>& latency_overrides,
    const std::vector<int>&              num_rs,
    const std::vector<int>&              num_fus,
    const std::vector<int>&              switch_cycles,
    const int                            dispatch_width,
    const int                            rob_capacity,
    const bool                           has_predictor,
    const ARCHITECTURE                   arch
) :
    Thread(
        InstructionFactory::ParseTrace(assembly, arch),
        latency_overrides,
        num_rs,
        num_fus,
        switch_cycles,
        dispatch_width,
        rob_capacity,
        has_predictor,
        arch
    )
{}

// Público:
Thread::Thread(
    std::vector<std::unique_ptr<Instruction>> instructions,
    const std::vector<LATENCY_OVERRIDE>&      latency_overrides,
    const std::vector<int>&                   num_rs,
    const std::vector<int>&                   num_fus,
    const std::vector<int>&                   switch_cycles,
    const int                                 dispatch_width,
    const int                                 rob_capacity,
    const bool                                has_predictor,
    const ARCHITECTURE                        arch
) :
    has_rob        (rob_capacity > 0),
    rob_capacity   (rob_capacity > 0 ? rob_capacity : 1),
    has_predictor  (has_predictor),
    switch_cycles  (switch_cycles)
{
    // Verifica inconsistência de parâmetros.
    if (has_predictor && !has_rob) {
        std::cerr << "[ERRO] ROB obrigatório para previsor de desvios!\n";
        std::abort();
    }

    // Passa as instruções para a tabela e confirma a identidade posicional.
    for (std::size_t position{}; position < instructions.size(); position++) {
        std::unique_ptr<Instruction>& instruction{instructions[position]};
        if (!instruction || instruction->GetPosition() != static_cast<int>(position)) {
            std::cerr <<
                "[ERRO] Posição inválida na sequência de instruções.\n"
                "- Índice esperado: " << position << '\n';
            std::abort();
        }

        // Ignora propositalmente os outros valores de instruction_table para que eles recebam o default.
        // - Gera warning (por passar menos elementos do que deve na struct) ignorado pela diretiva.
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        // Converte de unique_ptr para shared_ptr ao guardar na tabela para que RS e ROB compartilhem o MESMO objeto.
        instruction_table.push_back({std::shared_ptr<Instruction>(std::move(instruction))});
        #pragma GCC diagnostic pop
    }

    // Inicializa os RSs e as FUs.
    InitializeComponents(num_rs, num_fus, dispatch_width, arch);

    // Passa os overrides vetoriais às instruções antes do primeiro Issue.
    for (const auto& [position, ex_latencies, mem_latencies] : latency_overrides) {
        if (static_cast<size_t>(position) < instruction_table.size()) {
            instruction_table[position].instruction->SetLatencies(
                ex_latencies,
                mem_latencies
            );
        }
    }
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Thread::InitializeComponents(
    const std::vector<int>& num_rs,
    const std::vector<int>& num_fus,
    const int               dispatch_width,
    const ARCHITECTURE      arch
){
    // Declara os registradores do banco (CDB montado por arquitetura):
    cdb = InstructionFactory::MakeCDB(arch);

    // Verifica se foram passados valores para RSs.
    std::vector<int> aux{num_rs.empty() ? std::vector<int>{5,5,5,4,3,2} : num_rs}; // Valores arbitrários de default.

    // A quantidade de valores para os grupos de RS é inválido.
    if(aux.size() != num_rs_groups){
        std::cerr << "[ERRO] Quantidade inválida de RSs: " << num_rs.size() << "\n";
        std::abort();
    }

    // Declara os componententes da RS:
    int i{};
    std::vector<std::string> rs_names{"load", "store", "int_basic", "int_mult_div", "float_basic", "float_mult_div"};
    for (std::vector<RS>* group : GetAllRSGroups(rs)) {
        for(int j{}; j < aux[i]; j++) group->push_back(RS(rs_names[i] + std::to_string(j)));
        i++;
    }

    // Verifica se foram passados valores para FUs.
    aux = num_fus.empty() ? std::vector<int>{1,1,1,1,1,2} : num_fus; // Valores arbitrários de default.

    // A quantidade de valores para as FUs é inválido.
    if(aux.size() != num_fu_groups){
        std::cerr << "[ERRO] Quantidade inválida de FUs: " << num_fus.size() << "\n";
        std::abort();
    }

    // Declara os componententes do FU:
    i = 0;
    for (std::vector<FU>* group : GetAllFUGroups(fu)) {
        for(int j{}; j < aux[i]; j++) group->push_back(FU{});
        i++;
    }
    // Valores int:
    fu.wr = aux[5];
    if(has_rob) fu.commit = dispatch_width; // Só inicializa commit se tem ROB.
}

// Público:
bool Thread::IsSwitchCycle() {
    // Não existem ciclos de troca de thread ou então já acabaram.
    if (switch_cycles.empty()) return false;

    // Não é o ciclo de troca ainda.
    if (current_instruction_position != switch_cycles.front()) return false;

    // Apaga o valor atual, já que o ciclo já passou.
    switch_cycles.erase(switch_cycles.begin());
    return true;
}

// ─── ISSUE ────────────────────────────────────────────────────────
// Público:
ISSUE_RESULT Thread::Issue(
    const int cycle
){
    // Etapas internas aptas possuem prioridade por posição lógica.
    for (IN_FLIGHT_ENTRY& entry : in_flight_entries) {
        if (entry.state != IN_FLIGHT_STATE::WAITING_ISSUE ||
            entry.next_issue_cycle > cycle) continue;

        const int position{entry.instruction->GetPosition()};
        ISSUE_RESULT result{TryIssue(position, cycle, false)};
        if (result.outcome != ISSUE_OUTCOME::BLOCKED) return result;
    }

    // Todas as instruções novas já foram adicionadas.
    if (current_instruction_position >= static_cast<int>(instruction_table.size())) return {};

    return TryIssue(current_instruction_position, cycle, true);
}

// Privado:
ISSUE_RESULT Thread::TryIssue(
    const int  position,
    const int  cycle,
    const bool new_instruction
) {
    std::shared_ptr<Instruction> instruction;
    std::size_t current_stage{};
    IN_FLIGHT_ENTRY* entry{};

    // A primeira admissão começa na etapa zero. Issues internos reutilizam o cursor existente.
    if (new_instruction) {
        if (position != static_cast<int>(in_flight_entries.size())) {
            std::cerr <<
                "[ERRO] Ordem inválida na criação da entrada in-flight.\n"
                "- Posição: " << position << '\n' <<
                "- Entradas existentes: " << in_flight_entries.size() << '\n';
            std::abort();
        }
        instruction = instruction_table[position].instruction;
    }
    else {
        entry = &GetInFlightEntry(position);
        if (entry->state != IN_FLIGHT_STATE::WAITING_ISSUE ||
            entry->next_issue_cycle > cycle) {
            std::cerr <<
                "[ERRO] Issue interno de entrada não elegível.\n"
                "- Posição: " << position << '\n' <<
                "- Estado: " << static_cast<int>(entry->state) << '\n' <<
                "- Próximo ciclo: " << entry->next_issue_cycle << '\n' <<
                "- Ciclo atual: " << cycle << '\n';
            std::abort();
        }
        instruction = entry->instruction;
        current_stage = entry->current_stage;
    }

    INSTRUCTION_TYPE type{instruction->GetInstructionType(current_stage)};

    // Verifica se a instrução é válida:
    if (type == INSTRUCTION_TYPE::INVALID) {
        std::cerr <<
            "[ERRO] Tentativa de adicionar instrução inválida no issue! \n" <<
            "- Instrução: " << instruction->GetInstructionString() << '\n'  <<
            "- Posição: "   << position << '\n';
        std::abort();
    }

    // Somente a primeira admissão cria uma entrada no ROB.
    if (has_rob) {
        const bool already_in_rob{IsInROB(position)};
        if (!new_instruction && !already_in_rob) {
            std::cerr <<
                "[ERRO] Issue interno sem entrada correspondente no ROB.\n"
                "- Posição: " << position << '\n';
            std::abort();
        }
        if (new_instruction && already_in_rob) {
            std::cerr <<
                "[ERRO] Tentativa de duplicar posição no ROB.\n"
                "- Posição: " << position << '\n';
            std::abort();
        }
        if (new_instruction && rob.size() >= static_cast<size_t>(rob_capacity)) return {};
    }

    // Identifica o tipo de grupo de RS necessário para alocar e procura uma vaga.
    std::vector<RS>& group{GetRSGroupForType(rs, type)};

    for (RS& r : group) {
        // Se conseguiu adicionar:
        if (r.AddIssue(
            instruction,
            cdb.register_status,
            cycle,
            new_instruction,
            current_stage
        )) {

            // 1. Marca na tabela o IS da instrução.
            instruction_table[position].issue_cycles.push_back(cycle);

            // Issue interno preserva frontend, branch e entrada do ROB.
            if (!new_instruction) {
                entry->state = IN_FLIGHT_STATE::ALLOCATED;
                entry->next_issue_cycle = -1;
                return {
                    ISSUE_OUTCOME::INTERNAL_STAGE,
                    position,
                    type
                };
            }

            // A entrada dinâmica nasce somente depois do primeiro Issue bem-sucedido.
            in_flight_entries.push_back({
                instruction,
                current_stage,
                IN_FLIGHT_STATE::ALLOCATED,
                -1
            });

            // 2.1. Se tem um ROB:
            if (has_rob){
                // Adiciona uma única entrada funcional para a instrução lógica.
                const std::size_t final_stage{instruction->GetStageCount() - 1};
                const bool final_store{
                    instruction->GetInstructionType(final_stage) == INSTRUCTION_TYPE::STORE
                };
                rob.push_back({
                    position,
                    false,
                    -1,
                    final_store ? ROB_STORE_STATE::WAITING_EXECUTION :
                        ROB_STORE_STATE::NOT_STORE
                });

                /*
                 * Vale ressaltar que não é feita a verificação se a instrução
                 * é um brach (mesmo que ele tenha ou não um previsor de desvios)
                 * pois com o ROB a instrução não é impedida de executar até a
                 * conclusão do desvio. No máximo, se não há previsor, o seu
                 * "issue" e seu "commit" são adiados em um ciclo.
                 */
            }
            // 2.2. Se não tem ROB:
            // - Se for um branch, marca para as instruções posteriores serem atrasadas.
            // - Obrigatóriamente sem previsor de desvio.
            else if (type == INSTRUCTION_TYPE::BRANCH)
                unresolved_branch_position = position;

            // 3. Passa para a próxima instrução.
            current_instruction_position++;

            return {
                ISSUE_OUTCOME::NEW_INSTRUCTION,
                position,
                type
            };
        }
    }
    return {};
}

// Privado:
bool Thread::IsInROB(
    const int position
) const {
    for (const ROB_ENTRY& entry : rob)
        if (entry.position == position) return true;
    return false;
}

// Privado:
ROB_ENTRY& Thread::GetROBEntry(
    const int position
) {
    for (ROB_ENTRY& entry : rob)
        if (entry.position == position) return entry;

    std::cerr <<
        "[ERRO] Entrada do ROB não encontrada.\n"
        "- Posição: " << position << '\n';
    std::abort();
}

// Privado:
void Thread::MarkROBReady(
    const int position,
    const int ready_cycle
) {
    ROB_ENTRY& entry{GetROBEntry(position)};
    if (entry.store_state != ROB_STORE_STATE::NOT_STORE ||
        entry.ready || entry.ready_cycle >= 0 || ready_cycle < 0) {
        std::cerr <<
            "[ERRO] Transição inválida para ROB pronto.\n"
            "- Posição: " << position << '\n' <<
            "- Pronto: " << entry.ready << '\n' <<
            "- Ciclo registrado: " << entry.ready_cycle << '\n' <<
            "- Novo ciclo: " << ready_cycle << '\n';
        std::abort();
    }

    entry.ready = true;
    entry.ready_cycle = ready_cycle;
}

// Privado:
void Thread::MarkStoreWaitingMemory(
    const int position
) {
    ROB_ENTRY& entry{GetROBEntry(position)};
    if (entry.store_state != ROB_STORE_STATE::WAITING_EXECUTION ||
        entry.ready || entry.ready_cycle >= 0) {
        std::cerr <<
            "[ERRO] STORE chegou à memória em estado inválido do ROB.\n"
            "- Posição: " << position << '\n' <<
            "- Estado: " << static_cast<int>(entry.store_state) << '\n';
        std::abort();
    }

    entry.store_state = ROB_STORE_STATE::WAITING_MEMORY;
}

// Privado:
void Thread::AdvanceStoreCommit(
    ROB_ENTRY&             rob_entry,
    const IN_FLIGHT_ENTRY& in_flight_entry,
    const int              cycle
) {
    // STORE ainda não concluiu a execução final.
    if (rob_entry.store_state == ROB_STORE_STATE::WAITING_EXECUTION) return;

    if (rob_entry.store_state == ROB_STORE_STATE::NOT_STORE) {
        std::cerr <<
            "[ERRO] Temporização de STORE solicitada para entrada comum.\n"
            "- Posição: " << rob_entry.position << '\n';
        std::abort();
    }

    // A memória começa somente quando o STORE concluído alcança a cabeça.
    if (rob_entry.store_state == ROB_STORE_STATE::WAITING_MEMORY) {
        const int memory_latency{
            in_flight_entry.instruction->GetMemLatency(in_flight_entry.current_stage)
        };
        if (memory_latency <= 0) {
            std::cerr <<
                "[ERRO] Latência de memória inválida para STORE.\n"
                "- Posição: " << rob_entry.position << '\n' <<
                "- Latência: " << memory_latency << '\n';
            std::abort();
        }

        rob_entry.store_state = ROB_STORE_STATE::EXECUTING_MEMORY;
        rob_entry.ready_cycle = cycle + memory_latency - 1;
    }

    if (rob_entry.store_state == ROB_STORE_STATE::EXECUTING_MEMORY &&
        cycle >= rob_entry.ready_cycle)
        rob_entry.ready = true;
}

// Privado:
IN_FLIGHT_ENTRY& Thread::GetInFlightEntry(
    const int position
) {
    if (position < 0 || position >= static_cast<int>(in_flight_entries.size()) ||
        !in_flight_entries[position].instruction ||
        in_flight_entries[position].instruction->GetPosition() != position) {
        std::cerr <<
            "[ERRO] Entrada in-flight inválida.\n"
            "- Posição: " << position << '\n' <<
            "- Entradas existentes: " << in_flight_entries.size() << '\n';
        std::abort();
    }
    return in_flight_entries[position];
}

// Privado:
bool Thread::HasFinishedExecution() const {
    if (in_flight_entries.size() != instruction_table.size()) return false;

    return std::all_of(
        in_flight_entries.begin(),
        in_flight_entries.end(),
        [](const IN_FLIGHT_ENTRY& entry) {
            return entry.state == IN_FLIGHT_STATE::FINISHED;
        }
    );
}

// ─── EX/MEM ───────────────────────────────────────────────────────
// Público:
bool Thread::ExMem(
    const int cycle
){
    // Com ROB: Verifica se todas as instruções já foram commitadas.
    if (static_cast<size_t>(num_committed_instructions) == instruction_table.size()) return true;

    // Sem ROB: Verifica se todas as instruções já passaram pelo WR.
    const bool execution_finished{HasFinishedExecution()};
    if (!has_rob && execution_finished) return true;

    // Se ainda faltar instruções a serem executadas, ele as executa no ciclo.
    if (!execution_finished) StartPhase(cycle);
    return false;
}

// Privado:
void Thread::StartPhase(
    const int cycle
){
    std::vector<RS*> candidates;

    // Adiciona as instruções aptas a iniciar sua fase de execução em um vetor.
    for (std::vector<RS>* group : GetAllRSGroups(rs)) {
        for (RS& r : *group) {

            // Célula da RS vazia.
            if (!r.IsBusy()) continue;

            // Filtro que garante selecionar apenas as instruções em IS ou MEM (que vai começar)
            // - São as únicas que podem ter o countdown = -1.
            // - Redundante em lógica, já que o ReservationStations::UpdateDependencies() já faz esse filtro, mas diminui overhead do sort.
            if (r.GetCountdown() != -1 || r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR) continue;

            // Instrução observada foi adicionada após um branch não resolvido:
            // - Tem sua execução atrasada (branch stall).
            // - Não verifica se tem ROB ou não pois o "unresolved_branch_position >= 0" só ocorre sem ROB.
            int inst_position{r.GetCurrentInstruction().GetPosition()};
            if (unresolved_branch_position >= 0 && inst_position > unresolved_branch_position) continue;

            // Previne que STORE com ROB seja escalonado para WR (nesse caso é apenas commit).
            IN_FLIGHT_ENTRY& entry{GetInFlightEntry(inst_position)};
            INSTRUCTION_TYPE type{
                entry.instruction->GetInstructionType(entry.current_stage)
            };
            const bool final_stage{
                entry.current_stage + 1 == entry.instruction->GetStageCount()
            };
            if (type == INSTRUCTION_TYPE::STORE && has_rob && final_stage &&
                r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) continue;

            candidates.push_back(&r);
        }
    }

    // Ordena os candidatos com base na sua ordenação original.
    std::stable_sort(candidates.begin(), candidates.end(), ComparePositionOnRS);

    // Avança as instruções de fase:
    for (RS* r : candidates) {
        // Verifica se conseguiu mudar a instrução de fase:
        if (r->UpdateDependencies(cdb.register_status, fu, cycle)) {

            // Marca na tabela:
            int position{r->GetCurrentInstruction().GetPosition()};
            IN_FLIGHT_ENTRY& entry{GetInFlightEntry(position)};

            if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX) {
                if (entry.state != IN_FLIGHT_STATE::ALLOCATED) {
                    std::cerr <<
                        "[ERRO] Entrada iniciou EX fora do estado ALLOCATED.\n"
                        "- Posição: " << position << '\n' <<
                        "- Estado: " << static_cast<int>(entry.state) << '\n';
                    std::abort();
                }
                entry.state = IN_FLIGHT_STATE::EXECUTING;
                instruction_table[position].ex_cycles.push_back(cycle);
            }

            else if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) {
                if (entry.state != IN_FLIGHT_STATE::EXECUTING) {
                    std::cerr <<
                        "[ERRO] Entrada iniciou MEM fora do estado EXECUTING.\n"
                        "- Posição: " << position << '\n' <<
                        "- Estado: " << static_cast<int>(entry.state) << '\n';
                    std::abort();
                }
                instruction_table[position].mem_cycles.push_back(cycle);
            }
        }
    }
}

// ─── WR ───────────────────────────────────────────────────────────
// Público:
void Thread::Wr(
    const int cycle
){
    // Pega os WR que estavam pendentes e coloca no buffer.
    for (int p : pending_wr_buffer)
        wr_buffer.push_back(p);
    pending_wr_buffer.clear();

    // Ordena o buffer de WR.
    std::sort(wr_buffer.begin(), wr_buffer.end());

    // Escreve os resultados prontos.
    PerformWriteResult(cycle);

    // Detecta novas transições de fase.
    DetectPhaseTransitions(cycle);
}

// Privado:
void Thread::PerformWriteResult(
    const int cycle
){
    int writes{};
    while (!wr_buffer.empty() && writes < fu.wr) { // Limitado pela capacidade de WR simultâneo.

        int position{wr_buffer.front()};
        IN_FLIGHT_ENTRY& entry{GetInFlightEntry(position)};
        INSTRUCTION_TYPE instr_type{
            entry.instruction->GetInstructionType(entry.current_stage)
        };
        bool store_with_rob{instr_type == INSTRUCTION_TYPE::STORE && has_rob};

        if (entry.state != IN_FLIGHT_STATE::WAITING_WR) {
            std::cerr <<
                "[ERRO] Entrada chegou ao WR fora do estado WAITING_WR.\n"
                "- Posição: " << position << '\n' <<
                "- Estado: " << static_cast<int>(entry.state) << '\n';
            std::abort();
        }

        // O estágio "wr" do "store" só é marcado se o o processador não possui ROB.
        if (store_with_rob) {
            ReleaseRSByPosition(rs, position, cycle);
            wr_buffer.erase(wr_buffer.begin());
            entry.state = IN_FLIGHT_STATE::FINISHED;
            MarkStoreWaitingMemory(position);
            continue; // Não conta como um WR (não há resultado sendo escrito de fato).
        }

        // Faz a marcação na tabela:
        // - "stores" e "branches" não escrevem em nenhum resultado em registradores ("wr" nulo).
        if (instr_type != INSTRUCTION_TYPE::STORE && instr_type != INSTRUCTION_TYPE::BRANCH)
            instruction_table[position].wr_cycle = cycle;

        // Propaga a informação nos componentes.
        WriteResultOnComponents(position, cycle);
        wr_buffer.erase(wr_buffer.begin());
        entry.state = IN_FLIGHT_STATE::FINISHED;

        // Resultados comuns só podem receber Commit a partir do próximo ciclo.
        // Branch é marcado na própria resolução do EX.
        if (has_rob && instr_type != INSTRUCTION_TYPE::BRANCH)
            MarkROBReady(position, cycle + 1);

        // Branch não ocupa porta de WR (não escreve em registrador nenhum).
        if (instr_type != INSTRUCTION_TYPE::BRANCH) writes++;
    }
}

// Privado:
void Thread::WriteResultOnComponents(
    const int position,
    const int cycle
){
    // Nesse mesmo loop:
    // 1. Propaga o resultado no CDB e libera os registradores.
    // 2. Resolve as dependências nos RSs que estavam esperando.
    const std::vector<Register>& dests{instruction_table[position].instruction->GetDestRegisters()};
    // Instrução pode ter mais de um destino (ex: x86 reg + EFLAGS).
    // - O broadcast é feito para cada destino, um por um.
    for (const Register& dest : dests)
        BroadcastOnCDBAndRS(cdb, rs, dest, position, cycle);

    // Libera a célula da RS produtora.
    ReleaseRSByPosition(rs, position, cycle);
}

// Privado:
void Thread::DetectPhaseTransitions(
    const int cycle
){
    // Procura em todos os grupos de RS instruções que finalizaram uma fase:
    // - EX_inicio   -> EX_concluido  (próximo é o MEM ou o WR).
    // - MEM _inicio -> MEM_concluido (próximo éWR)
    for (std::vector<RS>* group : GetAllRSGroups(rs)) {
        for (RS& r : *group) {

            // Célula da RS vazia.
            if (!r.IsBusy()) continue;

            // Verifica se a fase mudou com o incremento do contador:
            // - Guarda a fase antes da tentativa, para comparar com a fase depois.
            INSTRUCTION_PHASE_TOMASULO phase_before{r.GetInstructionPhase()};

            // Ainda executando.
            if (!r.UpdateCountdown(fu, cycle)) continue;

            INSTRUCTION_PHASE_TOMASULO phase_after{r.GetInstructionPhase()};
            int position{r.GetCurrentInstruction().GetPosition()};
            IN_FLIGHT_ENTRY& entry{GetInFlightEntry(position)};
            if (entry.state != IN_FLIGHT_STATE::EXECUTING) {
                std::cerr <<
                    "[ERRO] Fase concluída fora do estado EXECUTING.\n"
                    "- Posição: " << position << '\n' <<
                    "- Estado: " << static_cast<int>(entry.state) << '\n';
                std::abort();
            }
            INSTRUCTION_TYPE type{
                entry.instruction->GetInstructionType(entry.current_stage)
            };
            bool has_mem{type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE};
            bool final_stage{
                entry.current_stage + 1 == entry.instruction->GetStageCount()
            };
            bool store_with_rob{type == INSTRUCTION_TYPE::STORE && has_rob && final_stage};

            // Caso 1: EX finalizado: falta MEM.
            if (phase_before == INSTRUCTION_PHASE_TOMASULO::EX && phase_after == INSTRUCTION_PHASE_TOMASULO::MEM) {
                // Marca na tabela.
                instruction_table[position].ex_cycles.push_back(cycle);
                // O ciclo MEM do STORE é representado apenas quando ele não possui ROB.
                // - Pula direto pro WR.
                if (store_with_rob) {
                    entry.state = IN_FLIGHT_STATE::WAITING_WR;
                    pending_wr_buffer.push_back(position);
                }
            }
            // Caso 2: * finalizado: falta WR.
            // - Não precisa verificar o phase_before por que mudou de fase para o final.
            else if (phase_after == INSTRUCTION_PHASE_TOMASULO::WR) {
                // Se tem MEM e não é o caso especial de STORE+ROB (que já marcou o próprio ciclo de EX acima).
                if (has_mem && !store_with_rob)
                    instruction_table[position].mem_cycles.push_back(cycle);
                // Fim da execução das demais.
                else if (!has_mem)
                    instruction_table[position].ex_cycles.push_back(cycle);

                // Se o Branch foi resolvido, a flag é desmarcada.
                if (position == unresolved_branch_position &&
                    r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR)
                    unresolved_branch_position = -1; // Valor default.

                // Etapa intermediária libera hardware sem produzir resultado arquitetural.
                if (!final_stage) {
                    ReleaseRSByPosition(rs, position, cycle);
                    entry.current_stage++;
                    entry.state = IN_FLIGHT_STATE::WAITING_ISSUE;
                    entry.next_issue_cycle = cycle + 1;
                    continue;
                }

                // Branch final pode receber Commit no ciclo em que seu EX é resolvido.
                if (has_rob && type == INSTRUCTION_TYPE::BRANCH)
                    MarkROBReady(position, cycle);

                // Coloca a instrução na fila de WR.
                entry.state = IN_FLIGHT_STATE::WAITING_WR;
                pending_wr_buffer.push_back(position);
            }
        }
    }
}

// ─── COMMIT ───────────────────────────────────────────────────────
// Público:
void Thread::Commit(
    const int cycle
){
    // Apenas realiza commit se possui ROB.
    if (!has_rob) return;

    int writes{};
    // Até acabar as instruções ou até o limite de despacho.
    while (!rob.empty() && writes < fu.commit){
        ROB_ENTRY& rob_entry{rob.front()};
        const int position{rob_entry.position};
        IN_FLIGHT_ENTRY& in_flight_entry{GetInFlightEntry(position)};
        const std::size_t final_stage{
            in_flight_entry.instruction->GetStageCount() - 1
        };
        const INSTRUCTION_TYPE type{
            in_flight_entry.instruction->GetInstructionType(final_stage)
        };

        // STORE avança sua temporização somente quando está na cabeça.
        if (type == INSTRUCTION_TYPE::STORE)
            AdvanceStoreCommit(rob_entry, in_flight_entry, cycle);

        // A cabeça bloqueia todas as entradas posteriores até ficar pronta.
        if (!rob_entry.ready || cycle < rob_entry.ready_cycle) break;

        // Marca na tabela:
        instruction_table[position].commit_cycle = cycle;
        num_committed_instructions++;
        writes++;
        rob.pop_front();

        // Se não tem previsor, as instruções após um Branch tem que ser em outro ciclo.
        if (type == INSTRUCTION_TYPE::BRANCH && !(has_predictor && has_rob)) break;
    }
}

} // namespace processor
