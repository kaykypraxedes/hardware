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
    Register& reg{GetReg(cdb, dest)};
    if (!reg.DeallocateProducer(position, cycle)) {
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

static void ReleaseRS(
    std::vector<RS>& group,
    const int        position,
    const int        cycle
) {
    // Libera o módulo da RS que terminou o WR (identificada pela posição da instrução).
    for (RS& r : group) {
        if (r.IsBusy() &&
            r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR &&
            r.GetCurrentInstruction().GetPosition() == position) {

            r.Release(cycle);
            break; // Já encontrou a célula correta (não precisa continuar iterando).
        }
    }
}

static void ReleaseRSStoreWithROB( // ReleaseRS() para Store com ROB (a célula da RS é liberada ainda na fase MEM).
    std::vector<RS>& group,
    const int        position,
    const int        cycle
) {
    for (RS& r : group) {
        // Ignora a checagem da fase WR para evitar que o processador trave (já que Store não tem a fase WR marcada).
        if (r.IsBusy() &&
            r.GetCurrentInstruction().GetPosition() == position) {

            r.Release(cycle);
            break; // Já encontrou a célula correta (não precisa continuar iterando).
        }
    }
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

    // Passa as instruções para a tabela:
    std::vector<std::unique_ptr<Instruction>> parsed{InstructionFactory::ParseTrace(assembly, arch)};
    for (std::unique_ptr<Instruction>& inst : parsed) {
        // Ignora propositalmente os outros valores de instruction_table para que eles recebam o default.
        // - Gera warning (por passar menos elementos do que deve na struct) ignorado pela diretiva.
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        // Converte de unique_ptr para shared_ptr ao guardar na tabela para que RS e ROB compartilhem o MESMO objeto.
        instruction_table.push_back({std::shared_ptr<Instruction>(std::move(inst))});
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
bool Thread::Issue(
    const int cycle
){
    // Todas as instruções já foram adicionadas.
    if (current_instruction_position >= static_cast<int>(instruction_table.size())) return false;

    // ROB já está cheio (apenas em Tomasulo especulativo).
    if (rob.size() >= static_cast<size_t>(rob_capacity)) return false;

    const std::shared_ptr<Instruction>& instruction{instruction_table[current_instruction_position].instruction};
    INSTRUCTION_TYPE type{instruction->GetInstructionType()};

    // Verifica se a instrução é válida:
    if (type == INSTRUCTION_TYPE::INVALID) {
        std::cerr <<
            "[ERRO] Tentativa de adicionar instrução inválida no issue! \n" <<
            "- Instrução: " << instruction->GetInstructionString() << '\n'  <<
            "- Posição: "   << current_instruction_position << '\n';
        std::abort();
    }

    // Identifica o tipo de grupo de RS necessário para alocar e procura uma vaga.
    std::vector<RS>& group{GetRSGroupForType(rs, type)};

    for (RS& r : group) {
        // Se conseguiu adicionar:
        if (r.AddIssue(instruction, cdb, cycle)) {

            // 1. Marca na tabela o IS da instrução.
            instruction_table[current_instruction_position].issue_cycle = cycle;

            // 2.1. Se tem um ROB:
            if (has_rob){
                // - Adiciona a instrução no ROB (MESMO shared_ptr da tabela - sem cópia).
                rob.push_back(instruction_table[current_instruction_position].instruction);

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
                unresolved_branch_position = current_instruction_position;

            // 3. Passa para a próxima instrução.
            current_instruction_position++;

            return true;
        }
    }
    return false;
}

// ─── EX/MEM ───────────────────────────────────────────────────────
// Público:
bool Thread::ExMem(
    const int cycle
){
    // Com ROB: Verifica se todas as instruções já foram commitadas.
    if (static_cast<size_t>(num_committed_instructions) == instruction_table.size()) return true;

    // Sem ROB: Verifica se todas as instruções já passaram pelo WR.
    if (!has_rob && static_cast<size_t>(num_finished_instructions) == instruction_table.size()) return true;

    // Se ainda faltar instruções a serem executadas, ele as executa no ciclo.
    if(static_cast<size_t>(num_finished_instructions) != instruction_table.size())
        StartPhase(cycle);
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
            INSTRUCTION_TYPE type{r.GetCurrentInstruction().GetInstructionType()};
            if (type == INSTRUCTION_TYPE::STORE && has_rob && r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) continue;

            candidates.push_back(&r);
        }
    }

    // Ordena os candidatos com base na sua ordenação original.
    std::stable_sort(candidates.begin(), candidates.end(), ComparePositionOnRS);

    // Avança as instruções de fase:
    for (RS* r : candidates) {
        // Verifica se conseguiu mudar a instrução de fase:
        if (r->UpdateDependencies(cdb, fu, cycle)) {

            // Marca na tabela:
            int position{r->GetCurrentInstruction().GetPosition()};

            if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX)
                instruction_table[position].ex_cycles.push_back(cycle);

            else if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM)
                instruction_table[position].mem_cycles.push_back(cycle);
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
        INSTRUCTION_TYPE instr_type{instruction_table[position].instruction->GetInstructionType()};
        bool store_with_rob{instr_type == INSTRUCTION_TYPE::STORE && has_rob};

        // O estágio "wr" do "store" só é marcado se o o processador não possui ROB.
        if (store_with_rob) {
            ReleaseRSStoreWithROB(rs.store, position, cycle);
            wr_buffer.erase(wr_buffer.begin());
            num_finished_instructions++;
            continue; // Não conta como um WR (não há resultado sendo escrito de fato).
        }

        // Faz a marcação na tabela:
        // - "stores" e "branches" não escrevem em nenhum resultado em registradores ("wr" nulo).
        if (instr_type != INSTRUCTION_TYPE::STORE && instr_type != INSTRUCTION_TYPE::BRANCH)
            instruction_table[position].wr_cycle = cycle;

        // Propaga a informação nos componentes.
        WriteResultOnComponents(position, cycle, instr_type);
        wr_buffer.erase(wr_buffer.begin());
        num_finished_instructions++;

        // Branch não ocupa porta de WR (não escreve em registrador nenhum).
        if (instr_type != INSTRUCTION_TYPE::BRANCH) writes++;
    }
}

// Privado:
void Thread::WriteResultOnComponents(
    const int              position,
    const int              cycle,
    const INSTRUCTION_TYPE instr_type
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
    ReleaseRS(GetRSGroupForType(rs, instr_type), position, cycle);
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
            INSTRUCTION_TYPE type{r.GetCurrentInstruction().GetInstructionType()};
            bool has_mem{type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE};
            bool store_with_rob{type == INSTRUCTION_TYPE::STORE && has_rob};

            // Caso 1: EX finalizado: falta MEM.
            if (phase_before == INSTRUCTION_PHASE_TOMASULO::EX && phase_after == INSTRUCTION_PHASE_TOMASULO::MEM) {
                // Marca na tabela.
                instruction_table[position].ex_cycles.push_back(cycle);
                // O ciclo MEM do STORE é representado apenas quando ele não possui ROB.
                // - Pula direto pro WR.
                if (store_with_rob) pending_wr_buffer.push_back(position);
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

                // Coloca a instrução na fila de WR.
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
        TABLE_ROW& row{instruction_table[commit_pointer]};
        INSTRUCTION_TYPE type{row.instruction->GetInstructionType()};
        bool is_store{type == INSTRUCTION_TYPE::STORE};
        bool ready{false};

        // Verifica se é um Store (caso especial, pois não tem WR).
        if (is_store) {
            // Verifica se a instrução já concluiu o MEM dela.
            if (row.mem_cycles.empty()) {
                row.mem_cycles.push_back(cycle); // Marca temporariamente o ciclo de início do MEM.
            }
            // Verifica se a instrução já acabou o MEM.
            if (row.mem_cycles.size() == 1) {
                int mem_end{row.mem_cycles.back() + row.instruction->GetMemLatency() - 1};
                if (cycle >= mem_end) {
                    row.mem_cycles.pop_back(); // Tira a marcação temporária da tabela.
                    ready = true;
                }
            }
        }
        // Verifica se é um Branch (sem WR também, dependendo do EX completo).
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            ready = (row.ex_cycles.size() == 2);
        }
        // Demais instruções.
        else {
            ready = (row.wr_cycle > 0 && row.wr_cycle < cycle);
        }

        // Marca na tabela:
        if (ready) {
            row.commit_cycle = cycle;
            num_committed_instructions++;
            writes++;
            commit_pointer++;
            rob.erase(rob.begin());

            // Se não tem previsor, as instruções após um Branch tem que ser em outro ciclo.
            if (type == INSTRUCTION_TYPE::BRANCH && !(has_predictor && has_rob)) break;
        }
        else break;
    }
}

} // namespace processor
