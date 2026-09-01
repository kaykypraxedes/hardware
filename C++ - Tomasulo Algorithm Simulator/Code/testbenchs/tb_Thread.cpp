/* tb_Thread.cpp */
// Testbench isolado de Thread.cpp
#include "../headers/Thread.h"
#include "../headers/Components.h"
#include "tb_Helpers.h"
#include "tb_SyntheticInstruction.h"
#include <memory> // para std::unique_ptr e std::make_unique
#include <vector>

using namespace processor;

static const std::vector<int> DEFAULT_NUM_RS    = {5,5,5,4,3,2};
static const std::vector<int> DEFAULT_NUM_FUS   = {1,1,1,1,1,2};
static const int              DEFAULT_DISPATCH_WIDTH = 1;

// Cria uma instrução sintética já decodificada na posição solicitada.
static std::unique_ptr<Instruction> MakeSyntheticInstruction(
    const int          position,
    const std::string& plan
) {
    auto instruction{std::make_unique<SyntheticInstruction>(position)};
    instruction->Parse(plan);
    return instruction;
}

// Cria uma instrução da arquitetura simplificada com posição explícita.
static std::unique_ptr<Instruction> MakeSimplifiedInstruction(
    const int          position,
    const std::string& text
) {
    auto instruction{std::make_unique<InstructionSimplified>(position)};
    instruction->Parse(text);
    return instruction;
}

// Confirma que nenhuma linha mantém ciclos observacionais registrados.
static bool IsTraceEmpty(
    const std::vector<TABLE_ROW>& trace
) {
    for (const TABLE_ROW& row : trace) {
        if (!row.issue_cycles.empty() ||
            !row.ex_cycles.empty() ||
            !row.mem_cycles.empty() ||
            row.wr_cycle != -1 ||
            row.commit_cycle != -1) return false;
    }
    return true;
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E CONFIGURAÇÃO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E CONFIGURAÇÃO");

    section("1.1 Thread() — construtor sem ROB");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "sub r4, r1, r5"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        check("GetCurrentInstructionPosition() == 0",      t.GetCurrentInstructionPosition() == 0);
        check("GetTable().size() == 2",                    t.GetTable().size() == 2);
        check("tabela[0].instruction->GetPosition() == 0",  t.GetTable()[0].instruction->GetPosition() == 0);
        check("tabela[1].instruction->GetPosition() == 1",  t.GetTable()[1].instruction->GetPosition() == 1);
        check("tabela[0].issue_cycles vazio (não emitido)", t.GetTable()[0].issue_cycles.empty());

        check("GetRegisterStatus().Size() == 66 (R0-31 + F32-63 + M6-65)",
            t.GetRegisterStatus().Size() == 66);
        check("GetRegisterBanks() preserva as 3 faixas simplificadas",
            t.GetRegisterBanks().size() == 3);

        check("GetRS().GetLoadStations().size() == 5",
            t.GetRS().GetLoadStations().size() == 5);
        check("GetRS().GetStoreStations().size() == 5",
            t.GetRS().GetStoreStations().size() == 5);
        check("GetRS().GetIntBasicStations().size() == 5",
            t.GetRS().GetIntBasicStations().size() == 5);
        check("GetRS().GetIntMultDivStations().size() == 4",
            t.GetRS().GetIntMultDivStations().size() == 4);
        check("GetRS().GetFloatBasicStations().size() == 3",
            t.GetRS().GetFloatBasicStations().size() == 3);
        check("GetRS().GetFloatMultDivStations().size() == 2",
            t.GetRS().GetFloatMultDivStations().size() == 2);

        check("GetFU().GetMemoryAccessUnits().size() == 1",
            t.GetFU().GetMemoryAccessUnits().size() == 1);
        check("GetFU().GetIntBasicUnits().size() == 1",
            t.GetFU().GetIntBasicUnits().size() == 1);
        check("GetFU().GetWriteResultWidth() == 2",
            t.GetFU().GetWriteResultWidth() == 2);
        check("GetFU().GetCommitWidth() == 0 sem ROB",
            t.GetFU().GetCommitWidth() == 0);
    }

    section("1.2 Thread() — construtor COM ROB");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, /*dispatch_width=*/3, /*rob_capacity=*/8);

        // A largura de Commit permite observar a configuração associada ao ROB.
        check("GetFU().GetCommitWidth() == dispatch_width (3) quando há ROB",
            t.GetFU().GetCommitWidth() == 3);
    }

    section("1.3 IsSwitchCycle() — granulação grossa");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "sub r4, r1, r5"};
        std::vector<int> switch_cycles = {1};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, switch_cycles);

        t.Issue(1);
        check("posição 1 é ciclo de troca", t.IsSwitchCycle());
        check("IsSwitchCycle() consome a marcação (não repete)", !t.IsSwitchCycle());
    }

    section("1.4 latency_overrides no construtor");
    {
        std::vector<std::string> prog = {"l.d f2, 0(r1)"};

        Thread t1(prog);  // sem latency_overrides
        check("exLat base == 1",  t1.GetTable()[0].instruction->GetExLatency()  == 1);
        check("memLat base == 1", t1.GetTable()[0].instruction->GetMemLatency() == 1);

        Thread t2(prog, {{0, {3}, {2}}});  // ex=3, mem=2
        check("exLat == 3",  t2.GetTable()[0].instruction->GetExLatency()  == 3);
        check("memLat == 2", t2.GetTable()[0].instruction->GetMemLatency() == 2);

        Thread t3(prog, {{0, {5}, {0}}});  // ex=5, mem=0 → preserva a base
        check("exLat == 5", t3.GetTable()[0].instruction->GetExLatency()  == 5);
        check("memLat continua 1 (base)", t3.GetTable()[0].instruction->GetMemLatency() == 1);
    }

    section("1.5 Thread() — instruções pré-decodificadas");
    {
        std::vector<std::unique_ptr<Instruction>> instructions;
        instructions.push_back(MakeSyntheticInstruction(0, "multi"));

        Thread t(std::move(instructions), {{0, {1, 1, 1}, {1, 0, 1}}});
        check("sobrecarga preserva plano multi-etapa",
            t.GetTable()[0].instruction->GetInstructionTypes() ==
                std::vector<INSTRUCTION_TYPE>({
                    INSTRUCTION_TYPE::LOAD,
                    INSTRUCTION_TYPE::INT_MUL,
                    INSTRUCTION_TYPE::STORE
                }));

        check("sobrecarga rejeita posição descontínua", Aborts([]() {
            std::vector<std::unique_ptr<Instruction>> invalid;
            invalid.push_back(MakeSyntheticInstruction(1, "multi"));
            Thread thread(std::move(invalid));
        }));
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] Construtor: previsor sem ROB deve abortar");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        // has_predictor=true, rob_capacity=0 → combinação inválida, espera abort().
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH,
                 0    // rob_capacity,
                 true // has_predictor);
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Construtor: quantidade inválida de valores de RS deve abortar");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        std::vector<int> rs_errado = {1, 2, 3}; // precisa ter 6 elementos
        Thread t(prog, {}, rs_errado, DEFAULT_NUM_FUS);
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Construtor: quantidade inválida de valores de FU deve abortar");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        std::vector<int> fu_errado = {1, 1}; // precisa ter 6 elementos
        Thread t(prog, {}, DEFAULT_NUM_RS, fu_errado);
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. Issue() ISOLADO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. Issue() ISOLADO");

    section("2.1 Issue() — emissão simples sem dependência");
    {
        std::vector<std::string> prog = {"add r3, r1, r2", "sub r5, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        ISSUE_RESULT result1{t.Issue(1)};
        check("ciclo 1: retorna NEW_INSTRUCTION",
            result1.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        check("ciclo 1: identifica posição e tipo emitidos",
            result1.position == 0 &&
            result1.instruction_type == INSTRUCTION_TYPE::INT_BASIC);
        check("ciclo 1: Posição avança para 1",           t.GetCurrentInstructionPosition() == 1);
        check("ciclo 1: tabela[0].issue_cycles.front() == 1",      t.GetTable()[0].issue_cycles.front() == 1);

        ISSUE_RESULT result2{t.Issue(2)};
        check("ciclo 2: retorna NEW_INSTRUCTION",
            result2.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        check("ciclo 2: identifica posição e tipo emitidos",
            result2.position == 1 &&
            result2.instruction_type == INSTRUCTION_TYPE::INT_BASIC);
        check("ciclo 2: Posição avança para 2",           t.GetCurrentInstructionPosition() == 2);
        check("ciclo 2: tabela[1].issue_cycles.front() == 2",      t.GetTable()[1].issue_cycles.front() == 2);

        ISSUE_RESULT result3{t.Issue(3)};
        check("ciclo 3: retorna BLOCKED (sem mais instruções)",
            result3.outcome == ISSUE_OUTCOME::BLOCKED);
        check("resultado bloqueado não identifica instrução",
            result3.position == -1 &&
            result3.instruction_type == INSTRUCTION_TYPE::INVALID);
    }

    section("2.2 Issue() — RS cheia bloqueia emissão");
    {
        std::vector<std::string> prog = {
            "l.d f0, 0(r0)", "l.d f1, 0(r1)", "l.d f2, 0(r2)",
            "l.d f3, 0(r3)", "l.d f4, 0(r4)", "l.d f5, 0(r5)"
        };
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);
        for (int c = 1; c <= 5; c++)       t.Issue(c);
        check("Posição == 5 após 5 LOADs", t.GetCurrentInstructionPosition() == 5);
        ISSUE_RESULT cheio{t.Issue(6)};
        check("6o LOAD retorna BLOCKED (RS cheia)",
            cheio.outcome == ISSUE_OUTCOME::BLOCKED);
        check("bloqueio por RS cheia não identifica instrução",
            cheio.position == -1 &&
            cheio.instruction_type == INSTRUCTION_TYPE::INVALID);
        check("Posição não avança",        t.GetCurrentInstructionPosition() == 5);
    }

    section("2.3 Issue() — ROB cheio bloqueia emissão (rob_capacity pequeno)");
    {
        std::vector<std::string> prog = {"add r1,r2,r3", "add r4,r5,r6", "add r7,r8,r9"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/2);

        check("1a issue nova", t.Issue(1).outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        check("2a issue nova", t.Issue(2).outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        check("3a issue bloqueada (ROB cheio, capacidade 2)",
            t.Issue(3).outcome == ISSUE_OUTCOME::BLOCKED);
        check("posicao nao avancou (continua em 2)", t.GetCurrentInstructionPosition() == 2);
    }

    section("2.4 Issue() — BRANCH sem ROB: emissão liberada, bloqueio ocorre no EX");
    {
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        ISSUE_RESULT branch_result{t.Issue(1)}; // bnez emitido, unresolved_branch_position = 0
        check("Branch retorna evento novo explícito",
            branch_result.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION &&
            branch_result.position == 0 &&
            branch_result.instruction_type == INSTRUCTION_TYPE::BRANCH);
        check("Posição avança para 1 após issue do bnez", t.GetCurrentInstructionPosition() == 1);

        // Issue() em si NÃO verifica unresolved_branch_position (só StartExOrMemPhase verifica),
        // então o add consegue ser emitido para a RS aqui — o bloqueio real acontece no EX,
        // testado na seção 5.
        ISSUE_RESULT add_result{t.Issue(2)};
        check("add consegue ser emitido (bloqueio é no EX, não no Issue)",
            add_result.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] Issue(): instrução inválida (NONEXISTENT) deve abortar");
    {
        std::vector<std::string> prog = {"nop_invalido r1, r2, r3"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);
        t.Issue(1);
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 3. PIPELINE DE UMA INSTRUÇÃO POR VEZ (TRAÇOS COMPLETOS)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. PIPELINE DE UMA INSTRUÇÃO POR VEZ (TRAÇOS COMPLETOS)");

    section("3.1 add r3,r1,r2 — issue->EX->WR em 3 ciclos");
    {
        std::vector<std::string> prog = {"add r3, r1, r2"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        check("ciclo 1: issue registrado", t.GetTable()[0].issue_cycles.front() == 1);

        t.ExMem(2);
        t.Wr(2);
        check("ciclo 2: ex_cycles tem 2 entradas [inicio,fim]",
              t.GetTable()[0].ex_cycles.size() == 2);
        check("ciclo 2: ex_cycles[0] == 2 (inicio)",
              t.GetTable()[0].ex_cycles[0] == 2);
        check("ciclo 2: ex_cycles[1] == 2 (fim, latência 1)",
              t.GetTable()[0].ex_cycles[1] == 2);

        t.ExMem(3);
        t.Wr(3);
        check("ciclo 3: wr_cycle == 3", t.GetTable()[0].wr_cycle == 3);
    }

    section("3.2 mul r3,r1,r2 (exLat=4) — multiciclo");
    {
        std::vector<std::string> prog = {"mul r3, r1, r2"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        for (int c = 2; c <= 6; c++) {
            t.ExMem(c);
            t.Wr(c);
        }

        auto tab = t.GetTable();
        check("mul: issue == 1",                 tab[0].issue_cycles.front() == 1);
        check("mul: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("mul: ex_cycles[1] == 5 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 5);
        check("mul: WR == 6",                    tab[0].wr_cycle == 6);
        check("mul: mem_cycles vazio",           tab[0].mem_cycles.empty());
    }

    section("3.3 l.d f2,0(r1) — issue->EX->MEM->WR");
    {
        std::vector<std::string> prog = {"l.d f2, 0(r1)"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        check("LOAD: issue == 1", t.GetTable()[0].issue_cycles.front() == 1);

        t.ExMem(2);
        check("LOAD: ex_cycles[0] == 2 (inicio)",
            t.GetTable()[0].ex_cycles.size() >= 1 && t.GetTable()[0].ex_cycles[0] == 2);
        t.Wr(2);
        check("LOAD: ex_cycles[1] == 2 (fim)",
            t.GetTable()[0].ex_cycles.size() == 2 && t.GetTable()[0].ex_cycles[1] == 2);

        t.ExMem(3);
        check("LOAD: mem_cycles[0] == 3 (inicio)",
            t.GetTable()[0].mem_cycles.size() >= 1 && t.GetTable()[0].mem_cycles[0] == 3);
        t.Wr(3);
        check("LOAD: mem_cycles[1] == 3 (fim)",
            t.GetTable()[0].mem_cycles.size() == 2 && t.GetTable()[0].mem_cycles[1] == 3);

        t.ExMem(4);
        t.Wr(4);
        check("LOAD: WR == 4", t.GetTable()[0].wr_cycle == 4);

        t.ExMem(5);
    }

    section("3.4 s.d f2,0(r1) SEM ROB — completa igual a um LOAD, mas nunca marca wr_cycle");
    {
        std::vector<std::string> prog = {"s.d f2, 0(r1)"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS); // sem ROB

        t.Issue(1);
        t.ExMem(2); t.Wr(2); // IS -> EX
        check("STORE: ex_cycles == [2,2]",
            t.GetTable()[0].ex_cycles.size() == 2 &&
            t.GetTable()[0].ex_cycles[0] == 2 && t.GetTable()[0].ex_cycles[1] == 2);

        t.ExMem(3); t.Wr(3); // EX -> MEM -> WR (mem lat=1 por padrão)
        check("STORE: mem_cycles == [3,3]",
            t.GetTable()[0].mem_cycles.size() == 2 &&
            t.GetTable()[0].mem_cycles[0] == 3 && t.GetTable()[0].mem_cycles[1] == 3);

        t.ExMem(4); t.Wr(4); // efetiva o "WR" (libera RS, mas STORE não tem destino)
        check("STORE: wr_cycle continua -1 (STORE nunca marca WR)",
            t.GetTable()[0].wr_cycle == -1);
        check("STORE: mesmo assim é contado como finalizado", t.ExMem(5));
    }

    section("3.5 s.d f2,0(r1) COM ROB — RS liberada logo após o EX; latência de MEM simulada no Commit()");
    {
        std::vector<std::string> prog = {"s.d f2, 0(r1)"};
        // mem_latency = 3 (via latency_overrides) para deixar visível a simulação multi-ciclo no Commit().
        Thread t(prog, {{0, {1}, {3}}}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/4);

        t.Issue(1);
        check("STORE cria uma entrada do ROB aguardando execução",
            t.GetROBEntries().size() == 1 &&
            t.GetROBEntries()[0].position == 0 &&
            !t.GetROBEntries()[0].ready &&
            t.GetROBEntries()[0].ready_cycle == -1 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::WAITING_EXECUTION);
        t.ExMem(2); t.Wr(2); // IS -> EX (ex_cycles == [2,2], lat=1)
        // Com ROB, ao terminar o EX a RS já é agendada para liberação — não espera o MEM de verdade.
        t.ExMem(3); t.Wr(3);
        check("STORE c/ ROB: RS já liberada (não fica presa em MEM)",
            !t.GetRS().GetStoreStations()[0].IsBusy());
        check("STORE concluído aguarda temporização na cabeça do ROB",
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::WAITING_MEMORY &&
            !t.GetROBEntries()[0].ready &&
            t.GetROBEntries()[0].ready_cycle == -1);
        check("STORE c/ ROB: mem_cycles fica vazio (latência simulada só no Commit)",
            t.GetTable()[0].mem_cycles.empty());
        check("STORE c/ ROB: wr_cycle nunca é marcado", t.GetTable()[0].wr_cycle == -1);

        // Commit() simula a latência de MEM (3 ciclos) a partir da PRIMEIRA vez que chega no topo do ROB.
        t.Commit(10);
        check("commit 1/3: STORE iniciou memória com ready_cycle explícito",
            t.GetTable()[0].commit_cycle == -1 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::EXECUTING_MEMORY &&
            t.GetROBEntries()[0].ready_cycle == 12 &&
            !t.GetROBEntries()[0].ready);
        t.Commit(11);
        check("commit 2/3: ainda não commitou", t.GetTable()[0].commit_cycle == -1);
        t.Commit(12);
        check("commit 3/3: commitou no ciclo 12 e removeu a cabeça",
            t.GetTable()[0].commit_cycle == 12 && t.GetROBEntries().empty());
        check("temporização do STORE nunca foi gravada no trace",
            t.GetTable()[0].mem_cycles.empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. DEPENDÊNCIAS E HAZARDS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. DEPENDÊNCIAS E HAZARDS");

    section("4.1 RAW: sub espera add terminar antes de executar");
    {
        std::vector<std::string> prog = {"add r3, r1, r2", "sub r5, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        t.Issue(2);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);
        t.ExMem(4); t.Wr(4);
        t.ExMem(5); t.Wr(5);

        auto tab = t.GetTable();
        check("RAW: add issue == 1",                           tab[0].issue_cycles.front() == 1);
        check("RAW: add ex_cycles[0] == 2 (inicio)",           tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("RAW: add ex_cycles[1] == 2 (fim)",              tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("RAW: add WR == 3",                              tab[0].wr_cycle == 3);

        check("RAW: sub issue == 2",                           tab[1].issue_cycles.front() == 2);
        check("RAW: sub ex_cycles[0] == 4 (inicio, após RAW)", tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 4);
        check("RAW: sub ex_cycles[1] == 4 (fim)",              tab[1].ex_cycles.size() == 2 && tab[1].ex_cycles[1] == 4);
        check("RAW: sub WR == 5",                              tab[1].wr_cycle == 5);

        bool all_done = t.ExMem(6);
        check("RAW: ExMem retorna true após tudo concluído", all_done);
    }

    section("4.2 WAW: duas escritas no mesmo registrador liberam a RS certa (por posição, não por registrador)");
    {
        // Regressão do bug corrigido: release por posição em vez de por registrador de destino.
        std::vector<std::string> prog = {"add r3, r1, r2", "sub r3, r4, r5"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        t.Issue(2);
        for (int c = 2; c <= 5; c++) { t.ExMem(c); t.Wr(c); }

        auto tab = t.GetTable();
        check("WAW: add (posição 0) recebeu WR",  tab[0].wr_cycle > 0);
        check("WAW: sub (posição 1) recebeu WR",  tab[1].wr_cycle > 0);
        check("WAW: WR de posições diferentes (não confundiu as duas RS)",
              tab[0].wr_cycle != tab[1].wr_cycle);
        check("WAW: ambas as RS de int_basic ficaram livres no fim",
              !t.GetRS().GetIntBasicStations()[0].IsBusy() &&
              !t.GetRS().GetIntBasicStations()[1].IsBusy());
    }

    section("4.2.1 WAW+RAW: produtor antigo não libera consumidor do mais novo");
    {
        std::vector<std::string> prog{
            "add r3, r1, r2",
            "mul r3, r4, r5",
            "sub r6, r3, r7"
        };
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        t.Issue(2);
        t.Issue(3);

        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);
        check("WR do produtor posição 0 ocorreu primeiro", t.GetTable()[0].wr_cycle == 3);
        check("consumidor continua bloqueado pelo produtor posição 1",
            t.GetTable()[2].ex_cycles.empty());

        for (int cycle = 4; cycle <= 7; cycle++) {
            t.ExMem(cycle);
            t.Wr(cycle);
        }
        check("produtor mais novo também concluiu", t.GetTable()[1].wr_cycle > 0);
        check("consumidor iniciou somente após o WR do produtor mais novo",
            !t.GetTable()[2].ex_cycles.empty() &&
            t.GetTable()[2].ex_cycles.front() > t.GetTable()[1].wr_cycle);
    }

    section("4.3 Hazard estrutural de FU: 2 muls disputando a única FU int_mult_div_alu");
    {
        // DEFAULT_NUM_FUS = {1,1,1,1,1,2} → só 1 FU de int_mult_div_alu.
        std::vector<std::string> prog = {"mul r1,r2,r3", "mul r4,r5,r6"}; // sem dependência entre si
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1);
        t.Issue(2);
        for (int c = 2; c <= 6; c++) { t.ExMem(c); t.Wr(c); }

        auto tab = t.GetTable();
        check("FU hazard: 1o mul entra em EX no ciclo 2",  tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("FU hazard: 2o mul só entra em EX no ciclo 6 (após a FU ser liberada)",
              tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 6);
        check("FU hazard: 2o mul começou estritamente depois do 1o terminar o EX",
              tab[1].ex_cycles[0] > tab[0].ex_cycles[1]);
    }

    section("4.4 Hazard estrutural de porta WR (fu.wr=1): duas add prontas no mesmo ciclo, só 1 escreve por vez");
    {
        // int_basic_alu=2 (evita disputa de FU na EX) + wr=1 (força disputa só na porta de WR).
        std::vector<int> fus_custom = {1,2,1,1,1,1};
        std::vector<std::string> prog = {"add r1,r2,r3", "add r4,r5,r6"}; // sem dependência entre si
        Thread t(prog, {}, DEFAULT_NUM_RS, fus_custom);

        t.Issue(1);
        t.Issue(2);
        t.ExMem(2); t.Wr(2); // ambas entram e terminam o EX no mesmo ciclo (lat=1, 2 FUs livres)
        check("WR hazard: ambas terminaram o EX no ciclo 2",
              t.GetTable()[0].ex_cycles == std::vector<int>{2,2} &&
              t.GetTable()[1].ex_cycles == std::vector<int>{2,2});

        t.ExMem(3); t.Wr(3); // fu.wr=1 → só a 1a (posição 0) consegue escrever
        check("WR hazard: add posição 0 escreve no ciclo 3", t.GetTable()[0].wr_cycle == 3);
        check("WR hazard: add posição 1 ainda NÃO escreveu (porta ocupada)", t.GetTable()[1].wr_cycle == -1);

        t.ExMem(4); t.Wr(4); // ciclo seguinte, a porta libera para a 2a
        check("WR hazard: add posição 1 escreve no ciclo 4 (um ciclo depois)", t.GetTable()[1].wr_cycle == 4);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. BRANCH");

    // Um ciclo de processador equivale a: ExMem -> Wr -> Commit -> Issue
    // (mesma ordem de Processor::ExecuteCycle()).
    auto cycle = [](Thread& t, int c){ t.ExMem(c); t.Wr(c); t.Commit(c); t.Issue(c); };

    section("5.1 Branch SEM ROB (lat EX=1): instrução seguinte só começa o EX após o branch");
    {
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS); // 1 FU int_basic_alu

        cycle(t, 1); // bnez emitido (unresolved_branch_position = 0)
        check("ciclo 1: bnez emitido", t.GetTable()[0].issue_cycles.front() == 1);

        cycle(t, 2); // bnez entra em EX (2-2); add emitido, filtrado pelo stall
        check("ciclo 2: bnez em EX", t.GetTable()[0].ex_cycles.size() >= 1);
        check("ciclo 2: add ainda NÃO entrou em EX", t.GetTable()[1].ex_cycles.empty());

        cycle(t, 3); // branch terminou o EX no ciclo 2 (lat=1) → add liberado
        check("ciclo 3: bnez ex == [2,2]", t.GetTable()[0].ex_cycles == std::vector<int>{2,2});
        check("ciclo 3: add entra em EX logo após o branch terminar",
              t.GetTable()[1].ex_cycles.size() >= 1 && t.GetTable()[1].ex_cycles[0] == 3);
    }

    section("5.2 Branch COM ROB: instrução seguinte NÃO trava (independente de previsor)");
    {
        // bnez e add disputam a mesma FU int_basic_alu; com o pool padrão (1 FU)
        // o add ficaria preso por hazard estrutural, não por branch stall — usa-se
        // 2 FUs para isolar o comportamento do branch (mesma técnica da seção 4.4).
        std::vector<int> fus_custom = {1, 2, 1, 1, 1, 1};
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, fus_custom, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/4);

        // Despacho de largura 2 no ciclo 1 (como Processor::ExecuteIssue faria).
        t.Issue(1); t.Issue(1);
        cycle(t, 2); // bnez e add entram em EX juntos (sem stall)
        check("com ROB: add entra em EX no mesmo ciclo do branch (sem stall)",
              !t.GetTable()[1].ex_cycles.empty() && t.GetTable()[1].ex_cycles[0] == 2);
    }

    section("5.3 Branch final marca ready_cycle no ciclo de resolução");
    {
        std::vector<std::string> prog = {"bnez r1, foo"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/4);

        t.Issue(1);
        t.ExMem(2); t.Wr(2);
        check("Branch pronto vem do ROB, no ciclo final do EX",
            t.GetROBEntries().size() == 1 &&
            t.GetROBEntries()[0].ready &&
            t.GetROBEntries()[0].ready_cycle == 2 &&
            t.GetTable()[0].commit_cycle == -1);

        t.Commit(2);
        check("Branch pode commitar no ciclo de resolução",
            t.GetTable()[0].commit_cycle == 2 && t.GetROBEntries().empty());
    }

    section("5.4 Commit() de BRANCH sem previsor: serializa 1 por ciclo mesmo com ROB");
    {
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/4,
                 /*has_predictor=*/false);

        for (int c = 1; c <= 8; c++) cycle(t, c);

        check("BRANCH commitou", t.GetTable()[0].commit_cycle > 0);
        // Sem previsor, o Commit() dá 'break' logo após commitar o BRANCH — o add só
        // deve commitar em um ciclo POSTERIOR ao do branch, nunca no mesmo ciclo.
        check("add commitou depois, não no mesmo ciclo do branch",
              t.GetTable()[1].commit_cycle > t.GetTable()[0].commit_cycle);
    }

    section("5.5 Branch SEM ROB com EX latência 4: add só entra em EX após o branch terminar");
    {
        // bnez com exLat=4 (via latency_overrides). Com 1 FU o add também seria atrasado
        // por hazard estrutural — a flag e a FU agem na mesma direção.
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {{0, {4}, {0}}}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        t.Issue(1); t.Issue(1); // despacho largura 2 no ciclo 1
        cycle(t, 2);            // bnez entra em EX (2-5); add filtrado pela flag
        check("lat4: bnez entrou em EX no ciclo 2",
              t.GetTable()[0].ex_cycles.size() >= 1 && t.GetTable()[0].ex_cycles[0] == 2);
        check("lat4: add ainda não entrou em EX", t.GetTable()[1].ex_cycles.empty());

        cycle(t, 3); cycle(t, 4); cycle(t, 5); // bnez executa até o ciclo 5
        check("lat4: bnez terminou EX no ciclo 5",
              t.GetTable()[0].ex_cycles.size() == 2 && t.GetTable()[0].ex_cycles[1] == 5);
        check("lat4: add continua bloqueado durante o EX do branch", t.GetTable()[1].ex_cycles.empty());

        cycle(t, 6);
        check("lat4: add entra em EX no ciclo 6 (após o branch terminar)",
              t.GetTable()[1].ex_cycles.size() >= 1 && t.GetTable()[1].ex_cycles[0] == 6);
    }

    section("5.6 Branch SEM ROB com EX latência 4 e 2 FUs: flag segura até o fim do EX");
    {
        // Com 2 FUs int_basic_alu não há hazard estrutural mascarando o teste:
        // o add só pode entrar em EX quando a flag for limpa (fim do EX do branch).
        std::vector<int> fus_custom = {1, 2, 1, 1, 1, 1};
        std::vector<std::string> prog = {"bnez r1, foo", "add r2, r3, r4"};
        Thread t(prog, {{0, {4}, {0}}}, DEFAULT_NUM_RS, fus_custom);

        t.Issue(1); t.Issue(1); // despacho largura 2 no ciclo 1
        cycle(t, 2);             // bnez entra em EX (2-5); add filtrado pela flag
        check("lat4+2FUs: add ainda não entrou em EX", t.GetTable()[1].ex_cycles.empty());

        cycle(t, 3); cycle(t, 4); cycle(t, 5);
        check("lat4+2FUs: bnez terminou EX no ciclo 5",
              t.GetTable()[0].ex_cycles.size() == 2 && t.GetTable()[0].ex_cycles[1] == 5);
        check("lat4+2FUs: add bloqueado mesmo com FU livre (flag)", t.GetTable()[1].ex_cycles.empty());

        cycle(t, 6);
        check("lat4+2FUs: add entra em EX no ciclo 6",
              t.GetTable()[1].ex_cycles.size() >= 1 && t.GetTable()[1].ex_cycles[0] == 6);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. Commit() (tudo com ROB)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. Commit() (tudo com ROB)");

    section("6.1 ready_cycle e cabeça do ROB controlam Commit em ordem");
    {
        std::vector<std::string> prog = {"mul r1,r2,r3", "add r4,r5,r6"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, /*dispatch_width=*/2, /*rob_capacity=*/4);

        t.Issue(1); t.Issue(1);
        for (int c = 2; c <= 3; c++) {
            t.ExMem(c); t.Wr(c); t.Commit(c);
        }
        check("resultado mais novo fica pronto sem ultrapassar a cabeça",
            t.GetROBEntries().size() == 2 &&
            !t.GetROBEntries()[0].ready &&
            t.GetROBEntries()[1].ready &&
            t.GetROBEntries()[1].ready_cycle == 4 &&
            t.GetTable()[1].commit_cycle == -1);

        for (int c = 4; c <= 6; c++) {
            t.ExMem(c); t.Wr(c); t.Commit(c);
        }
        check("resultado da cabeça espera o ciclo posterior ao WR",
            t.GetROBEntries()[0].ready &&
            t.GetROBEntries()[0].ready_cycle == 7 &&
            t.GetTable()[0].wr_cycle == 6 &&
            t.GetTable()[0].commit_cycle == -1 &&
            t.GetTable()[1].commit_cycle == -1);

        t.Commit(7);
        check("cabeça e resultado posterior commitam em ordem no ciclo elegível",
            t.GetTable()[0].commit_cycle == 7 &&
            t.GetTable()[1].commit_cycle == 7 &&
            t.GetROBEntries().empty());
    }

    section("6.2 fu.commit limita commits por ciclo");
    {
        std::vector<std::string> prog = {"add r1,r2,r3", "sub r4,r5,r6", "or r7,r8,r9"};
        // dispatch_width = 1 → fu.commit = 1 (só 1 commit por ciclo).
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, /*dispatch_width=*/1, /*rob_capacity=*/8);

        t.Issue(1); t.Issue(2); t.Issue(3);
        for (int c = 2; c <= 6; c++) { t.ExMem(c); t.Wr(c); }

        t.Commit(10);
        int committed_by_cycle10 = (t.GetTable()[0].commit_cycle == 10) +
                                 (t.GetTable()[1].commit_cycle == 10) +
                                 (t.GetTable()[2].commit_cycle == 10);
        check("fu.commit=1: no máximo 1 instrução commita por ciclo", committed_by_cycle10 <= 1);
    }

    section("6.3 ROB esvaziando libera espaço para novos Issue()");
    {
        std::vector<std::string> prog = {"add r1,r2,r3", "add r4,r5,r6", "add r7,r8,r9"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/2);

        t.Issue(1); t.Issue(2);
        check("ROB cheio (cap=2): 3a issue falha",
            t.Issue(3).outcome == ISSUE_OUTCOME::BLOCKED);

        for (int c = 2; c <= 4; c++) { t.ExMem(c); t.Wr(c); }
        for (int c = 5; c <= 6; c++) t.Commit(c);

        check("após commit liberar o ROB, 3a issue consegue entrar",
            t.Issue(7).outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
    }

    section("6.4 STORE só inicia memória depois de alcançar a cabeça");
    {
        std::vector<std::string> prog = {"mul r1,r2,r3", "s.d f2, 0(r3)"};
        Thread t(
            prog,
            {{1, {1}, {3}}},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS,
            {},
            /*dispatch_width=*/2,
            /*rob_capacity=*/4
        );

        t.Issue(1); t.Issue(1);
        for (int c = 2; c <= 3; c++) {
            t.ExMem(c); t.Wr(c); t.Commit(c);
        }
        check("STORE concluído atrás da cabeça não inicia temporização",
            t.GetROBEntries().size() == 2 &&
            t.GetROBEntries()[1].store_state == ROB_STORE_STATE::WAITING_MEMORY &&
            t.GetROBEntries()[1].ready_cycle == -1 &&
            t.GetTable()[1].mem_cycles.empty());

        for (int c = 4; c <= 6; c++) {
            t.ExMem(c); t.Wr(c); t.Commit(c);
        }
        check("STORE continua aguardando enquanto a cabeça não é elegível",
            t.GetROBEntries().size() == 2 &&
            t.GetROBEntries()[1].store_state == ROB_STORE_STATE::WAITING_MEMORY &&
            t.GetROBEntries()[1].ready_cycle == -1);

        t.Commit(7);
        check("STORE inicia memória ao tornar-se cabeça",
            t.GetTable()[0].commit_cycle == 7 &&
            t.GetTable()[1].commit_cycle == -1 &&
            t.GetROBEntries().size() == 1 &&
            t.GetROBEntries()[0].position == 1 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::EXECUTING_MEMORY &&
            t.GetROBEntries()[0].ready_cycle == 9);
        t.Commit(8);
        t.Commit(9);
        check("STORE commita após latência sem alterar trace MEM",
            t.GetTable()[1].commit_cycle == 9 &&
            t.GetTable()[1].mem_cycles.empty() &&
            t.GetROBEntries().empty());
    }

    section("6.5 STORE na cabeça espera concluir a execução final");
    {
        std::vector<std::string> prog = {"s.d f2, 0(r3)"};
        Thread t(
            prog,
            {{0, {4}, {2}}},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS,
            {},
            DEFAULT_DISPATCH_WIDTH,
            /*rob_capacity=*/4
        );

        t.Issue(1);
        for (int c = 2; c <= 5; c++) {
            t.ExMem(c); t.Wr(c); t.Commit(c);
        }
        check("STORE não inicia memória durante o EX",
            t.GetTable()[0].ex_cycles == std::vector<int>({2, 5}) &&
            t.GetTable()[0].commit_cycle == -1 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::WAITING_EXECUTION &&
            t.GetROBEntries()[0].ready_cycle == -1);

        t.ExMem(6); t.Wr(6); t.Commit(6);
        check("STORE inicia memória somente depois da liberação final",
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::EXECUTING_MEMORY &&
            t.GetROBEntries()[0].ready_cycle == 7 &&
            t.GetTable()[0].commit_cycle == -1);
        t.Commit(7);
        check("STORE lento commita após EX e MEM funcionais",
            t.GetTable()[0].commit_cycle == 7 &&
            t.GetTable()[0].mem_cycles.empty() &&
            t.GetROBEntries().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. INTEGRAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. INTEGRAÇÃO");

    section("7.1 ExMem() retorna true após todas as instruções finalizadas (sem ROB)");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS);

        check("antes: false com instrução pendente", !t.ExMem(1));

        t.Issue(1);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);

        bool fim = t.ExMem(4);
        check("depois: true quando todas instruções finalizadas", fim);
        check("chamada extra ainda retorna true", t.ExMem(5));
    }

    section("7.2 Programa completo COM ROB: issue -> exec -> commit até esvaziar");
    {
        std::vector<std::string> prog = {"add r1,r2,r3", "sub r4,r1,r5", "mul r6,r4,r1"};
        Thread t(prog, {}, DEFAULT_NUM_RS, DEFAULT_NUM_FUS, {}, DEFAULT_DISPATCH_WIDTH, /*rob_capacity=*/8);

        int cycle = 1;
        while (t.GetCurrentInstructionPosition() < static_cast<int>(t.GetTable().size()))
            if (t.Issue(cycle).outcome != ISSUE_OUTCOME::BLOCKED) cycle++;
        int max_cycles = 50;
        while (max_cycles-- > 0) {
            t.ExMem(cycle);
            t.Wr(cycle);
            t.Commit(cycle);
            cycle++;
        }

        auto tab = t.GetTable();
        check("integração: todas as instruções passaram por issue",
              tab[0].issue_cycles.front() > 0 && tab[1].issue_cycles.front() > 0 && tab[2].issue_cycles.front() > 0);
        check("integração: todas as instruções commitaram",
              tab[0].commit_cycle > 0 && tab[1].commit_cycle > 0 && tab[2].commit_cycle > 0);
        check("integração: ordem de commit respeitada (posição 0 <= 1 <= 2)",
              tab[0].commit_cycle <= tab[1].commit_cycle && tab[1].commit_cycle <= tab[2].commit_cycle);
    }

    // ════════════════════════════════════════════════════════════════════
    // 8. INSTRUÇÕES MULTI-RS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("8. INSTRUÇÕES MULTI-RS");

    section("8.1 LOAD -> INT_MUL -> STORE usa três Issues e uma identidade");
    {
        std::vector<std::unique_ptr<Instruction>> instructions;
        instructions.push_back(MakeSyntheticInstruction(0, "multi"));
        Thread t(
            std::move(instructions),
            {{0, {1, 1, 1}, {1, 0, 1}}},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS
        );

        const std::vector<INSTRUCTION_TYPE> original_types{
            t.GetTable()[0].instruction->GetInstructionTypes()
        };
        const std::vector<int> original_ex_latencies{
            t.GetTable()[0].instruction->GetExLatencies()
        };
        const std::vector<int> original_mem_latencies{
            t.GetTable()[0].instruction->GetMemLatencies()
        };

        ISSUE_RESULT first_issue{t.Issue(1)};
        check("primeiro Issue avança o frontend",
            first_issue.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION &&
            first_issue.position == 0 &&
            first_issue.instruction_type == INSTRUCTION_TYPE::LOAD &&
            t.GetCurrentInstructionPosition() == 1);
        check("primeiro Issue cria entrada in-flight na etapa zero",
            t.GetInFlightEntries().size() == 1 &&
            t.GetInFlightEntries()[0].current_stage == 0 &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::ALLOCATED);

        t.ExMem(2); t.Wr(2);
        check("etapa em execução não recebe segundo Issue",
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::EXECUTING &&
            t.Issue(2).outcome == ISSUE_OUTCOME::BLOCKED);

        t.ExMem(3); t.Wr(3);
        check("LOAD intermediário libera a RS e avança o cursor",
            t.GetInFlightEntries()[0].current_stage == 1 &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::WAITING_ISSUE &&
            t.GetInFlightEntries()[0].next_issue_cycle == 4 &&
            t.GetTable()[0].instruction->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("fim no ciclo 3 não permite Issue no mesmo ciclo",
            t.Issue(3).outcome == ISSUE_OUTCOME::BLOCKED);
        check("destino permanece reservado após LOAD intermediário",
            t.GetRegisterStatus().FindLatestProducerBefore(Register('R', 6), 1) == 0 &&
            !t.GetRegisterStatus().IsProducerResolved(Register('R', 6), 0) &&
            t.GetTable()[0].wr_cycle == -1);

        ISSUE_RESULT second_issue{t.Issue(4)};
        check("INT_MUL recebe Issue interno explícito no ciclo seguinte",
            second_issue.outcome == ISSUE_OUTCOME::INTERNAL_STAGE &&
            second_issue.position == 0 &&
            second_issue.instruction_type == INSTRUCTION_TYPE::INT_MUL &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::ALLOCATED &&
            t.GetCurrentInstructionPosition() == 1);
        t.ExMem(5); t.Wr(5);
        check("INT_MUL intermediário libera a RS e avança o cursor",
            t.GetInFlightEntries()[0].current_stage == 2 &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::WAITING_ISSUE &&
            t.GetInFlightEntries()[0].next_issue_cycle == 6 &&
            t.GetTable()[0].instruction->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("fim no ciclo 5 não permite Issue no mesmo ciclo",
            t.Issue(5).outcome == ISSUE_OUTCOME::BLOCKED);
        check("destino permanece reservado após INT_MUL intermediário",
            t.GetRegisterStatus().FindLatestProducerBefore(Register('R', 6), 1) == 0 &&
            !t.GetRegisterStatus().IsProducerResolved(Register('R', 6), 0) &&
            t.GetTable()[0].wr_cycle == -1);

        ISSUE_RESULT third_issue{t.Issue(6)};
        check("STORE recebe terceiro Issue interno sem avançar frontend",
            third_issue.outcome == ISSUE_OUTCOME::INTERNAL_STAGE &&
            third_issue.position == 0 &&
            third_issue.instruction_type == INSTRUCTION_TYPE::STORE &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::ALLOCATED &&
            t.GetCurrentInstructionPosition() == 1);
        t.ExMem(7); t.Wr(7);
        check("STORE permanece em execução entre EX e MEM",
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::EXECUTING &&
            t.Issue(7).outcome == ISSUE_OUTCOME::BLOCKED);
        t.ExMem(8); t.Wr(8);
        check("etapa final aguarda WR sem novo Issue",
            t.Issue(8).outcome == ISSUE_OUTCOME::BLOCKED &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::WAITING_WR);
        check("destino só é liberado pelo resultado final",
            t.GetRegisterStatus().FindLatestProducerBefore(Register('R', 6), 1) == 0 &&
            !t.GetRegisterStatus().IsProducerResolved(Register('R', 6), 0));

        t.ExMem(9); t.Wr(9);
        const TABLE_ROW& row{t.GetTable()[0]};
        check("cronologia multi-RS é exata",
            row.issue_cycles == std::vector<int>({1, 4, 6}) &&
            row.ex_cycles == std::vector<int>({2, 2, 5, 5, 7, 7}) &&
            row.mem_cycles == std::vector<int>({3, 3, 8, 8}));
        check("resultado final libera destino e encerra a instrução",
            t.GetRegisterStatus().IsProducerResolved(Register('R', 6), 0) &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::FINISHED &&
            t.ExMem(10));
        check("posição lógica permanece imutável", row.instruction->GetPosition() == 0);
        check("descrição completa permanece imutável após a execução",
            row.instruction->GetInstructionTypes() == original_types &&
            row.instruction->GetExLatencies() == original_ex_latencies &&
            row.instruction->GetMemLatencies() == original_mem_latencies);
    }

    section("8.2 ROB cheio bloqueia instrução nova, mas não Issue interno");
    {
        std::vector<std::unique_ptr<Instruction>> instructions;
        instructions.push_back(MakeSyntheticInstruction(0, "multi"));
        instructions.push_back(MakeSimplifiedInstruction(1, "add r7, r1, r2"));
        Thread t(
            std::move(instructions),
            {{0, {1, 1, 1}, {1, 0, 1}}},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS,
            {},
            DEFAULT_DISPATCH_WIDTH,
            /*rob_capacity=*/1
        );

        check("primeira instrução ocupa a única entrada do ROB",
            t.Issue(1).outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        check("primeiro Issue cria exatamente uma entrada lógica",
            t.GetROBEntries().size() == 1 &&
            t.GetROBEntries()[0].position == 0 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::WAITING_EXECUTION);
        check("ROB cheio bloqueia a próxima instrução",
            t.Issue(1).outcome == ISSUE_OUTCOME::BLOCKED);
        t.ExMem(2); t.Wr(2); t.Commit(2);
        t.ExMem(3); t.Wr(3); t.Commit(3);
        check("ciclo de término ainda não reemite a etapa",
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::WAITING_ISSUE &&
            t.GetInFlightEntries()[0].next_issue_cycle == 4 &&
            t.Issue(3).outcome == ISSUE_OUTCOME::BLOCKED);
        check("Issue interno ignora capacidade cheia e preserva cursor",
            t.Issue(4).outcome == ISSUE_OUTCOME::INTERNAL_STAGE &&
            t.GetCurrentInstructionPosition() == 1);
        check("Issue interno reutiliza a entrada existente do ROB",
            t.GetROBEntries().size() == 1 &&
            t.GetROBEntries()[0].position == 0 &&
            t.GetROBEntries()[0].store_state == ROB_STORE_STATE::WAITING_EXECUTION);
        check("ROB não admitiu a segunda posição",
            t.GetTable()[0].issue_cycles == std::vector<int>({1, 4}) &&
            t.GetTable()[1].issue_cycles.empty());
    }

    section("8.3 Candidato bloqueado não impede etapa apta de outro grupo");
    {
        std::vector<std::unique_ptr<Instruction>> instructions;
        instructions.push_back(MakeSyntheticInstruction(0, "multi"));
        instructions.push_back(MakeSimplifiedInstruction(1, "mul r7, r1, r2"));
        instructions.push_back(MakeSyntheticInstruction(2, "load_store"));
        const std::vector<int> constrained_rs{2, 1, 1, 1, 1, 1};
        Thread t(
            std::move(instructions),
            {
                {0, {1, 1, 1}, {1, 0, 1}},
                {1, {6}, {0}},
                {2, {1, 1}, {1, 1}}
            },
            constrained_rs,
            DEFAULT_NUM_FUS,
            {},
            /*dispatch_width=*/3
        );

        const ISSUE_RESULT first_new{t.Issue(1)};
        const ISSUE_RESULT second_new{t.Issue(1)};
        const ISSUE_RESULT third_new{t.Issue(1)};
        check("três instruções novas recebem Issue no ciclo 1",
            first_new.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION &&
            second_new.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION &&
            third_new.outcome == ISSUE_OUTCOME::NEW_INSTRUCTION);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);
        check("etapa recém-concluída não reemite no ciclo 3",
            t.Issue(3).outcome == ISSUE_OUTCOME::BLOCKED);
        t.ExMem(4); t.Wr(4);
        check("LOAD concluído no ciclo 4 não reemite no mesmo ciclo",
            t.Issue(4).outcome == ISSUE_OUTCOME::BLOCKED &&
            t.GetTable()[2].mem_cycles == std::vector<int>({4, 4}));
        t.ExMem(5); t.Wr(5);
        ISSUE_RESULT available_internal{t.Issue(5)};
        check("STORE mais novo recebe Issue enquanto INT_MUL antigo segue bloqueado",
            available_internal.outcome == ISSUE_OUTCOME::INTERNAL_STAGE &&
            available_internal.position == 2 &&
            available_internal.instruction_type == INSTRUCTION_TYPE::STORE &&
            t.GetTable()[0].issue_cycles == std::vector<int>({1}) &&
            t.GetInFlightEntries()[0].current_stage == 1 &&
            t.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::WAITING_ISSUE &&
            t.GetTable()[0].instruction->GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
            t.GetInFlightEntries()[1].state == IN_FLIGHT_STATE::EXECUTING &&
            t.GetTable()[2].issue_cycles == std::vector<int>({1, 5}) &&
            t.GetInFlightEntries()[2].current_stage == 1 &&
            t.GetInFlightEntries()[2].state == IN_FLIGHT_STATE::ALLOCATED &&
            t.GetCurrentInstructionPosition() == 3);
    }

    // ════════════════════════════════════════════════════════════════════
    // 9. INDEPENDÊNCIA DO TRACE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("9. INDEPENDÊNCIA DO TRACE");

    section("9.1 Limpar e desabilitar trace não altera decisões funcionais");
    {
        const std::vector<std::string> program{"add r1, r2, r3"};
        Thread control(
            program,
            {},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS,
            {},
            DEFAULT_DISPATCH_WIDTH,
            /*rob_capacity=*/4
        );
        Thread without_trace(
            program,
            {},
            DEFAULT_NUM_RS,
            DEFAULT_NUM_FUS,
            {},
            DEFAULT_DISPATCH_WIDTH,
            /*rob_capacity=*/4
        );

        const ISSUE_RESULT control_issue{control.Issue(1)};
        const ISSUE_RESULT without_trace_issue{without_trace.Issue(1)};
        check("Issue produz o mesmo resultado antes da limpeza",
            control_issue.outcome == without_trace_issue.outcome &&
            control_issue.position == without_trace_issue.position &&
            control_issue.instruction_type == without_trace_issue.instruction_type);

        control.ExMem(2); control.Wr(2); control.Commit(2);
        without_trace.ExMem(2); without_trace.Wr(2); without_trace.Commit(2);
        check("trace possui eventos antes da limpeza",
            !IsTraceEmpty(without_trace.GetTable()));

        without_trace.ClearTrace();
        without_trace.SetTraceEnabled(false);
        check("limpeza preserva linha e identidade da instrução",
            IsTraceEmpty(without_trace.GetTable()) &&
            without_trace.GetTable().size() == 1 &&
            without_trace.GetTable()[0].instruction->GetPosition() == 0);

        bool same_completion{
            control.ExMem(3) == without_trace.ExMem(3)
        };
        control.Wr(3); control.Commit(3);
        without_trace.Wr(3); without_trace.Commit(3);
        check("ROB recebe a mesma prontidão sem consultar o trace",
            control.GetROBEntries().size() == 1 &&
            without_trace.GetROBEntries().size() == 1 &&
            control.GetROBEntries()[0].ready == without_trace.GetROBEntries()[0].ready &&
            control.GetROBEntries()[0].ready_cycle == without_trace.GetROBEntries()[0].ready_cycle);

        same_completion = same_completion &&
            control.ExMem(4) == without_trace.ExMem(4);
        control.Wr(4); control.Commit(4);
        without_trace.Wr(4); without_trace.Commit(4);
        same_completion = same_completion &&
            control.ExMem(5) == without_trace.ExMem(5);

        check("conclusão funcional permanece idêntica",
            same_completion &&
            control.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::FINISHED &&
            without_trace.GetInFlightEntries()[0].state == IN_FLIGHT_STATE::FINISHED &&
            control.GetROBEntries().empty() &&
            without_trace.GetROBEntries().empty() &&
            control.GetRegisterStatus().IsProducerResolved(Register('R', 1), 0) &&
            without_trace.GetRegisterStatus().IsProducerResolved(Register('R', 1), 0));
        check("eventos posteriores continuam desabilitados",
            IsTraceEmpty(without_trace.GetTable()) &&
            control.GetTable()[0].wr_cycle == 3 &&
            control.GetTable()[0].commit_cycle == 4);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
