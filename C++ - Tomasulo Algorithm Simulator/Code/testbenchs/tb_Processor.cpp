// ──────────────────────────────────────────────────────────────────────────
//  tb_Processor.cpp  —  Testbench de integração de Processor.cpp
//  Compile: g++ -o tb_Processor tb_Processor.cpp ../Components.cpp ../Instruction.cpp ../ReservationStations.cpp ../Thread.cpp ../Processor.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Processor.h"
#include "../headers/Instruction.h"
#include "tb_helpers.h"
#include <vector>

using namespace processor;

static int rodarAteOFim(Processor& p, int limite = 200) {
    int c = 0;
    while (c++ < limite)
        if (p.ExecuteCycle()) return c;
    return -1;
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E CONFIGURAÇÃO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E CONFIGURAÇÃO");

    secao("1.1 Construtor — 1 thread, TOMASULO_CLASSIC");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        check("GetThreadTable(0).size() == 1", p.GetThreadTable(0).size() == 1);
        check("instrução na posição 0 é ADD (INT_BASIC)",
              p.GetThreadTable(0)[0].instruction.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
    }

    secao("1.2 Programa vazio — caso de borda trivial");
    {
        std::vector<std::string> prog = {};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("Programa vazio: encerra imediatamente no ciclo inicial", ciclo_fim == 1);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    secao("[ABORT] Construtor: mais de uma thread sem modelo de multithreading deve abortar");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        // model == NONE com num_threads > 1 → combinação inválida, espera abort().
        Processor p(2, 1, false,
                    PROCESSOR_TYPE::TOMASULO_CLASSIC,
                    MULTITHREADING_MODEL::NONE,
                    prog);
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. PIPELINE DE UMA INSTRUÇÃO (TRAÇOS COMPLETOS)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. PIPELINE DE UMA INSTRUÇÃO (TRAÇOS COMPLETOS)");

    secao("2.1 ADD — issue->EX->WR em 3 ciclos (ExecuteCycle() retorna true)");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("ADD: terminou (não atingiu limite)", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("ADD: issue == 1",                tab[0].issue_cycle == 1);
        check("ADD: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("ADD: ex_cycles[1] == 2 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("ADD: WR == 3",                    tab[0].wr_cycle == 3);
    }

    secao("2.2 LOAD completo — issue->EX->MEM->WR");
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("LOAD: terminou", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("LOAD: issue == 1",                  tab[0].issue_cycle == 1);
        check("LOAD: ex_cycles[0] == 2 (inicio)",  tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("LOAD: ex_cycles[1] == 2 (fim)",     tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("LOAD: mem_cycles[0] == 3 (inicio)", tab[0].mem_cycles.size() >= 1 && tab[0].mem_cycles[0] == 3);
        check("LOAD: mem_cycles[1] == 3 (fim)",    tab[0].mem_cycles.size() == 2 && tab[0].mem_cycles[1] == 3);
        check("LOAD: WR == 4",                     tab[0].wr_cycle == 4);
    }

    secao("2.3 MUL multiciclo via Processor (exLat=4)");
    {
        std::vector<std::string> prog = {"MUL R3, R1, R2"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("MUL: issue == 1",                 tab[0].issue_cycle == 1);
        check("MUL: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("MUL: ex_cycles[1] == 5 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 5);
        check("MUL: WR == 6",                    tab[0].wr_cycle == 6);
        check("MUL: mem_cycles vazio",           tab[0].mem_cycles.empty());
    }

    secao("2.4 Modificação customizada de Latência (new_latency via parâmetro)");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        // Tupla: <posição_instrucao, ex_latency, mem_latency>
        // Vamos forçar o ADD (que normalmente leva 1 ciclo) a levar 5 ciclos de EX
        std::vector<std::tuple<int,int,int>> lat = {{0, 5, 0}};

        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, {}, {}, {}, lat);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Latência Injetada: ADD levou exatos 5 ciclos em EX",
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

    secao("3.1 Superscalar (largura=2): 2 instruções independentes no ciclo 1");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        std::vector<int> num_rs  = {5,5,5,4,3,2};
        std::vector<int> num_fus = {1,2,1,1,1,2};
        Processor p(1, 2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, num_rs, num_fus);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Superscalar: ADD[0] issue == 1", tab[0].issue_cycle == 1);
        check("Superscalar: ADD[1] issue == 1", tab[1].issue_cycle == 1);
        check("Superscalar: ambas terminam no mesmo WR",
              tab[0].wr_cycle == tab[1].wr_cycle);
    }

    secao("3.2 2 threads, FINE_GRAINED, largura=1: thread mantém prioridade até esgotar");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        Processor p(2, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        rodarAteOFim(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("FINA: T0[0] issue == 1", t0[0].issue_cycle == 1);
        check("FINA: T0[1] issue == 2", t0[1].issue_cycle == 2);
        check("FINA: T1[0] issue == 3", t1[0].issue_cycle == 3);
        check("FINA: T1[1] issue == 4", t1[1].issue_cycle == 4);
    }

    secao("3.3 2 threads, SMT, largura=2: 1 issue por thread por ciclo");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(2, 2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::SMT,
                       prog);

        rodarAteOFim(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("SMT: T0[0] issue == 1", t0[0].issue_cycle == 1);
        check("SMT: T1[0] issue == 1", t1[0].issue_cycle == 1);
    }

    secao("3.4 Multithreading: COARSE_GRAINED com ciclos de troca (switch_instructions)");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        // O vetor define após qual posição de instrução deve ocorrer a troca (ex: índice 0)
        std::vector<int> switch_instructions = {0};

        Processor p(2, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::COARSE_GRAINED,
                       prog, {}, {}, switch_instructions);

        rodarAteOFim(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("Coarse-grained: T0 executou issue", t0[0].issue_cycle > 0);
        check("Coarse-grained: T1 alternou e executou issue após switch", t1[0].issue_cycle > 0);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. DEPENDÊNCIAS E HAZARDS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. DEPENDÊNCIAS E HAZARDS");

    secao("4.1 RAW via Processor: ADD -> SUB dependente");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("RAW: terminou", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("RAW: ADD issue == 1", tab[0].issue_cycle == 1);
        check("RAW: ADD WR == 3",    tab[0].wr_cycle == 3);
        check("RAW: SUB issue == 2", tab[1].issue_cycle == 2);
        check("RAW: SUB EX começa no ciclo 4 (após WR do ADD)",
              tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 4);
        check("RAW: SUB WR == 5",    tab[1].wr_cycle == 5);
    }

    secao("4.2 Disputa estrutural de FU: 2 instruções independentes competindo por 1 única FU");
    {
        // Duas instruções ADD (int_basic_alu) com num_fus default = 1 para int_basic
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        std::vector<int> num_fus = {1, 1, 1, 1, 1, 2}; // 1 FU para int_basic (índice 2)

        Processor p(1, 2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, {}, num_fus);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Disputa FU: ADD[0] inicia EX no ciclo 2", tab[0].ex_cycles[0] == 2);
        check("Disputa FU: ADD[1] serializa e inicia EX após a liberação da FU", tab[1].ex_cycles[0] > tab[0].ex_cycles[0]);
    }

    secao("4.3 Conflito Estrutural no CDB/WR: fu.wr restringe escritas simultâneas");
    {
        // 2 instruções ADD independentes que terminariam EX no mesmo ciclo
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        // num_fus: {mem=1, int_basic=2, int_mul=1, float=1, float_mul=1, wr=1}
        std::vector<int> num_fus = {1, 2, 1, 1, 1, 1}; // Note wr = 1 (índice 5)

        Processor p(1, 2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, {}, num_fus);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Conflito CDB: Ambos ADD[0] e ADD[1] terminam EX no ciclo 2",
                tab[0].ex_cycles.back() == 2 && tab[1].ex_cycles.back() == 2);
        check("Conflito CDB: Escritas (WR) foram serializadas (ciclos diferentes)",
                tab[0].wr_cycle != tab[1].wr_cycle);
    }

    secao("4.4 Esgotamento de RS (Stall no Issue): Faltam slots de Reservation Station");
    {
        // Dispatch=2, mas vamos estrangular as RS de int_basic para apenas 1 slot.
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        // num_rs: {load=5, store=5, int_basic=1, int_mul=5, float=5, float_mul=5}
        std::vector<int> num_rs = {5, 5, 1, 5, 5, 5};

        Processor p(1, 2, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog, num_rs);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Stall RS: ADD[0] emitido no ciclo 1 (ocupou a única RS)", tab[0].issue_cycle == 1);
        check("Stall RS: ADD[1] estolou e foi emitido depois (após a RS liberar)", tab[1].issue_cycle > tab[0].issue_cycle);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. BRANCH");

    secao("5.1 BRANCH sem ROB e sem previsor: dispatch para após o BNEZ");
    {
        std::vector<std::string> prog = {"BNEZ R1, fim", "ADD R2, R3, R4"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_CLASSIC,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("BRANCH: BNEZ issue == 1",      tab[0].issue_cycle == 1);
        check("BRANCH: BNEZ EX começa no ciclo 2", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("BRANCH: ADD emitido após BNEZ", tab[1].issue_cycle > tab[0].issue_cycle);
        check("BRANCH: ADD WR registrado",     tab[1].wr_cycle > 0);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. TOMASULO ESPECULATIVO (COM ROB)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. TOMASULO ESPECULATIVO (COM ROB)");

    secao("6.1 Tomasulo Especulativo (com ROB): Thread::Commit() e Branches/Stores ordenados");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "S.D F2, 0(R1)"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("Tomasulo Especulativo com Store: terminou", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("Especulativo: ADD commit_cycle válido", tab[0].commit_cycle > 0);
        check("Especulativo: STORE commit_cycle válido", tab[1].commit_cycle > 0);
        check("Especulativo: ordem de commit respeitada", tab[1].commit_cycle >= tab[0].commit_cycle);
    }

    secao("6.2 Instrução do tipo STORE (store_with_rob): caminho sem WR normal");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "S.D F2, 0(R1)"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Store: WR cycle permanece inalterado (-1) com ROB", tab[1].wr_cycle == -1);
        check("Store: passou pelo commit corretamente", tab[1].commit_cycle > 0);
    }

    secao("6.3 Branch Especulativo (has_predictor=true): Previsor evita stall no Issue");
    {
        // BNEZ e ADD podem entrar no ciclo 1 porque dispatch=2 e tem previsor+ROB
        std::vector<std::string> prog = {"BNEZ R1, fim", "ADD R2, R3, R4"};
        Processor p(1, 2, true, // has_predictor = true
                       PROCESSOR_TYPE::TOMASULO_ESPECULATIVE,
                       MULTITHREADING_MODEL::NONE,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Predictor: BNEZ issue == 1", tab[0].issue_cycle == 1);
        check("Predictor: ADD issue não foi estolado (emitido ciclo 1)", tab[1].issue_cycle == 1);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
