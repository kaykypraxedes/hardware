// ──────────────────────────────────────────────────────────────────────────
//  tb_Processor.cpp  —  Testbench de integração de Processor
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

    secao("Construtor — 1 thread, TOMASULO_WITHOUT_ROB, FINE_GRAINED");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        check("GetThreadTable(0).size() == 1", p.GetThreadTable(0).size() == 1);
        check("instrução na posição 0 é ADD (INT_BASIC)",
              p.GetThreadTable(0)[0].instruction.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
    }

    secao("Ciclo único: ADD completa e ExecuteCycle() retorna true");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("terminou (não atingiu limite)", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("issue == 1",          tab[0].issue_cycle == 1);
        check("ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("ex_cycles[1] == 2 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("WR == 3",             tab[0].wr_cycle == 3);
    }

    secao("LOAD completo: issue->EX->MEM->WR");
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("terminou", ciclo_fim != -1);

        auto tab = p.GetThreadTable(0);
        check("LOAD: issue == 1",           tab[0].issue_cycle == 1);
        check("LOAD: ex_cycles[0] == 2 (inicio)",  tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("LOAD: ex_cycles[1] == 2 (fim)",     tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("LOAD: mem_cycles[0] == 3 (inicio)", tab[0].mem_cycles.size() >= 1 && tab[0].mem_cycles[0] == 3);
        check("LOAD: mem_cycles[1] == 3 (fim)",    tab[0].mem_cycles.size() == 2 && tab[0].mem_cycles[1] == 3);
        check("LOAD: WR == 4",              tab[0].wr_cycle == 4);
    }

    secao("RAW via Processor: ADD -> SUB dependente");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
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

    secao("Superscalar (largura=2): 2 instruções independentes no ciclo 1");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        std::vector<int> num_rs  = {5,5,5,4,3,2};
        std::vector<int> num_fus = {1,2,1,1,1,2};
        Processor p(1, 2, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog, num_rs, num_fus);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("Superscalar: ADD[0] issue == 1", tab[0].issue_cycle == 1);
        check("Superscalar: ADD[1] issue == 1", tab[1].issue_cycle == 1);
        check("Superscalar: ambas terminam no mesmo WR",
              tab[0].wr_cycle == tab[1].wr_cycle);
    }

    secao("2 threads, FINE_GRAINED, largura=1: thread mantém prioridade até esgotar");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        Processor p(2, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
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

    secao("2 threads, SMT, largura=2: 1 issue por thread por ciclo");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processor p(2, 2, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::SMT,
                       prog);

        rodarAteOFim(p);
        auto t0 = p.GetThreadTable(0);
        auto t1 = p.GetThreadTable(1);

        check("SMT: T0[0] issue == 1", t0[0].issue_cycle == 1);
        check("SMT: T1[0] issue == 1", t1[0].issue_cycle == 1);
    }

    secao("BRANCH sem ROB e sem previsor: dispatch para após o BNEZ");
    {
        std::vector<std::string> prog = {"BNEZ R1, fim", "ADD R2, R3, R4"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("BRANCH: BNEZ issue == 1",      tab[0].issue_cycle == 1);
        check("BRANCH: BNEZ EX começa no ciclo 2",
              tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("BRANCH: ADD emitido após BNEZ", tab[1].issue_cycle > tab[0].issue_cycle);
        check("BRANCH: ADD WR registrado",     tab[1].wr_cycle > 0);
    }

    secao("MUL multiciclo via Processor (exLat=4)");
    {
        std::vector<std::string> prog = {"MUL R3, R1, R2"};
        Processor p(1, 1, false,
                       PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB,
                       MULTITHREADING_MODEL::FINE_GRAINED,
                       prog);

        rodarAteOFim(p);
        auto tab = p.GetThreadTable(0);

        check("MUL: issue == 1",          tab[0].issue_cycle == 1);
        check("MUL: ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("MUL: ex_cycles[1] == 5 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 5);
        check("MUL: WR == 6",             tab[0].wr_cycle == 6);
        check("MUL: mem_cycles vazio",    tab[0].mem_cycles.empty());
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
