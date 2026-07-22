// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
//  tb_Processador.cpp  —  Testbench de integração de Processador
//  Compile: g++ -o tb_Processador tb_Processador.cpp ../Componentes.cpp ../Instrucao.cpp ../ReservationStations.cpp ../Thread.cpp ../Código/Processador.cpp
// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
#include "../headers/Processador.h"
#include "../headers/Instrucao.h"
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

// Roda o processador até o fim e retorna o ciclo em que parou
static int rodarAteOFim(Processador& p, int limite = 200) {
    int c = 0;
    while (c++ < limite)
        if (p.executarCiclo()) return c;
    return -1; // não terminou dentro do limite
}

int main() {

    // ────────────────────────────────────────────────────────
    secao("Construtor — 1 thread, TOMASULO_SEM_ROB, GRANULACAO_FINA");
    // ────────────────────────────────────────────────────────
    {
        // Assinatura: Processador(num_threads, largura_de_despacho, tem_previsor,
        //                          TipoProcessador, ModeloMultithreading, Assembly, ...)
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        check("getTabelaThread(0).size() == 1", p.getTabelaThread(0).size() == 1);
        check("instrução na posição 0 é ADD (INT_BASICO)",
              p.getTabelaThread(0)[0].instrucao.getTipoInstrucao() == TipoInstrucao::INT_BASICO);
    }

    // ────────────────────────────────────────────────────────
    secao("Ciclo único: ADD completa e executarCiclo() retorna true");
    // ────────────────────────────────────────────────────────
    {
        // ADD latEX=1: issue c1 → EX c2 → WR c3 → fim detectado c4
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("terminou (não atingiu limite)", ciclo_fim != -1);

        auto tab = p.getTabelaThread(0);
        check("issue == 1",          tab[0].ciclo_issue == 1);
        check("EX[0] == 2 (início)", tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("EX[1] == 2 (fim)",    tab[0].ciclo_EX.size() == 2 && tab[0].ciclo_EX[1] == 2);
        check("WR == 3",             tab[0].ciclo_WR == 3);
    }

    // ────────────────────────────────────────────────────────
    secao("LOAD completo: issue→EX→MEM→WR");
    // ────────────────────────────────────────────────────────
    {
        // L.D: latEX=1, latMEM=1
        // issue c1 → EX c2 → MEM c3 → WR c4
        std::vector<std::string> prog = {"L.D F2, 0(R1)"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("terminou", ciclo_fim != -1);

        auto tab = p.getTabelaThread(0);
        check("LOAD: issue == 1",           tab[0].ciclo_issue == 1);
        check("LOAD: EX[0] == 2 (início)",  tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("LOAD: EX[1] == 2 (fim)",     tab[0].ciclo_EX.size() == 2 && tab[0].ciclo_EX[1] == 2);
        check("LOAD: MEM[0] == 3 (início)", tab[0].ciclo_MEM.size() >= 1 && tab[0].ciclo_MEM[0] == 3);
        check("LOAD: MEM[1] == 3 (fim)",    tab[0].ciclo_MEM.size() == 2 && tab[0].ciclo_MEM[1] == 3);
        check("LOAD: WR == 4",              tab[0].ciclo_WR == 4);
    }

    // ────────────────────────────────────────────────────────
    secao("RAW via Processador: ADD → SUB dependente");
    // ────────────────────────────────────────────────────────
    {
        // ADD WR em c3; SUB vê CDB livre em c4, EX c4, WR c5.
        std::vector<std::string> prog = {"ADD R3, R1, R2", "SUB R5, R3, R4"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        int ciclo_fim = rodarAteOFim(p);
        check("RAW: terminou", ciclo_fim != -1);

        auto tab = p.getTabelaThread(0);
        check("RAW: ADD issue == 1", tab[0].ciclo_issue == 1);
        check("RAW: ADD WR == 3",    tab[0].ciclo_WR == 3);
        check("RAW: SUB issue == 2", tab[1].ciclo_issue == 2);
        check("RAW: SUB EX começa no ciclo 4 (após WR do ADD)",
              tab[1].ciclo_EX.size() >= 1 && tab[1].ciclo_EX[0] == 4);
        check("RAW: SUB WR == 5",    tab[1].ciclo_WR == 5);
    }

    // ────────────────────────────────────────────────────────
    secao("Superscalar (largura=2): 2 instruções independentes no ciclo 1");
    // ────────────────────────────────────────────────────────
    {
        // Com largura=2, instruções independentes são emitidas no mesmo ciclo 1.
        // Para concluir em paralelo (mesmo WR), são necessárias 2 ULAs int_basico
        // (caso contrário a segunda ADD espera a ULA liberar e termina 1 ciclo depois).
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        std::vector<int> num_rs  = {5,5,5,4,3,2};
        std::vector<int> num_ufs = {1,2,1,1,1,2}; // 2 ULAs int_basico
        Processador p(1, 2, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog, num_rs, num_ufs);

        rodarAteOFim(p);
        auto tab = p.getTabelaThread(0);

        check("Superscalar: ADD[0] issue == 1", tab[0].ciclo_issue == 1);
        check("Superscalar: ADD[1] issue == 1", tab[1].ciclo_issue == 1);
        check("Superscalar: ambas terminam no mesmo WR",
              tab[0].ciclo_WR == tab[1].ciclo_WR);
    }

    // ────────────────────────────────────────────────────────
    secao("2 threads, GRANULACAO_FINA, largura=1: thread mantém prioridade até esgotar");
    // ────────────────────────────────────────────────────────
    {
        // Com largura=1, o ponteiro de thread só avança quando o issue da
        // thread atual FALHA (Issue retorna false). Assim, T0 emite
        // todas as suas instruções primeiro (c1, c2); só então o ponteiro
        // avança para T1 (c3, c4).
        std::vector<std::string> prog = {"ADD R1, R2, R3", "ADD R4, R5, R6"};
        Processador p(2, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        rodarAteOFim(p);
        auto t0 = p.getTabelaThread(0);
        auto t1 = p.getTabelaThread(1);

        check("FINA: T0[0] issue == 1", t0[0].ciclo_issue == 1);
        check("FINA: T0[1] issue == 2", t0[1].ciclo_issue == 2);
        check("FINA: T1[0] issue == 3", t1[0].ciclo_issue == 3);
        check("FINA: T1[1] issue == 4", t1[1].ciclo_issue == 4);
    }

    // ────────────────────────────────────────────────────────
    secao("2 threads, SMT, largura=2: 1 issue por thread por ciclo");
    // ────────────────────────────────────────────────────────
    {
        // SMT com largura=2 e 2 threads: cada ciclo tenta emitir 1 instrução
        // de cada thread (round-robin dentro do mesmo ciclo).
        // Ambas as threads recebem issue no ciclo 1.
        std::vector<std::string> prog = {"ADD R1, R2, R3"};
        Processador p(2, 2, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::SMT,
                       prog);

        rodarAteOFim(p);
        auto t0 = p.getTabelaThread(0);
        auto t1 = p.getTabelaThread(1);

        check("SMT: T0[0] issue == 1", t0[0].ciclo_issue == 1);
        check("SMT: T1[0] issue == 1", t1[0].ciclo_issue == 1);
    }

    // ────────────────────────────────────────────────────────
    secao("BRANCH sem ROB e sem previsor: dispatch para após o BNEZ");
    // ────────────────────────────────────────────────────────
    {
        // Sem previsor, instruções após o branch não são despachadas no mesmo
        // ciclo do branch. BNEZ issue=1, EX=2; ADD só é emitido no ciclo 2.
        std::vector<std::string> prog = {"BNEZ R1, fim", "ADD R2, R3, R4"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        rodarAteOFim(p);
        auto tab = p.getTabelaThread(0);

        check("BRANCH: BNEZ issue == 1",      tab[0].ciclo_issue == 1);
        check("BRANCH: BNEZ EX começa no ciclo 2",
              tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("BRANCH: ADD emitido após BNEZ", tab[1].ciclo_issue > tab[0].ciclo_issue);
        check("BRANCH: ADD WR registrado",     tab[1].ciclo_WR > 0);
    }

    // ────────────────────────────────────────────────────────
    secao("MUL multiciclo via Processador (latEX=4)");
    // ────────────────────────────────────────────────────────
    {
        // MUL: issue c1, EX c2-c5, WR c6
        std::vector<std::string> prog = {"MUL R3, R1, R2"};
        Processador p(1, 1, false,
                       TipoProcessador::TOMASULO_SEM_ROB,
                       ModeloMultithreading::GRANULACAO_FINA,
                       prog);

        rodarAteOFim(p);
        auto tab = p.getTabelaThread(0);

        check("MUL: issue == 1",          tab[0].ciclo_issue == 1);
        check("MUL: EX[0] == 2 (início)", tab[0].ciclo_EX.size() >= 1 && tab[0].ciclo_EX[0] == 2);
        check("MUL: EX[1] == 5 (fim)",    tab[0].ciclo_EX.size() == 2 && tab[0].ciclo_EX[1] == 5);
        check("MUL: WR == 6",             tab[0].ciclo_WR == 6);
        check("MUL: ciclo_MEM vazio",     tab[0].ciclo_MEM.empty());
    }

    // ────────────────────────────────────────────────────────
    std::cout << "\n─────────────────────────────\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
