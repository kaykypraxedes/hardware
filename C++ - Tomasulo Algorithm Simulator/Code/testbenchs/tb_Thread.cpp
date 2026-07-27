// ──────────────────────────────────────────────────────────────────────────
//  tb_Thread.cpp  —  Testbench isolado de Thread.cpp
//  Compile: g++ -o tb_Thread tb_Thread.cpp ../Componentes.cpp ../Instrucao.cpp ../ReservationStations.cpp ../Thread.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Thread.h"
#include "../headers/Instruction.h"
#include "../headers/Components.h"
#include "tb_helpers.h"
#include <vector>

using namespace processor;

static const std::vector<int> NUM_RS_PADRAO  = {5,5,5,4,3,2};
static const std::vector<int> NUM_FUS_PADRAO = {1,1,1,1,1,2};
static const int             LARG_DESP_PADRAO = 1;

int main() {

    secao("Thread(assembly, false) — construtor sem ROB");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "SUB R4, R1, R5"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        check("GetPC() == 0",                             t.GetPC() == 0);
        check("GetNumStalls() == 0",                      t.GetNumStalls() == 0);
        check("GetThreadState() == FREE",                 t.GetThreadState() == THREAD_STATE::FREE);
        check("GetTable().size() == 2",                   t.GetTable().size() == 2);
        check("tabela[0].instruction.GetPC() == 0",       t.GetTable()[0].instruction.GetPC() == 0);
        check("tabela[1].instruction.GetPC() == 1",       t.GetTable()[1].instruction.GetPC() == 1);
        check("tabela[0].issue_cycle == 0 (não emitido)", t.GetTable()[0].issue_cycle == 0);

        check("GetCDB().R.size() == 32", t.GetCDB().R.size() == 32);
        check("GetCDB().F.size() == 32", t.GetCDB().F.size() == 32);

        check("GetRS().load.size() == 5",          t.GetRS().load.size() == 5);
        check("GetRS().store.size() == 5",         t.GetRS().store.size() == 5);
        check("GetRS().int_basic.size() == 5",     t.GetRS().int_basic.size() == 5);
        check("GetRS().int_mult_div.size() == 4",  t.GetRS().int_mult_div.size() == 4);
        check("GetRS().float_basic.size() == 3",   t.GetRS().float_basic.size() == 3);
        check("GetRS().float_mult_div.size() == 2",t.GetRS().float_mult_div.size() == 2);

        check("GetFU().memory_access.size() == 1",   t.GetFU().memory_access.size() == 1);
        check("GetFU().int_basic_alu.size() == 1",   t.GetFU().int_basic_alu.size() == 1);
        check("GetFU().wr == 2",                    t.GetFU().wr == 2);
    }

    secao("Thread(assembly, false, instrucoes_troca) — construtor granulação grossa");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        std::vector<int> troca = {0};
        Thread t(troca, prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        check("construtor grossa: GetPC() == 0",      t.GetPC() == 0);
        check("construtor grossa: tabela.size() == 1", t.GetTable().size() == 1);
    }

    secao("SetCustomLatency()");
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        check("antes: exLat == 1",  t.GetTable()[0].instruction.GetExLatency()  == 1);
        check("antes: memLat == 1", t.GetTable()[0].instruction.GetMemLatency() == 1);
        t.SetCustomLatency(0, 3, 2);
        check("depois: exLat == 3", t.GetTable()[0].instruction.GetExLatency()  == 3);
        check("depois: memLat == 2",t.GetTable()[0].instruction.GetMemLatency() == 2);
        t.SetCustomLatency(0, 5, 0);
        check("memLat não muda quando arg == 0", t.GetTable()[0].instruction.GetMemLatency() == 2);
    }

    secao("Issue() — emissão simples sem dependência");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        bool ok1 = t.Issue(1);
        check("ciclo 1: Issue retorna true",      ok1);
        check("ciclo 1: PC avança para 1",                t.GetPC() == 1);
        check("ciclo 1: tabela[0].issue_cycle == 1",      t.GetTable()[0].issue_cycle == 1);
        check("ciclo 1: estado ainda FREE",                t.GetThreadState() == THREAD_STATE::FREE);

        bool ok2 = t.Issue(2);
        check("ciclo 2: Issue retorna true",      ok2);
        check("ciclo 2: PC avança para 2",                t.GetPC() == 2);
        check("ciclo 2: tabela[1].issue_cycle == 2",      t.GetTable()[1].issue_cycle == 2);

        bool ok3 = t.Issue(3);
        check("ciclo 3: retorna false (sem mais instruções)", !ok3);
    }

    secao("Issue() — BRANCH (sem ROB): thread NÃO fica BLOCKED");
    {
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("estado ainda FREE após BNEZ (controle de despacho é do Processador)",
              t.GetThreadState() == THREAD_STATE::FREE);
        check("PC avança para 1 após issue do BNEZ", t.GetPC() == 1);
    }

    secao("Issue() — RS cheia bloqueia emissão");
    {
        std::vector<std::string> prog = {
            "L.D F0, 0(R0)", "L.D F1, 0(R1)", "L.D F2, 0(R2)",
            "L.D F3, 0(R3)", "L.D F4, 0(R4)", "L.D F5, 0(R5)"
        };
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        for (int c = 1; c <= 5; c++) t.Issue(c);
        check("PC == 5 após 5 LOADs",   t.GetPC() == 5);
        bool cheio = t.Issue(6);
        check("6o LOAD retorna false (RS cheia)", !cheio);
        check("PC não avança",                     t.GetPC() == 5);
    }

    secao("ExMem() — retorna true quando tudo finalizado");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);
        bool ret = t.ExMem(1);
        check("ExMem retorna false com instruções pendentes", !ret);
    }

    secao("Ciclo completo: ADD R3,R1,R2 — issue->EX->WR em 3 ciclos");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

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
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("LOAD: issue == 1",           t.GetTable()[0].issue_cycle == 1);
        t.ExMem(2);
        check("LOAD: ex_cycles[0] == 2 (inicio)",  t.GetTable()[0].ex_cycles.size() >= 1 && t.GetTable()[0].ex_cycles[0] == 2);
        t.Wr(2);
        check("LOAD: ex_cycles[1] == 2 (fim)",     t.GetTable()[0].ex_cycles.size() == 2 && t.GetTable()[0].ex_cycles[1] == 2);
        t.ExMem(3);
        check("LOAD: mem_cycles[0] == 3 (inicio)", t.GetTable()[0].mem_cycles.size() >= 1 && t.GetTable()[0].mem_cycles[0] == 3);
        t.Wr(3);
        check("LOAD: mem_cycles[1] == 3 (fim)",    t.GetTable()[0].mem_cycles.size() == 2 && t.GetTable()[0].mem_cycles[1] == 3);
        t.ExMem(4);
        t.Wr(4);
        check("LOAD: WR == 4",              t.GetTable()[0].wr_cycle == 4);
        t.ExMem(5);
    }

    secao("WAITING: thread desbloqueia após WR do BRANCH");
    {
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        check("após issue BNEZ: estado FREE (bloqueio é responsabilidade do Processor)",
              t.GetThreadState() == THREAD_STATE::FREE);

        t.ExMem(2); t.Wr(2);
        check("após WR do BNEZ: WAITING",    t.GetThreadState() == THREAD_STATE::WAITING);

        t.ExMem(3);
        check("após ExMem: FREE", t.GetThreadState() == THREAD_STATE::FREE);

        bool ok = t.Issue(3);
        check("ADD pode ser emitido após desbloqueio", ok);
    }

    secao("RAW: SUB espera ADD terminar antes de executar");
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

        t.Issue(1);
        t.Issue(2);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);
        t.ExMem(4); t.Wr(4);
        t.ExMem(5); t.Wr(5);

        auto tab = t.GetTable();
        check("RAW: ADD issue == 1",          tab[0].issue_cycle == 1);
        check("RAW: ADD ex_cycles[0] == 2 (inicio)", tab[0].ex_cycles.size() >= 1 && tab[0].ex_cycles[0] == 2);
        check("RAW: ADD ex_cycles[1] == 2 (fim)",    tab[0].ex_cycles.size() == 2 && tab[0].ex_cycles[1] == 2);
        check("RAW: ADD WR == 3",             tab[0].wr_cycle == 3);

        check("RAW: SUB issue == 2",          tab[1].issue_cycle == 2);
        check("RAW: SUB ex_cycles[0] == 4 (inicio, após RAW)", tab[1].ex_cycles.size() >= 1 && tab[1].ex_cycles[0] == 4);
        check("RAW: SUB ex_cycles[1] == 4 (fim)",    tab[1].ex_cycles.size() == 2 && tab[1].ex_cycles[1] == 4);
        check("RAW: SUB WR == 5",             tab[1].wr_cycle == 5);

        bool tudo_feito = t.ExMem(6);
        check("RAW: ExMem retorna true após tudo concluído", tudo_feito);
    }

    secao("ExMem() retorna true após todas as instruções finalizadas");
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

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
        Thread t(prog, false, NUM_RS_PADRAO, NUM_FUS_PADRAO);

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
