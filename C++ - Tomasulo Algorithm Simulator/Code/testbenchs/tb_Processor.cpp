/* tb_Processor.cpp /*/
// Testbench de integração de Processor.cpp
#include "../headers/Processor.h"
#include "../headers/Architecture.h"
#include "tb_Helpers.h"
#include <vector>

using namespace processor;

// Monta configurações específicas sem duplicar os defaults do produto.
static PIPELINE_CONFIGURATION MakeProcessorConfiguration(
    const int                             issue_width,
    const RESERVATION_STATION_CAPACITIES& reservation_station_capacities = {},
    const FUNCTIONAL_UNIT_CAPACITIES&     functional_unit_capacities = {},
    const int                             write_result_width = 2
) {
    PIPELINE_CONFIGURATION configuration;
    configuration.reservation_station_capacities = reservation_station_capacities;
    configuration.functional_unit_capacities = functional_unit_capacities;
    configuration.issue_width = issue_width;
    configuration.commit_width = issue_width;
    configuration.write_result_width = write_result_width;
    return configuration;
}

static int runUntilEnd(Processor& p, int limit = 200) {
    int c = 0;
    while (c++ < limit)
        if (p.ExecuteCycle()) return c;
    return -1;
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E CONFIGURAÇÃO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E CONFIGURAÇÃO");

    section("1.1 Construtor — 1 thread, TOMASULO_CLASSIC");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        check("GetThreadTable(0).size() == 1", p.GetThreadTable(0).size() == 1);
        check("instrução na posição 0 é add (INT_BASIC)",
              p.GetThreadTable(0)[0].instruction->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
    }

    section("1.2 Programa vazio — caso de borda trivial");
    {
        std::vector<std::string> prog = {};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        int end_cycle = runUntilEnd(p);
        check("Programa vazio: encerra imediatamente no ciclo inicial", end_cycle == 1);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] Construtor: mais de uma thread sem modelo de multithreading deve abortar");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        // model == NONE com num_threads > 1 → combinação inválida, espera abort().
        Processor p(2, false,
                    PROCESSOR_TYPE::TOMASULO_CLASSIC,
                    MULTITHREADING_MODEL::NONE,
                    prog, MakeProcessorConfiguration(1));
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. PIPELINE DE UMA INSTRUÇÃO (TRAÇOS COMPLETOS)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. PIPELINE DE UMA INSTRUÇÃO (TRAÇOS COMPLETOS)");

    section("2.1 add — issue->EX->WR em 3 ciclos (ExecuteCycle() retorna true)");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        int end_cycle = runUntilEnd(p);
        check("add: terminou (não atingiu limite)", end_cycle != -1);

        auto tab = p.GetThreadTable(0);
        check("add: issue == 1",                tab[0].issue_cycles.front() == 1);
        check("add: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("add: ex_cycles[1] == 2 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("add: WR == 3",                    tab[0].wr_cycle == 3);
    }

    section("2.2 LOAD completo — issue->EX->MEM->WR");
    {
        std::vector<std::string> prog = {"l.d f2, 0(r1)"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        int end_cycle = runUntilEnd(p);
        check("LOAD: terminou", end_cycle != -1);

        auto tab = p.GetThreadTable(0);
        check("LOAD: issue == 1",                  tab[0].issue_cycles.front() == 1);
        check("LOAD: ex_cycles[0] == 2 (inicio)",  tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("LOAD: ex_cycles[1] == 2 (fim)",     tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("LOAD: mem_cycles[0] == 3 (inicio)", tab[0].mem_cycles.size() >= 1 && tab[0].mem_cycles[0] == 3);
        check("LOAD: mem_cycles[1] == 3 (fim)",    tab[0].mem_cycles.size() == 2 && tab[0].mem_cycles[1] == 3);
        check("LOAD: WR == 4",                     tab[0].wr_cycle == 4);
    }

    section("2.3 mul multiciclo via Processor (exLat=4)");
    {
        std::vector<std::string> prog = {"mul r3, r1, r2"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("mul: issue == 1",                 tab[0].issue_cycles.front() == 1);
        check("mul: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("mul: ex_cycles[1] == 5 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 5);
        check("mul: WR == 6",                    tab[0].wr_cycle == 6);
        check("mul: mem_cycles vazio",           tab[0].mem_cycles.empty());
    }

    section("2.4 Modificação customizada de Latência (latency_overrides via parâmetro)");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        // Tupla: <posição_instrucao, ex_latencies, mem_latencies>
        // Vamos forçar o add (que normalmente leva 1 ciclo) a levar 5 ciclos de EX
        std::vector<LATENCY_OVERRIDE> lat = {{0, {5}, {0}}};

        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1), {}, lat);

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Latência Injetada: add levou exatos 5 ciclos em EX",
            tab[0].ex_cycles.back() - tab[0].ex_cycles.front() + 1 == 5);
        check("Latência Injetada: EX começou no ciclo 2", tab[0].ex_cycles.front() == 2);
        check("Latência Injetada: EX terminou no ciclo 6", tab[0].ex_cycles.back() == 6);
        check("Latência Injetada: WR ocorreu no ciclo 7", tab[0].wr_cycle == 7);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. DESPACHO E MULTITHREADING
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. DESPACHO E MULTITHREADING");

    section("3.1 Superscalar (largura=2): 2 instruções independentes no ciclo 1");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};
        const FUNCTIONAL_UNIT_CAPACITIES functional_units{1, 2, 1, 1, 1};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2, {}, functional_units));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Superscalar: add[0] issue == 1", tab[0].issue_cycles.front() == 1);
        check("Superscalar: add[1] issue == 1", tab[1].issue_cycles.front() == 1);
        check("Superscalar: ambas terminam no mesmo WR",
              tab[0].wr_cycle == tab[1].wr_cycle);
    }

    section("3.2 2 threads, FINE_GRAINED, largura=1: thread mantém prioridade até esgotar");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};
        Processor p(2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog, MakeProcessorConfiguration(1));

        runUntilEnd(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("FINA: T0[0] issue == 1", t0[0].issue_cycles.front() == 1);
        check("FINA: T0[1] issue == 2", t0[1].issue_cycles.front() == 2);
        check("FINA: T1[0] issue == 3", t1[0].issue_cycles.front() == 3);
        check("FINA: T1[1] issue == 4", t1[1].issue_cycles.front() == 4);
    }

    section("3.3 2 threads, SMT, largura=2: 1 issue por thread por ciclo");
    {
        std::vector<std::string> prog = {"add r1, r2, r3"};
        Processor p(2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::SMT,
                       prog, MakeProcessorConfiguration(2));

        runUntilEnd(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("SMT: T0[0] issue == 1", t0[0].issue_cycles.front() == 1);
        check("SMT: T1[0] issue == 1", t1[0].issue_cycles.front() == 1);
    }

    section("3.4 Multithreading: COARSE_GRAINED com ciclos de troca (switch_instructions)");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};
        // O vetor define após qual posição de instrução deve ocorrer a troca (ex: índice 0)
        std::vector<int> switch_instructions = {0};

        Processor p(2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::COARSE_GRAINED,
                       prog, MakeProcessorConfiguration(1), switch_instructions);

        runUntilEnd(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("Coarse-grained: T0 executou issue", t0[0].issue_cycles.front() > 0);
        check("Coarse-grained: T1 alternou e executou issue após switch", t1[0].issue_cycles.front() > 0);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. DEPENDÊNCIAS E HAZARDS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. DEPENDÊNCIAS E HAZARDS");

    section("4.1 RAW via Processor: add -> sub dependente");
    {
        std::vector<std::string> prog = {"add r3, r1, r2", "sub r5, r3, r4"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        int end_cycle = runUntilEnd(p);
        check("RAW: terminou", end_cycle != -1);

        auto tab = p.GetThreadTable(0);
        check("RAW: add issue == 1", tab[0].issue_cycles.front() == 1);
        check("RAW: add WR == 3",    tab[0].wr_cycle == 3);
        check("RAW: sub issue == 2", tab[1].issue_cycles.front() == 2);
        check("RAW: sub EX começa no ciclo 4 (após WR do add)",
              tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 4);
        check("RAW: sub WR == 5",    tab[1].wr_cycle == 5);
    }

    section("4.2 Disputa estrutural de FU: 2 instruções independentes competindo por 1 única FU");
    {
        // Duas instruções add competem pela única FU int_basic padrão.
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};

        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Disputa FU: add[0] inicia EX no ciclo 2", tab[0].ex_cycles[0] == 2);
        check("Disputa FU: add[1] serializa e inicia EX após a liberação da FU", tab[1].ex_cycles[0] > tab[0].ex_cycles[0]);
    }

    section("4.3 Conflito Estrutural no CDB/WR: write_result_width restringe escritas simultâneas");
    {
        // 2 instruções add independentes que terminariam EX no mesmo ciclo
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};
        const FUNCTIONAL_UNIT_CAPACITIES functional_units{1, 2, 1, 1, 1};

        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2, {}, functional_units, 1));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Conflito CDB: Ambos add[0] e add[1] terminam EX no ciclo 2",
                tab[0].ex_cycles.back() == 2 && tab[1].ex_cycles.back() == 2);
        check("Conflito CDB: Escritas (WR) foram serializadas (ciclos diferentes)",
                tab[0].wr_cycle != tab[1].wr_cycle);
    }

    section("4.4 Esgotamento de RS (Stall no Issue): Faltam slots de Reservation Station");
    {
        // Dispatch=2, mas vamos estrangular as RS de int_basic para apenas 1 slot.
        std::vector<std::string> prog = {"add r1, r2, r3", "add r4, r5, r6"};
        const RESERVATION_STATION_CAPACITIES reservation_stations{5, 5, 1, 5, 5, 5};

        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2, reservation_stations));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Stall RS: add[0] emitido no ciclo 1 (ocupou a única RS)", tab[0].issue_cycles.front() == 1);
        check("Stall RS: add[1] estolou e foi emitido depois (após a RS liberar)", tab[1].issue_cycles.front() > tab[0].issue_cycles.front());
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. BRANCH");

    section("5.1 BRANCH sem ROB e sem previsor: largura 2 para após o bnez");
    {
        std::vector<std::string> prog = {"bnez r1, fim", "add r2, r3, r4"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("BRANCH: bnez issue == 1",      tab[0].issue_cycles.front() == 1);
        check("BRANCH: bnez EX começa no ciclo 2", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("BRANCH: add não usa a segunda posição de dispatch do ciclo 1",
            tab[1].issue_cycles.front() == 2);
        check("BRANCH: add WR registrado",     tab[1].wr_cycle > 0);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. TOMASULO ESPECULATIVO (COM ROB)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. TOMASULO ESPECULATIVO (COM ROB)");

    section("6.1 Tomasulo Especulativo (com ROB): Thread::Commit() e Branches/Stores ordenados");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "s.d f2, 0(r1)"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        int end_cycle = runUntilEnd(p);
        check("Tomasulo Especulativo com Store: terminou", end_cycle != -1);

        auto tab = p.GetThreadTable(0);
        check("Especulativo: add commit_cycle válido", tab[0].commit_cycle > 0);
        check("Especulativo: STORE commit_cycle válido", tab[1].commit_cycle > 0);
        check("Especulativo: ordem de commit respeitada", tab[1].commit_cycle >= tab[0].commit_cycle);
    }

    section("6.2 Instrução do tipo STORE (store_with_rob): caminho sem WR normal");
    {
        std::vector<std::string> prog = {"add r1, r2, r3", "s.d f2, 0(r1)"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(1));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Store: WR cycle permanece inalterado (-1) com ROB", tab[1].wr_cycle == -1);
        check("Store: passou pelo commit corretamente", tab[1].commit_cycle > 0);
    }

    section("6.3 Branch com ROB sem previsor: largura 2 para após o bnez");
    {
        std::vector<std::string> prog = {"bnez r1, fim", "add r2, r3, r4"};
        Processor p(1, false,
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("ROB sem previsor: bnez issue == 1", tab[0].issue_cycles.front() == 1);
        check("ROB sem previsor: add aguarda o próximo ciclo de dispatch",
            tab[1].issue_cycles.front() == 2);
    }

    section("6.4 Branch Especulativo (has_predictor=true): Previsor evita stall no Issue");
    {
        // bnez e add podem entrar no ciclo 1 porque dispatch=2 e tem previsor+ROB
        std::vector<std::string> prog = {"bnez r1, fim", "add r2, r3, r4"};
        Processor p(1, true, // has_predictor = true
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog, MakeProcessorConfiguration(2));

        runUntilEnd(p);
        auto tab = p.GetThreadTable(0);

        check("Predictor: bnez issue == 1", tab[0].issue_cycles.front() == 1);
        check("Predictor: add issue não foi estolado (emitido ciclo 1)", tab[1].issue_cycles.front() == 1);
    }

    section("6.5 Processadores intercalados preservam configurações independentes");
    {
        const std::vector<std::string> prog{"add r1, r2, r3"};
        PIPELINE_CONFIGURATION fast{MakeProcessorConfiguration(1)};
        PIPELINE_CONFIGURATION slow{MakeProcessorConfiguration(1)};
        fast.execution_latencies.int_basic = 1;
        slow.execution_latencies.int_basic = 4;

        Processor fast_processor(
            1,
            false,
            PROCESSOR_TYPE::TOMASULO_CLASSIC,
            MULTITHREADING_MODEL::NONE,
            prog,
            fast
        );
        Processor slow_processor(
            1,
            false,
            PROCESSOR_TYPE::TOMASULO_CLASSIC,
            MULTITHREADING_MODEL::NONE,
            prog,
            slow
        );

        bool fast_done{false};
        bool slow_done{false};
        for (int cycle{}; cycle < 20 && (!fast_done || !slow_done); cycle++) {
            if (!fast_done) fast_done = fast_processor.ExecuteCycle();
            if (!slow_done) slow_done = slow_processor.ExecuteCycle();
        }

        const TABLE_ROW& fast_row{fast_processor.GetThreadTable(0)[0]};
        const TABLE_ROW& slow_row{slow_processor.GetThreadTable(0)[0]};
        check("latências permanecem isoladas durante execução intercalada",
            fast_done && slow_done &&
            fast_row.ex_cycles == std::vector<int>({2, 2}) &&
            slow_row.ex_cycles == std::vector<int>({2, 5}));
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
