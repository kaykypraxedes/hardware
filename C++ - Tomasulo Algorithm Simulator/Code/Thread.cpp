/* Thread.cpp */
#include "headers/Thread.h"
#include "headers/InstructionFactory.h"

namespace processor {

// ─── ELEMENTOS STATIC ─────────────────────────────────────────────
static const int NUM_RS_GROUPS = 6;
static const int NUM_FU_GROUPS = 6;

// Retorna a referência para o vetor do grupo de RS correto da instrução.
static std::vector<ReservationStation>& GetRSGroupForType(
    RESERVATION_STATIONS&  rs,
    const INSTRUCTION_TYPE type
) {
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

// Função auxiliar para indicar ao stable_sort o padrão de organização desejado (nesse caso, crescente).
static bool ComparePositionOnRS(
    const ReservationStation* a,
    const ReservationStation* b
) {
    return a->GetCurrentInstruction().GetPosition() < b->GetCurrentInstruction().GetPosition();
}

// Libera a célula da RS que terminou o WR (identificada pela posição da instrução).
static void ReleaseRS(
    std::vector<ReservationStation>& group,
    const int                        position,
    const int                        cycle
) {
    for (ReservationStation& r : group) {
        if (r.IsBusy() &&
            r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR &&
            r.GetCurrentInstruction().GetPosition() == position) {

            r.Release(cycle);
            break; // Já encontrou a célula correta (não precisa continuar iterando).
        }
    }
}

// ReleaseRS() focado no caso de Store com ROB: a célula da RS é liberada ainda na fase MEM.
// - Ignora a checagem da fase WR para evitar que o processador trave (já que Store não tem a fase WR marcada).
static void ReleaseRSStoreWithROB(
    std::vector<ReservationStation>& group,
    const int                        position,
    const int                        cycle
) {
    for (ReservationStation& r : group) {
        if (r.IsBusy() &&
            r.GetCurrentInstruction().GetPosition() == position) {

            r.Release(cycle);
            break; // Já encontrou a célula correta (não precisa continuar iterando).
        }
    }
}

// Passa em um grupo de RS resolvendo dependências onde Qj/Qk = dest.
static void ResolveDependencyInGroup(
    std::vector<ReservationStation>& group,
    const std::string&               rs_id,
    const Register&                  dest
){
    for (ReservationStation& dep : group)
        if (dep.IsBusy()) dep.ResolveDependency(rs_id, dest);
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
int Thread::GetCurrentInstructionPosition()      const { return current_instruction_position; }

// Público:
const CDB& Thread::GetCDB()                      const { return cdb; }

// Público:
const RESERVATION_STATIONS& Thread::GetRS()      const { return rs; }

// Público:
const FUNCTIONAL_UNITS& Thread::GetFU()          const { return fu; }

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

    // Passa as instruções para a tabela.
    // - O parse é feito pelo InstructionFactory (multiarchitecture.md):
    //   por enquanto a arquitetura é fixa em MIPS32; a chave de config
    //   (configuração -> arquitetura) é assunto da Fase 2 (Bloco C).
    // - A Factory devolve unique_ptr; a Thread converte para shared_ptr ao
    //   guardar na tabela (D3) para que RS e ROB compartilhem o MESMO objeto.
    std::vector<std::unique_ptr<Instruction>> parsed =
        InstructionFactory::ParseTrace(assembly, Architecture::MIPS_32);
    for (std::unique_ptr<Instruction>& inst : parsed) {
        // Ignora propositalmente os outros valores para que eles recebam o default.
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        instruction_table.push_back({std::shared_ptr<Instruction>(std::move(inst))}); // Warning ignorado.
        #pragma GCC diagnostic pop
    }

    // Inicializa os RSs e as FUs.
    InitializeComponents(num_rs, num_fus, dispatch_width);

    // Passa as novas latências às instruções.
    for (const auto& [position, ex, mem] : new_latency) {
        if (static_cast<size_t>(position) < instruction_table.size()) {
            instruction_table[position].instruction->SetExLatency(ex);
            if (mem > 0) instruction_table[position].instruction->SetMemLatency(mem);
        }
    }
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
void Thread::InitializeComponents(
    const std::vector<int>& num_rs,
    const std::vector<int>& num_fus,
    const int               dispatch_width
){
    // Declara os registradores do banco:
    for(int i{}; i < num_registers; i++){
        cdb.R.push_back(Register("R" + std::to_string(i))); // R: R0, R1, ..., R31
        cdb.F.push_back(Register("F" + std::to_string(i))); // F: F0, F1, ..., F31
    }

    // Declara os componententes da RS:
    // Verifica se foram passados valores para RSs:
    std::vector<int> aux;
    aux = num_rs.empty() ? std::vector<int>{5,5,5,4,3,2} : num_rs; // Valores arbitrários de default.
    // Verifica se a quantidade de valores para RSs passados é válido:
    if(aux.size() != NUM_RS_GROUPS){
        std::cerr << "[ERRO] Quantidade inválida de RSs: " << num_rs.size() << "\n";
        std::abort();
    }
    int i{};
    std::vector<std::string> rs_names{"load", "store", "int_basic", "int_mult_div", "float_basic", "float_mult_div"};
    for (std::vector<ReservationStation>* group : GetAllRSGroups()) {
        for(int j{}; j < aux[i]; j++) group->push_back(ReservationStation("RS_" + rs_names[i] + std::to_string(j)));
        i++;
    }

    // Declara os componententes do FU:
    // Verifica se foram passados valores para FUs:
    aux = num_fus.empty() ? std::vector<int>{1,1,1,1,1,2} : num_fus; // Valores arbitrários de default.
    // Verifica se a quantidade de valores para FUs passados é válido:
    if(aux.size() != NUM_FU_GROUPS){
        std::cerr << "[ERRO] Quantidade inválida de FUs: " << num_fus.size() << "\n";
        std::abort();
    }
    i = 0;
    for (std::vector<FU>* group : GetAllFUGroups()) {
        for(int j{}; j < aux[i]; j++) group->push_back(FU{});
        i++;
    }
    // Valores int.
    fu.wr     = aux[5];
    if(has_rob) fu.commit = dispatch_width; // Só inicializa commit se tem ROB
}

// Privado:
// Retorna um vetor com os ponteiros para todos os grupos de RS.
// - Utilizado também em StartExOrMemPhase(), WriteResult(), BroadcastCDB() e DetectPhaseTransitions().
std::vector<std::vector<ReservationStation>*> Thread::GetAllRSGroups() {
    return {
        &rs.load,
        &rs.store,
        &rs.int_basic,
        &rs.int_mult_div,
        &rs.float_basic,
        &rs.float_mult_div
    };
}

// Privado:
// Retorna um vetor com os ponteiros para todos os grupos da FU (menos wr e commit que são int).
std::vector<std::vector<FU>*> Thread::GetAllFUGroups() {
    return {
        &fu.memory_access,
        &fu.int_basic_alu,
        &fu.int_mult_div_alu,
        &fu.float_basic_alu,
        &fu.float_mult_div_alu
    };
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
// Tenta adicionar Issues no RS.
bool Thread::Issue(
    const int cycle
){
    // Verifica se faltam instruções a ser adicionadas.
    if (current_instruction_position >= static_cast<int>(instruction_table.size())) return false;
    // Verifica se tem espaço no ROB para adicionar mais instruções.
    if (rob.size() >= static_cast<size_t>(rob_capacity)) return false;

    // Verifica se a instrução é válida (shared_ptr compartilhado com RS/ROB).
    const std::shared_ptr<Instruction>& instruction = instruction_table[current_instruction_position].instruction;
    INSTRUCTION_TYPE type    = instruction->GetInstructionType();
    if (type == INSTRUCTION_TYPE::INVALID) {
        std::cerr << "[ERRO] Tentativa de adicionar instrução inválida no issue! \n"
                  << "- Instrução: " << instruction->GetInstructionString() << "\n"
                  << "- Posição: "   << current_instruction_position << "\n";
        std::abort();
    }

    // Identifica o tipo de grupo de RS necessário para alocar e procura uma vaga.
    std::vector<ReservationStation>& group = GetRSGroupForType(rs, type);

    for (ReservationStation& r : group) {
        // Se conseguiu adicionar:
        if (r.AddIssue(instruction, cdb, cycle)) {

            // 1. Marca na tabela o IS da instrução.
            instruction_table[current_instruction_position].issue_cycle = cycle;
            // 2.1. Se tem um ROB:
            // - Adiciona a instrução no ROB (MESMO shared_ptr da tabela: sem cópia).
            if (has_rob) rob.push_back(instruction_table[current_instruction_position].instruction);
            // 2.2. Se não tem ROB (obrigatóriamente sem previsor de desvio) e for um branch:
            // - Marca para as instruções posteriores serem atrasadas.
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
// Verifica se as instruções já foram devidamente já estão aptas a começar a fase EX/MEM.
bool Thread::ExMem(
    const int cycle
){
    // Com ROB: Verifica se todas as instruções já foram commitadas.
    if (static_cast<size_t>(num_committed_instructions) == instruction_table.size()) return true;
    // Sem ROB: Verifica se todas as instruções já passaram pelo WR.
    if (!has_rob && static_cast<size_t>(num_finished_instructions) == instruction_table.size()) return true;

    // Se ainda faltar instruções a serem executadas, ele as executa no ciclo.
    if(static_cast<size_t>(num_finished_instructions) != instruction_table.size())
        StartExOrMemPhase(cycle);
    return false;
}

// Privado:
// Muda a instrução de fase se não tem mais dependências (Qj/Qk = null).
void Thread::StartExOrMemPhase(
    const int cycle
){
    // Procura em todos os grupos de RS com instruções aptas a mudar de fase:
    // IS  -> EX
    // EX  -> MEM
    std::vector<ReservationStation*> candidates;
    for (std::vector<ReservationStation>* group : GetAllRSGroups()) {
        for (ReservationStation& r : *group) {
            // Célula da RS vazia.
            if (!r.IsBusy()) continue;
            // Filtro que garante selecionar apenas as instruções em IS ou MEM (que vai começar), já que são as únicas que podem ter o countdown = -1.
            // - Redundante em lógica, já que o ReservationStations::UpdateDependencies() já faz esse filtro, mas diminui o sort.
            if (r.GetCountdown() != -1 || r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR) continue;

            // Instrução observada foi adicionada após um branch não resolvido:
            // - Tem sua execução atrasada (branch stall).
            int inst_position = r.GetCurrentInstruction().GetPosition();
            if (unresolved_branch_position >= 0 && inst_position > unresolved_branch_position) continue;

            // Previne que STORE com ROB seja escalonado para WR (nesse caso é apenas commit).
            INSTRUCTION_TYPE type = r.GetCurrentInstruction().GetInstructionType();
            if (type == INSTRUCTION_TYPE::STORE && has_rob && r.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM) continue;

            candidates.push_back(&r);
        }
    }

    // Ordena os candidatos com base na sua ordenação original.
    std::stable_sort(candidates.begin(), candidates.end(), ComparePositionOnRS);

    // Avança as instruções de fase:
    for (ReservationStation* r : candidates) {
        // Verifica se conseguiu mudar a instrução de fase:
        if (r->UpdateDependencies(cdb, fu, cycle)) {

            // Marca na tabela.
            int position = r->GetCurrentInstruction().GetPosition();
            if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX)
                instruction_table[position].ex_cycles.push_back(cycle);
            else if (r->GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM)
                instruction_table[position].mem_cycles.push_back(cycle);
        }
    }
}

// ─── WR ───────────────────────────────────────────────────────────
// Público:
// Verifica se as instruções já foram devidamente processadas.
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
// Efetiva a marcação do WR na tabela.
void Thread::PerformWriteResult(
    const int cycle
){
    // Marca na tabela as instruções que chegaram ao WR.
    int writes{};
    while (!wr_buffer.empty() && writes < fu.wr) { // Limitado pela capacidade de WR simultâneo.
        int position = wr_buffer.front();
        INSTRUCTION_TYPE instr_type{instruction_table[position].instruction->GetInstructionType()};
        bool store_with_rob = (instr_type == INSTRUCTION_TYPE::STORE && has_rob);

        // O estágio WR do STORE só é marcado se o o processador não possui ROB.
        // - Pula essa marcação na tabela.
        if (store_with_rob) {
            ReleaseRSStoreWithROB(rs.store, position, cycle);
            wr_buffer.erase(wr_buffer.begin());
            num_finished_instructions++;
            continue; // Não conta como um WR (não há resultado sendo escrito de fato).
        }

        // Escreve o resultado, e propaga a informação nos componentes.
        WriteResultOnComponents(position, cycle);
        wr_buffer.erase(wr_buffer.begin());
        num_finished_instructions++;

        // Branch não ocupa porta de WR (não escreve em registrador nenhum).
        if (instr_type != INSTRUCTION_TYPE::BRANCH) writes++;
    }
}

// Privado:
// Realiza as atualizações no CDB e no RS.
void Thread::WriteResultOnComponents(
    const int position,
    const int cycle
){
    INSTRUCTION_TYPE type = instruction_table[position].instruction->GetInstructionType();

    // STORE e BRANCH não escrevem em nenhum registrador.
    if (type != INSTRUCTION_TYPE::STORE && type != INSTRUCTION_TYPE::BRANCH)
        instruction_table[position].wr_cycle = cycle;

    // 1. Propaga o resultado no CDB e libera os registradores.
    // 2. Resolve as dependências nos RSs que estavam esperando.
    // - Instrução pode ter mais de um destino (ex.: x86 reg + EFLAGS);
    //   o broadcast é feito para cada destino, um por um.
    const std::vector<Register>& dests = instruction_table[position].instruction->GetDestRegisters();
    for (const Register& dest : dests)
        BroadcastOnRSAndCDB(dest, position, cycle);

    // Libera a célula da RS produtora.
    ReleaseRS(GetRSGroupForType(rs, type), position, cycle);
}

// Privado:
// Transmite no CDB na RS as instruções que chegaram ao ciclo de WR.
// - Elimina as dependências em Qj/Qk para que a instrução possa começar o EX ou o MEM.
void Thread::BroadcastOnRSAndCDB(
    const Register& dest,
    const int       position,
    const int       cycle
){
    // Instrução sem registrador de destino válido.
    if (dest.GetType() == 'Z') return;

    // Nos componentes:
    // 1. Desaloca no CDB registradores travados das instruções que finalizaram o WR.
    // 2. Atualiza em todos os grupos de RS as dependências das instruções que dependiam desse resultado.
    std::vector<Register>& regs = (dest.GetType() == 'F') ? cdb.F : cdb.R;
    for (std::vector<ReservationStation>* group : GetAllRSGroups()) {
        for (ReservationStation& r : *group) {

            // Ignora células da RS vazias ou que ainda não chegaram em WB.
            if (!r.IsBusy() || r.GetInstructionPhase() != INSTRUCTION_PHASE_TOMASULO::WR) continue;
            // Ignora células da RS de outras instruções (posição diferente).
            if (r.GetCurrentInstruction().GetPosition() != position) continue;

            // Tenta desalocar a célula da RS do registrador de destino no CDB.
            std::string rs_id       = r.GetId();
            int         start_cycle = regs[dest.GetId()].GetRSCycleStart(rs_id);
            if (!regs[dest.GetId()].DeallocateRS(rs_id, start_cycle, cycle)) {
                std::cerr << "[ERRO] Falha na desalocação da célula da RS!"
                "- RS: " << rs_id << '\n' <<
                "- [start-end]: [" << start_cycle << "-" << cycle << "]\n";
                std::abort();
            }

            // Propaga o valor para todas as RSs que esperavam por essa produção (Qj/Qk == rs_id).
            for (std::vector<ReservationStation>* group : GetAllRSGroups())
                ResolveDependencyInGroup(*group, rs_id, dest);
        }
    }
}

// Privado:
// Encontra instruções que mudaram de fase após a passagem do ciclo.
void Thread::DetectPhaseTransitions(
    const int cycle
){
    // Procura em todos os grupos de RS instruções que finalizaram uma operação:
    // EX  -> MEM
    // EX  -> WR
    // MEM -> WR
    for (std::vector<ReservationStation>* group : GetAllRSGroups()) {
        for (ReservationStation& r : *group) {
            // Célula da RS vazia.
            if (!r.IsBusy()) continue;

            // Verifica se a fase mudou com o incremento do contador:
            // - Guarda a fase antes da tentativa, para comparar com a fase depois.
            INSTRUCTION_PHASE_TOMASULO phase_before = r.GetInstructionPhase();

            // Ainda executando.
            if (!r.UpdateCountdown(fu, cycle)) continue;

            INSTRUCTION_PHASE_TOMASULO phase_after = r.GetInstructionPhase();
            int                        position    = r.GetCurrentInstruction().GetPosition();
            INSTRUCTION_TYPE           type        = r.GetCurrentInstruction().GetInstructionType();
            bool has_mem        = (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE);
            bool store_with_rob = (type == INSTRUCTION_TYPE::STORE && has_rob);

            // Caso 1: EX -> MEM
            if (phase_before == INSTRUCTION_PHASE_TOMASULO::EX && phase_after == INSTRUCTION_PHASE_TOMASULO::MEM) {
                // Marca na tabela.
                instruction_table[position].ex_cycles.push_back(cycle);
                // O ciclo MEM do STORE é representado apenas quando ele não possui ROB.
                // - Pula direto pro WR.
                if (store_with_rob) pending_wr_buffer.push_back(position);
            }
            // Caso 2: * -> WR
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

                // Coloca a instrução na fila de WR
                pending_wr_buffer.push_back(position);
            }
        }
    }
}

// ─── COMMIT ───────────────────────────────────────────────────────
// Público:
// Verifica instruções aptas ao commit.
void Thread::Commit(
    const int cycle
){
    // Apenas realiza commit se possui ROB.
    if (!has_rob) return;

    int writes{};
    // Até acabar as instruções ou até o limite de despacho.
    while (!rob.empty() && writes < fu.commit){
        TABLE_ROW& row{instruction_table[commit_pointer]};
        INSTRUCTION_TYPE type = row.instruction->GetInstructionType();
        bool is_store = (type == INSTRUCTION_TYPE::STORE);
        bool pronto = false;

        // Verifica se é um Store (caso especial, pois não tem WR).
        if (is_store) {
            // Verifica se a instrução já concluiu o MEM dela.
            if (row.mem_cycles.empty()) {
                row.mem_cycles.push_back(cycle); // Marca temporariamente o ciclo de início do MEM.
            }
            // Verifica se a instrução já acabou o MEM.
            if (row.mem_cycles.size() == 1) {
                int mem_end = row.mem_cycles.back() + row.instruction->GetMemLatency() - 1;
                if (cycle >= mem_end) {
                    row.mem_cycles.pop_back(); // Tira a marcação temporária da tabela.
                    pronto = true;
                }
            }
        } // Verifica se é um Branch (sem WR também, dependendo do EX completo).
        else if (type == INSTRUCTION_TYPE::BRANCH) {
            pronto = (row.ex_cycles.size() == 2);
        } // Demais instruções.
        else {
            pronto = (row.wr_cycle > 0 && row.wr_cycle < cycle);
        }

        // Marca na tabela:
        if (pronto) {
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
