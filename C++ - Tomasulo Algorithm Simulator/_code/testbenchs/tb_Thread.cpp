// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
//  tb_Thread.cpp  —  Testbench isolado de Thread.cpp
//  Compile: g++ -o tb_Thread tb_Thread.cpp ../Componentes.cpp ../Instrucao.cpp ../ReservationStations.cpp ../Thread.cpp
// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
#include "../headers/Thread.h"
#include "../headers/Instrucao.h"
#include "../headers/Componentes.h"
#include <iostream>
#include <string>
#include <vector>

static int passou = 0, falhou = 0;

static void check(const std::string& teste, bool condicao) {
    if (condicao) { std::cout << "  [OK]  " << teste << "\n"; passou++; }
    else          { std::cout << "  [FALHOU] " << teste << "\n"; falhou++; }
}
static void secao(const std::string& nome) {
    std::cout << "\n══ " << nome << " ══\n";
}

static const std::vector<int> NUM_RS_PADRAO  = {5,5,5,4,3,2};
static const std::vector<int> NUM_UFS_PADRAO = {1,1,1,1,1,2};
static const int             LARG_DESP_PADRAO = 1;

int main() {

    // ────────────────────────────────────────────────────────
    secao("Thread(assembly, false) — construtor sem ROB");
    // ────────────────────────────────────────────────────────
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3", "SUB R4, R1, R5"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        check("getPC() == 0",                             t.getPC() == 0);
        check("getNumStalls() == 0",                      t.getNumStalls() == 0);
        check("getEstadoThread() == LIBERADA",            t.getEstadoThread() == EstadoThread::LIBERADA);
        check("getTabela().size() == 2",                  t.getTabela().size() == 2);
        check("tabela[0].instrucao.getPC() == 0",         t.getTabela()[0].instrucao.getPC() == 0);
        check("tabela[1].instrucao.getPC() == 1",         t.getTabela()[1].instrucao.getPC() == 1);
        check("tabela[0].ciclo_issue == 0 (não emitido)", t.getTabela()[0].ciclo_issue == 0);

        // CDB inicializado com 32 registradores R e F
        check("getCDB().R.size() == 32", t.getCDB().R.size() == 32);
        check("getCDB().F.size() == 32", t.getCDB().F.size() == 32);

        // RS inicializadas
        check("getRS().load.size() == 5",          t.getRS().load.size() == 5);
        check("getRS().store.size() == 5",         t.getRS().store.size() == 5);
        check("getRS().int_basico.size() == 5",    t.getRS().int_basico.size() == 5);
        check("getRS().int_mult_div.size() == 4",  t.getRS().int_mult_div.size() == 4);
        check("getRS().float_basico.size() == 3",  t.getRS().float_basico.size() == 3);
        check("getRS().float_mult_div.size() == 2",t.getRS().float_mult_div.size() == 2);

        // UFs inicializadas
        check("getUF().acessar_memoria.size() == 1",   t.getUF().acessar_memoria.size() == 1);
        check("getUF().ula_int_basico.size() == 1",    t.getUF().ula_int_basico.size() == 1);
        check("getUF().wr == 2",                       t.getUF().wr == 2);
    }

    // ────────────────────────────────────────────────────────
    secao("Thread(assembly, false, instrucoes_troca) — construtor granulação grossa");
    // ────────────────────────────────────────────────────────
    {
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        std::vector<int> troca = {0};
        Thread t(troca, prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);
        check("construtor grossa: getPC() == 0",      t.getPC() == 0);
        check("construtor grossa: tabela.size() == 1", t.getTabela().size() == 1);
    }

    // ────────────────────────────────────────────────────────
    secao("definirLatenciaEspecifica()");
    // ────────────────────────────────────────────────────────
    {
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);
        // Latência padrão: EX=1, MEM=1
        check("antes: latEX == 1",  t.getTabela()[0].instrucao.getLatenciaEX()  == 1);
        check("antes: latMEM == 1", t.getTabela()[0].instrucao.getLatenciaMEM() == 1);
        t.definirLatenciaEspecifica(0, 3, 2);
        check("depois: latEX == 3", t.getTabela()[0].instrucao.getLatenciaEX()  == 3);
        check("depois: latMEM == 2",t.getTabela()[0].instrucao.getLatenciaMEM() == 2);
        // Sem latMEM (=0): só EX muda
        t.definirLatenciaEspecifica(0, 5, 0);
        check("latMEM não muda quando arg == 0", t.getTabela()[0].instrucao.getLatenciaMEM() == 2);
    }

    // ────────────────────────────────────────────────────────
    secao("Issue() — emissão simples sem dependência");
    // ────────────────────────────────────────────────────────
    {
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        // Ciclo 1: ADD emitido
        bool ok1 = t.Issue(1);
        check("ciclo 1: Issue retorna true",      ok1);
        check("ciclo 1: PC avança para 1",                t.getPC() == 1);
        check("ciclo 1: tabela[0].ciclo_issue == 1",      t.getTabela()[0].ciclo_issue == 1);
        check("ciclo 1: estado ainda LIBERADA",           t.getEstadoThread() == EstadoThread::LIBERADA);

        // Ciclo 2: SUB emitido (depende de R3, mas Tomasulo permite emitir)
        bool ok2 = t.Issue(2);
        check("ciclo 2: Issue retorna true",      ok2);
        check("ciclo 2: PC avança para 2",                t.getPC() == 2);
        check("ciclo 2: tabela[1].ciclo_issue == 2",      t.getTabela()[1].ciclo_issue == 2);

        // Ciclo 3: PC >= tamanho → false
        bool ok3 = t.Issue(3);
        check("ciclo 3: retorna false (sem mais instruções)", !ok3);
    }

    // ────────────────────────────────────────────────────────
    secao("Issue() — BRANCH (sem ROB): thread NÃO fica BLOQUEADA");
    // ────────────────────────────────────────────────────────
    {
        // No design atual, o bloqueio de despacho pós-branch é responsabilidade
        // do Processador (que para de chamar Issue dessa thread após um
        // BRANCH). A Thread em si não muda para BLOQUEADA ao emitir um BRANCH:
        // apenas armazena pc_branch_nao_resolvido para impedir que instruções
        // posteriores iniciem EX antes do branch ser resolvido.
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO); // sem ROB

        t.Issue(1); // emite BNEZ
        check("estado ainda LIBERADA após BNEZ (controle de despacho é do Processador)",
              t.getEstadoThread() == EstadoThread::LIBERADA);
        check("PC avança para 1 após issue do BNEZ", t.getPC() == 1);

        // A Thread permitiria emitir ADD (o Processador é quem impede isso fora),
        // mas após ExMem/Wr o branch resolver, o estado fica ESPERA e
        // depois LIBERADA — testado na seção "ESPERA" abaixo.
    }

    // ────────────────────────────────────────────────────────
    secao("Issue() — RS cheia bloqueia emissão");
    // ────────────────────────────────────────────────────────
    {
        // 5 LOADs enchem todas as RS de load (5 slots)
        std::vector<std::string> prog = {
            "L.D F0, 0(R0)", "L.D F1, 0(R1)", "L.D F2, 0(R2)",
            "L.D F3, 0(R3)", "L.D F4, 0(R4)", "L.D F5, 0(R5)" // 6ª não cabe
        };
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);
        for (int c = 1; c <= 5; c++) t.Issue(c);
        check("PC == 5 após 5 LOADs",   t.getPC() == 5);
        bool cheio = t.Issue(6);
        check("6º LOAD retorna false (RS cheia)", !cheio);
        check("PC não avança",                     t.getPC() == 5);
    }

    // ────────────────────────────────────────────────────────
    secao("ExMem() — retorna true quando tudo finalizado");
    // ────────────────────────────────────────────────────────
    {
        // Programa vazio: instrucoes_finalizadas == tabela.size() desde o início
        // Mas tabela nunca fica com num_instrucoes_finalizadas = 0 por padrão,
        // então usamos um programa de 1 instrução e executamos até o fim
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);
        // Nenhuma instrução finalizada ainda → retorna false
        bool ret = t.ExMem(1);
        check("ExMem retorna false com instruções pendentes", !ret);
    }

    // ────────────────────────────────────────────────────────
    secao("Ciclo completo: ADD R3,R1,R2 — issue→EX→WR em 3 ciclos");
    // ────────────────────────────────────────────────────────
    {
        // ADD tem latEX=1. Fluxo esperado:
        //   ciclo 1: issue
        //   ciclo 2: atualizarDependencias → inicia EX; atualizaContagem → EX termina, WB
        //   ciclo 3: consumirBufferWB → ciclo_WR=3, RS liberada
        std::vector<std::string> prog = {"ADD R3, R1, R2"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        t.Issue(1);
        check("ciclo 1: issue registrado", t.getTabela()[0].ciclo_issue == 1);

        t.ExMem(2);
        t.Wr(2);
        // EX deve ter sido iniciado e fechado no ciclo 2
        check("ciclo 2: ciclo_EX tem 2 entradas [inicio,fim]",
              t.getTabela()[0].ciclo_EX.size() == 2);
        check("ciclo 2: ciclo_EX[0] == 2 (início)",
              t.getTabela()[0].ciclo_EX[0] == 2);
        check("ciclo 2: ciclo_EX[1] == 2 (fim, latência 1)",
              t.getTabela()[0].ciclo_EX[1] == 2);

        t.ExMem(3);
        t.Wr(3);
        check("ciclo 3: ciclo_WR == 3", t.getTabela()[0].ciclo_WR == 3);
    }

    // ────────────────────────────────────────────────────────
    secao("Ciclo completo: L.D F2,0(R1) — issue→EX→MEM→WR");
    // ────────────────────────────────────────────────────────
    {
        // LOAD: latEX=1, latMEM=1
        // ciclo 1: issue
        // ciclo 2: inicia EX; EX termina (contagem 1)
        // ciclo 3: inicia MEM; MEM termina (contagem 1) → WB
        // ciclo 4: consumirBufferWB → ciclo_WR=4
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        t.Issue(1);
        check("LOAD: issue == 1",           t.getTabela()[0].ciclo_issue == 1);
        std::cout << "Ciclo Issue: " <<     t.getTabela()[0].ciclo_issue << '\n';
        t.ExMem(2);
        check("LOAD: EX[0] == 2 (início)",  t.getTabela()[0].ciclo_EX.size() >= 1 && t.getTabela()[0].ciclo_EX[0] == 2);
        std::cout << "Ciclo EX (inicio): " << t.getTabela()[0].ciclo_EX.size() << " - " << t.getTabela()[0].ciclo_EX[0] << '\n';
        t.Wr(2);
        check("LOAD: EX[1] == 2 (fim)",     t.getTabela()[0].ciclo_EX.size() == 2 && t.getTabela()[0].ciclo_EX[1] == 2);
        std::cout << "Ciclo EX (fim): " << t.getTabela()[0].ciclo_EX.size() << " - " << t.getTabela()[0].ciclo_EX[1] << '\n';
        t.ExMem(3);
        check("LOAD: MEM[0] == 3 (início)", t.getTabela()[0].ciclo_MEM.size() >= 1 && t.getTabela()[0].ciclo_MEM[0] == 3);
        std::cout << "Ciclo MEM (inicio): " << t.getTabela()[0].ciclo_MEM.size() << " - " << t.getTabela()[0].ciclo_MEM[0] << '\n';
        t.Wr(3);
        check("LOAD: MEM[1] == 3 (fim)",    t.getTabela()[0].ciclo_MEM.size() == 2 && t.getTabela()[0].ciclo_MEM[1] == 3);
        std::cout << "Ciclo MEM (fim): " << t.getTabela()[0].ciclo_MEM.size() << " - " << t.getTabela()[0].ciclo_MEM[1] << '\n';
        bool aux{t.ExMem(4)};
        std::cout << (aux ? "Bloqueou execução." : "Executou mais uma vez.") << '\n';
        t.Wr(4);
        check("LOAD: WR == 4",              t.getTabela()[0].ciclo_WR == 4);
        std::cout << "Ciclo WR: " << t.getTabela()[0].ciclo_WR << '\n';
        aux = t.ExMem(5);
        std::cout << (aux ? "Bloqueou execução." : "Executou mais uma vez.") << '\n';
    }

    // ────────────────────────────────────────────────────────
    secao("ESPERA: thread desbloqueia após WR do BRANCH");
    // ────────────────────────────────────────────────────────
    {
        // BNEZ latEX=1
        // ciclo 1: issue → estado LIBERADA (não BLOQUEADA)
        // ciclo 2: EX termina → entra no buffer_WB → estado = ESPERA
        // ciclo 3: ExMem percebe ESPERA → vira LIBERADA
        std::vector<std::string> prog = {"BNEZ R1, foo", "ADD R2, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        t.Issue(1);
        check("após issue BNEZ: estado LIBERADA (bloqueio é responsabilidade do Processador)",
              t.getEstadoThread() == EstadoThread::LIBERADA);

        t.ExMem(2); t.Wr(2);
        check("após WR do BNEZ: ESPERA",    t.getEstadoThread() == EstadoThread::ESPERA);

        t.ExMem(3);
        check("após ExMem: LIBERADA", t.getEstadoThread() == EstadoThread::LIBERADA);

        // Agora ADD pode ser emitido
        bool ok = t.Issue(3);
        check("ADD pode ser emitido após desbloqueio", ok);
    }

    // ────────────────────────────────────────────────────────
    secao("RAW: SUB espera ADD terminar antes de executar");
    // ────────────────────────────────────────────────────────
    {
        // ADD R3, R1, R2  (latEX=1) produz R3
        // SUB R5, R3, R4  depende de R3 → Qj deve ficar pendente até ADD concluir WR
        //
        // Fluxo real do Tomasulo (resultado disponível no ciclo APÓS o WR):
        //   ciclo 1: issue ADD → CDB.R3 = "int_basico0"
        //   ciclo 2: issue SUB → Qj = "int_basico0"; ADD: EX começa e termina
        //   ciclo 3: ADD: WR → CDB.R3 liberado  |  SUB: atualizarDependencias
        //            ainda vê CDB ocupado (ExMem roda antes do Wr no mesmo ciclo)
        //   ciclo 4: SUB: atualizarDependencias vê CDB livre → EX começa e termina
        //   ciclo 5: SUB: WR
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        t.Issue(1);                         // issue ADD
        t.Issue(2);                         // issue SUB (Qj pendente)
        t.ExMem(2); t.Wr(2);        // ADD: EX começa e termina
        t.ExMem(3); t.Wr(3);        // ADD: WR libera CDB.R3
        t.ExMem(4); t.Wr(4);        // SUB: EX começa e termina (Qj resolvido)
        t.ExMem(5); t.Wr(5);        // SUB: WR

        auto tab = t.getTabela();
        check("RAW: ADD issue == 1",          tab[0].ciclo_issue == 1);
        check("RAW: ADD EX[0] == 2 (início)", tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("RAW: ADD EX[1] == 2 (fim)",    tab[0].ciclo_EX.size() == 2 && tab[0].ciclo_EX[1] == 2);
        check("RAW: ADD WR == 3",             tab[0].ciclo_WR == 3);

        check("RAW: SUB issue == 2",          tab[1].ciclo_issue == 2);
        // SUB só vê CDB.R3 livre no ciclo 4 (ExMem roda antes do Wr no mesmo ciclo)
        check("RAW: SUB EX[0] == 4 (início, após RAW)", tab[1].ciclo_EX.size() >= 1 && tab[1].ciclo_EX[0] == 4);
        check("RAW: SUB EX[1] == 4 (fim)",    tab[1].ciclo_EX.size() == 2 && tab[1].ciclo_EX[1] == 4);
        check("RAW: SUB WR == 5",             tab[1].ciclo_WR == 5);

        // Confirma que tudo finalizou (ExMem deve retornar true)
        bool tudo_feito = t.ExMem(6);
        check("RAW: ExMem retorna true após tudo concluído", tudo_feito);
    }

    // ────────────────────────────────────────────────────────
    secao("ExMem() retorna true após todas as instruções finalizadas");
    // ────────────────────────────────────────────────────────
    {
        // Condição de parada do simulador: retorna true quando
        // num_instrucoes_finalizadas == tabela.size()
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        // Antes de qualquer execução: false
        check("antes: false com instrução pendente", !t.ExMem(1));

        // Roda até o fim (ADD: issue c1, EX c2, WR c3)
        t.Issue(1);
        t.ExMem(2); t.Wr(2);
        t.ExMem(3); t.Wr(3);

        // Agora tudo finalizado: deve retornar true
        bool fim = t.ExMem(4);
        check("depois: true quando todas instruções finalizadas", fim);

        // Chamadas adicionais continuam retornando true (idempotente)
        check("chamada extra ainda retorna true", t.ExMem(5));
    }

    // ────────────────────────────────────────────────────────
    secao("Instrução multiciclo: MUL R3,R1,R2 (latEX=4)");
    // ────────────────────────────────────────────────────────
    {
        // latEX=4: EX ocupa ciclos 2,3,4,5; WR no ciclo 6
        // Verifica que a contagem regressiva decrementa corretamente
        // sem encerrar cedo nem pular ciclos.
        //
        // Fluxo esperado:
        //   ciclo 1: issue
        //   ciclo 2: EX começa (contagem=4)
        //   ciclos 2,3,4,5: contagem decrementa 4→3→2→1→0
        //   ciclo 5: EX termina → WB buffer  (ciclo_EX = [2, 5])
        //   ciclo 6: WR                       (ciclo_WR = 6)
        std::vector<std::string> prog = {"MUL R3, R1, R2"};
        Thread t(prog, false, NUM_RS_PADRAO, NUM_UFS_PADRAO);

        t.Issue(1);
        for (int c = 2; c <= 6; c++) {
            t.ExMem(c);
            t.Wr(c);
        }

        auto tab = t.getTabela();
        check("MUL: issue == 1",           tab[0].ciclo_issue == 1);
        check("MUL: EX[0] == 2 (início)",  tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("MUL: EX[1] == 5 (fim)",     tab[0].ciclo_EX.size() == 2 && tab[0].ciclo_EX[1] == 5);
        check("MUL: WR == 6",              tab[0].ciclo_WR == 6);

        // Não deve ter ciclo_MEM (MUL não acessa memória)
        check("MUL: ciclo_MEM vazio",      tab[0].ciclo_MEM.empty());
    }

    // ────────────────────────────────────────────────────────
    std::cout << "\n─────────────────────────────\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
