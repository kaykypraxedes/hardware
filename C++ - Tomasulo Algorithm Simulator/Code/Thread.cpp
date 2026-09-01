/* Thread.cpp */
#include "headers/Thread.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Thread::GetCurrentInstructionPosition()      const { return current_instruction_position; }

// Público:
const RegisterStatusTable& Thread::GetRegisterStatus() const {
    return register_status;
}

// Público:
const std::vector<REGISTER_BANK>& Thread::GetRegisterBanks() const {
    return register_banks;
}

// Público:
const RESERVATION_STATION& Thread::GetRS()       const { return rs; }

// Público:
const FUNCTIONAL_UNITS& Thread::GetFU()          const { return fu; }

// Público:
const std::vector<TABLE_ROW>& Thread::GetTable() const { return instruction_trace; }

// Público:
const std::vector<IN_FLIGHT_ENTRY>& Thread::GetInFlightEntries() const {
    return in_flight_entries;
}

// Público:
const std::deque<ROB_ENTRY>& Thread::GetROBEntries() const {
    return rob;
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
void Thread::SetTraceEnabled(
    const bool enabled
) {
    trace_enabled = enabled;
}

// Público:
void Thread::ClearTrace() {
    // Preserva a identidade observada e limpa somente os eventos.
    for (TABLE_ROW& row : instruction_trace) {
        row.issue_cycles.clear();
        row.ex_cycles.clear();
        row.mem_cycles.clear();
        row.wr_cycle = -1;
        row.commit_cycle = -1;
    }
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

    // Separa o programa imutável das linhas usadas somente para observação.
    for (std::size_t position{}; position < instructions.size(); position++) {
        std::unique_ptr<Instruction>& instruction{instructions[position]};
        if (!instruction || instruction->GetPosition() != static_cast<int>(position)) {
            std::cerr <<
                "[ERRO] Posição inválida na sequência de instruções.\n"
                "- Índice esperado: " << position << '\n';
            std::abort();
        }

        // Converte uma única vez e compartilha a descrição com pipeline e trace.
        std::shared_ptr<Instruction> shared_instruction{std::move(instruction)};
        this->instructions.push_back(shared_instruction);

        TABLE_ROW row{};
        row.instruction = shared_instruction;
        instruction_trace.push_back(row);
    }

    // Inicializa os RSs e as FUs.
    InitializeComponents(num_rs, num_fus, dispatch_width, arch);

    // Passa os overrides vetoriais às instruções antes do primeiro Issue.
    for (const auto& [position, ex_latencies, mem_latencies] : latency_overrides) {
        if (static_cast<size_t>(position) < this->instructions.size()) {
            this->instructions[position]->SetLatencies(
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
    // Declara o estado persistente e separa os metadados de apresentação.
    const REGISTER_LAYOUT layout{InstructionFactory::MakeRegisterLayout(arch)};
    register_status = RegisterStatusTable(layout.references);
    register_banks = layout.banks;

    // Delega a criação e a validação dos grupos ao banco de RSs.
    std::vector<int> aux{num_rs.empty() ? std::vector<int>{5,5,5,4,3,2} : num_rs}; // Valores arbitrários de default.
    rs = RESERVATION_STATION(aux);

    // Delega grupos, WR e Commit ao banco de FUs.
    aux = num_fus.empty() ? std::vector<int>{1,1,1,1,1,2} : num_fus; // Valores arbitrários de default.
    fu = FUNCTIONAL_UNITS(aux, has_rob ? dispatch_width : 0);
}

// Privado:
void Thread::RecordTraceEvent(
    const int         position,
    const TRACE_EVENT event,
    const int         cycle
) {
    // Trace desabilitado não participa nem valida decisões funcionais.
    if (!trace_enabled) return;

    if (position < 0 || position >= static_cast<int>(instruction_trace.size())) {
        std::cerr <<
            "[ERRO] Posição inválida para registro no trace.\n"
            "- Posição: " << position << '\n' <<
            "- Linhas existentes: " << instruction_trace.size() << '\n';
        std::abort();
    }

    TABLE_ROW& row{instruction_trace[position]};
    switch (event) {
        case TRACE_EVENT::ISSUE:
            row.issue_cycles.push_back(cycle);
            break;
        case TRACE_EVENT::EX_BOUNDARY:
            row.ex_cycles.push_back(cycle);
            break;
        case TRACE_EVENT::MEM_BOUNDARY:
            row.mem_cycles.push_back(cycle);
            break;
        case TRACE_EVENT::WR:
            row.wr_cycle = cycle;
            break;
        case TRACE_EVENT::COMMIT:
            row.commit_cycle = cycle;
            break;
    }
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
    if (current_instruction_position >= static_cast<int>(instructions.size())) return {};

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
        instruction = instructions[position];
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

    // O banco identifica o grupo e tenta a primeira célula livre.
    if (rs.AddIssue(
            instruction,
            register_status,
            cycle,
            new_instruction,
            current_stage
        )) {

            // Issue interno preserva frontend, branch e entrada do ROB.
            if (!new_instruction) {
                entry->state = IN_FLIGHT_STATE::ALLOCATED;
                entry->next_issue_cycle = -1;
                RecordTraceEvent(position, TRACE_EVENT::ISSUE, cycle);
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
            RecordTraceEvent(position, TRACE_EVENT::ISSUE, cycle);

            return {
                ISSUE_OUTCOME::NEW_INSTRUCTION,
                position,
                type
            };
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
    if (in_flight_entries.size() != instructions.size()) return false;

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
    if (static_cast<size_t>(num_committed_instructions) == instructions.size()) return true;

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
    const std::vector<RS*> ready_candidates{rs.CollectReadyCandidates()};
    std::vector<RS*> candidates;

    // Aplica somente políticas da Thread sobre candidatas já ordenadas pelo banco.
    for (RS* r : ready_candidates) {
        // Instrução observada foi adicionada após um branch não resolvido:
        // - Tem sua execução atrasada (branch stall).
        // - Não verifica se tem ROB ou não pois o "unresolved_branch_position >= 0" só ocorre sem ROB.
        int inst_position{r->GetCurrentInstruction().GetPosition()};
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
            r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) continue;

        candidates.push_back(r);
    }

    // Avança as instruções de fase:
    for (RS* r : candidates) {
        // Verifica se conseguiu mudar a instrução de fase:
        if (r->UpdateDependencies(register_status, fu, cycle)) {

            // Obtém a entrada funcional associada à RS atual.
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
                RecordTraceEvent(position, TRACE_EVENT::EX_BOUNDARY, cycle);
            }

            else if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) {
                if (entry.state != IN_FLIGHT_STATE::EXECUTING) {
                    std::cerr <<
                        "[ERRO] Entrada iniciou MEM fora do estado EXECUTING.\n"
                        "- Posição: " << position << '\n' <<
                        "- Estado: " << static_cast<int>(entry.state) << '\n';
                    std::abort();
                }
                RecordTraceEvent(position, TRACE_EVENT::MEM_BOUNDARY, cycle);
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
    while (!wr_buffer.empty() && writes < fu.GetWriteResultWidth()) {

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
            rs.ReleaseByPosition(position, cycle);
            wr_buffer.erase(wr_buffer.begin());
            entry.state = IN_FLIGHT_STATE::FINISHED;
            MarkStoreWaitingMemory(position);
            continue; // Não conta como um WR (não há resultado sendo escrito de fato).
        }

        // Propaga a informação nos componentes.
        WriteResultOnComponents(position, cycle);
        wr_buffer.erase(wr_buffer.begin());
        entry.state = IN_FLIGHT_STATE::FINISHED;

        // Resultados comuns só podem receber Commit a partir do próximo ciclo.
        // Branch é marcado na própria resolução do EX.
        if (has_rob && instr_type != INSTRUCTION_TYPE::BRANCH)
            MarkROBReady(position, cycle + 1);

        // Somente resultados arquiteturais produzem evento observacional de WR.
        if (instr_type != INSTRUCTION_TYPE::STORE && instr_type != INSTRUCTION_TYPE::BRANCH)
            RecordTraceEvent(position, TRACE_EVENT::WR, cycle);

        // Branch não ocupa porta de WR (não escreve em registrador nenhum).
        if (instr_type != INSTRUCTION_TYPE::BRANCH) writes++;
    }
}

// Privado:
void Thread::WriteResultOnComponents(
    const int position,
    const int cycle
){
    // Cada destino produz um evento independente no CDB.
    const std::vector<Register>& dests{instructions[position]->GetDestRegisters()};
    // Instrução pode ter mais de um destino (ex: x86 reg + EFLAGS).
    // - O broadcast é feito para cada destino, um por um.
    for (const Register& dest : dests) {
        if (dest.GetType() == 'Z') continue;
        BroadcastResult({position, dest}, cycle);
    }

    // Libera a célula da RS produtora.
    rs.ReleaseByPosition(position, cycle);
}

// Privado:
void Thread::BroadcastResult(
    const CDB_BROADCAST& broadcast,
    const int            cycle
) {
    // A conclusão persistente deve ocorrer uma única vez antes da entrega.
    if (!broadcast.CompleteProducer(register_status, cycle)) {
        std::cerr <<
            "[ERRO] Falha na desalocação do produtor!\n"
            "- Posição: " << broadcast.producer_position << '\n' <<
            "- Ciclo final: " << cycle << '\n';
        std::abort();
    }

    // O banco de RSs conhece os grupos físicos e distribui o evento.
    rs.ResolveBroadcast(broadcast);
}

// Privado:
void Thread::DetectPhaseTransitions(
    const int cycle
){
    // Procura em todos os grupos de RS instruções que finalizaram uma fase:
    // - EX_inicio   -> EX_concluido  (próximo é o MEM ou o WR).
    // - MEM _inicio -> MEM_concluido (próximo éWR)
    for (RS* station : rs.CollectBusyStations()) {
        RS& r{*station};

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
            // O ciclo MEM do STORE é representado apenas quando ele não possui ROB.
            // - Pula direto pro WR.
            if (store_with_rob) {
                entry.state = IN_FLIGHT_STATE::WAITING_WR;
                pending_wr_buffer.push_back(position);
            }
            RecordTraceEvent(position, TRACE_EVENT::EX_BOUNDARY, cycle);
        }
        // Caso 2: * finalizado: falta WR.
        // - Não precisa verificar o phase_before por que mudou de fase para o final.
        else if (phase_after == INSTRUCTION_PHASE_TOMASULO::WR) {
            const TRACE_EVENT completed_event{
                has_mem && !store_with_rob ? TRACE_EVENT::MEM_BOUNDARY :
                    TRACE_EVENT::EX_BOUNDARY
            };

            // Se o Branch foi resolvido, a flag é desmarcada.
            if (position == unresolved_branch_position &&
                r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR)
                unresolved_branch_position = -1; // Valor default.

            // Etapa intermediária libera hardware sem produzir resultado arquitetural.
            if (!final_stage) {
                rs.ReleaseByPosition(position, cycle);
                entry.current_stage++;
                entry.state = IN_FLIGHT_STATE::WAITING_ISSUE;
                entry.next_issue_cycle = cycle + 1;
                RecordTraceEvent(position, completed_event, cycle);
                continue;
            }

            // Branch final pode receber Commit no ciclo em que seu EX é resolvido.
            if (has_rob && type == INSTRUCTION_TYPE::BRANCH)
                MarkROBReady(position, cycle);

            // Coloca a instrução na fila de WR.
            entry.state = IN_FLIGHT_STATE::WAITING_WR;
            pending_wr_buffer.push_back(position);
            RecordTraceEvent(position, completed_event, cycle);
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
    while (!rob.empty() && writes < fu.GetCommitWidth()){
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

        num_committed_instructions++;
        writes++;
        rob.pop_front();
        RecordTraceEvent(position, TRACE_EVENT::COMMIT, cycle);

        // Se não tem previsor, as instruções após um Branch tem que ser em outro ciclo.
        if (type == INSTRUCTION_TYPE::BRANCH && !(has_predictor && has_rob)) break;
    }
}

} // namespace processor
