// ──────────────────────────────────────────────────────────────────────────
//  tb_Thread.cpp  —  Testbench isolado de Thread.cpp
//  Compile: g++ -o tb_Thread tb_Thread.cpp ../Components.cpp ../Instruction.cpp ../ReservationStations.cpp ../Thread.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Thread.h"
#include "../headers/Instruction.h"
#include "../headers/Components.h"
#include "tb_helpers.h"
#include <vector>

using namespace processor;

static const std::vector<int> NUM_RS_PADRAO  = {5,5,5,4,3,2};
static const std::vector<int> NUM_FUS_PADRAO = {1,1,1,1,1,2};
static const int              LARG_DESP_PADRAO = 1;

int main() {

    secao("Thread(assembly, false) — construtor sem ROB");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "SUB R4, R1, R5"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        check("GetCurrentInstructionPosition() == 0",      t.GetCurrentInstructionPosition() == 0);
        check("GetNumStalls() == 0",                       t.GetNumStalls() == 0);
        check("GetTable().size() == 2",                    t.GetTable().size() == 2);
        check("tabela[0].instruction.GetPosition() == 0",  t.GetTable()[0].instruction.GetPosition() == 0);
        check("tabela[1].instruction.GetPosition() == 1",  t.GetTable()[1].instruction.GetPosition() == 1);
        check("tabela[0].issue_cycle == -1 (não emitido)", t.GetTable()[0].issue_cycle == -1);

        check("GetCDB().R.size() == 32",                   t.GetCDB().R.size() == 32);
        check("GetCDB().F.size() == 32",                   t.GetCDB().F.size() == 32);

        check("GetRS().load.size() == 5",                  t.GetRS().load.size() == 5);
        check("GetRS().store.size() == 5",                 t.GetRS().store.size() == 5);
        check("GetRS().int_basic.size() == 5",             t.GetRS().int_basic.size() == 5);
        check("GetRS().int_mult_div.size() == 4",          t.GetRS().int_mult_div.size() == 4);
        check("GetRS().float_basic.size() == 3",           t.GetRS().float_basic.size() == 3);
        check("GetRS().float_mult_div.size() == 2",        t.GetRS().float_mult_div.size() == 2);

        check("GetFU().memory_access.size() == 1",         t.GetFU().memory_access.size() == 1);
        check("GetFU().int_basic_alu.size() == 1",         t.GetFU().int_basic_alu.size() == 1);
        check("GetFU().wr == 2",                           t.GetFU().wr == 2);
    }

    secao("Thread(assembly, false, instrucoes_troca) — construtor granulação grossa");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        std::vector<int> troca = {0};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO, troca);
        check("construtor grossa: GetGetCurrentInstructionPosition() == 0",
            t.GetCurrentInstructionPosition() == 0);
        check("construtor grossa: tabela.size() == 1",
            t.GetTable().size() == 1);
    }

    secao("new_latency no construtor");
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};

        Thread t1(prog);  // sem new_latency
        check("exLat base == 1",  t1.GetTable()[0].instruction.GetExLatency()  == 1);
        check("memLat base == 1", t1.GetTable()[0].instruction.GetMemLatency() == 1);

        Thread t2(prog, {{0, 3, 2}});  // ex=3, mem=2
        check("exLat == 3",  t2.GetTable()[0].instruction.GetExLatency()  == 3);
        check("memLat == 2", t2.GetTable()[0].instruction.GetMemLatency() == 2);

        Thread t3(prog, {{0, 5, 0}});  // ex=5, mem=0 → não altera mem
        check("exLat == 5", t3.GetTable()[0].instruction.GetExLatency()  == 5);
        check("memLat continua 1 (base)", t3.GetTable()[0].instruction.GetMemLatency() == 1);
    }

    secao("Issue() — emissão simples sem dependência");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        bool ok1 = t.Issue(1);
        check("ciclo 1: Issue retorna true",              ok1);
        check("ciclo 1: Posição avança para 1",           t.GetCurrentInstructionPosition() == 1);
        check("ciclo 1: tabela[0].issue_cycle == 1",      t.GetTable()[0].issue_cycle == 1);

        bool ok2 = t.Issue(2);
        check("ciclo 2: Issue retorna true",              ok2);
        check("ciclo 2: Posição avança para 2",           t.GetCurrentInstructionPosition() == 2);
        check("ciclo 2: tabela[1].issue_cycle == 2",      t.GetTable()[1].issue_cycle == 2);

        bool ok3 = t.Issue(3);
        check("ciclo 3: retorna false (sem mais instruções)", !ok3);
    }

    secao("Issue() — BRANCH (sem ROB): thread NÃO fica BLOCKED");
    {
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("Posição avança para 1 após issue do BNEZ",
            t.GetCurrentInstructionPosition() == 1);
    }

    secao("Issue() — RS cheia bloqueia emissão");
    {
        std::vector<std::string> prog = {
            "L.D F0, 0(R0)", "L.D F1, 0(R1)", "L.D F2, 0(R2)",
            "L.D F3, 0(R3)", "L.D F4, 0(R4)", "L.D F5, 0(R5)"
        };
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        for (int c = 1; c <= 5; c++)       t.Issue(c);
        check("Posição == 5 após 5 LOADs", t.GetCurrentInstructionPosition() == 5);
        bool cheio =                       t.Issue(6);
        check("6o LOAD retorna false (RS cheia)", !cheio);
        check("Posição não avança",        t.GetCurrentInstructionPosition() == 5);
    }

    secao("ExMem() — retorna true quando tudo finalizado");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        bool ret = t.ExMem(1);
        check("ExMem retorna false com instruções pendentes", !ret);
    }

    secao("Ciclo completo: ADD R3,R1,R2 — issue->EX->WR em 3 ciclos");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("ciclo 1: issue registrado", t.GetTable()[0].issue_cycle == 1);

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

    secao("Ciclo completo: L.D F2,0(R1) — issue->EX->MEM->WR");
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("LOAD: issue == 1",
            t.GetTable()[0].issue_cycle == 1);
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
        check("LOAD: WR == 4",
            t.GetTable()[0].wr_cycle == 4);
        t.ExMem(5);
    }

    secao("WAITING: thread desbloqueia após WR do BRANCH");
    {
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        bool ok = t.Issue(3);
        check("ADD pode ser emitido após desbloqueio", ok);
    }

    secao("RAW: SUB espera ADD terminar antes de executar");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        t.Issue(2);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);
        t.ExMem(4); t.Wr(4);
        t.ExMem(5); t.Wr(5);

        auto tab = t.GetTable();
        check("RAW: ADD issue == 1",                           tab[0].issue_cycle == 1);
        check("RAW: ADD ex_cycles[0] == 2 (inicio)",           tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("RAW: ADD ex_cycles[1] == 2 (fim)",              tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("RAW: ADD WR == 3",                              tab[0].wr_cycle == 3);

        check("RAW: SUB issue == 2",                           tab[1].issue_cycle == 2);
        check("RAW: SUB ex_cycles[0] == 4 (inicio, após RAW)", tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 4);
        check("RAW: SUB ex_cycles[1] == 4 (fim)",              tab[1].ex_cycles.size() == 2 && tab[1].ex_cycles[1] == 4);
        check("RAW: SUB WR == 5",                              tab[1].wr_cycle == 5);

        bool tudo_feito = t.ExMem(6);
        check("RAW: ExMem retorna true após tudo concluído", tudo_feito);
    }

    secao("ExMem() retorna true após todas as instruções finalizadas");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        check("antes: false com instrução pendente", !t.ExMem(1));

        t.Issue(1);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);

        bool fim = t.ExMem(4);
        check("depois: true quando todas instruções finalizadas", fim);

        check("chamada extra ainda retorna true", t.ExMem(5));
    }

    secao("Instrução multiciclo: MUL R3,R1,R2 (exLat=4)");
    {
        std::vector<std::string> prog = {"MUL R3, R1, R2"};
        Thread t(prog, {}, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        for (int c = 2; c <= 6; c++) {
            t.ExMem(c);
            t.Wr(c);
        }

        auto tab = t.GetTable();
        check("MUL: issue == 1",           tab[0].issue_cycle == 1);
        check("MUL: ex_cycles[0] == 2 (inicio)",  tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("MUL: ex_cycles[1] == 5 (fim)",     tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 5);
        check("MUL: WR == 6",              tab[0].wr_cycle == 6);

        check("MUL: mem_cycles vazio",     tab[0].mem_cycles.empty());
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
