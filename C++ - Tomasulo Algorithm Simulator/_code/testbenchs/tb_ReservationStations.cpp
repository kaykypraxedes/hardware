// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
//  tb_ReservationStations.cpp  —  Testbench isolado de RS
//  Compile: g++ -o tb_ReservationStations tb_ReservationStations.cpp ../Componentes.cpp ../Instrucao.cpp ../ReservationStations.cpp
// ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
#include "../headers/ReservationStations.h"
#include "../headers/Componentes.h"
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

// ── helpers para montar CDB e UF mínimos ────────────────────
static CDB makeCDB() {
    CDB c;
    for (int i = 0; i < 32; i++) {
        c.R.push_back(Registrador("R" + std::to_string(i)));
        c.F.push_back(Registrador("F" + std::to_string(i)));
    }
    return c;
}

static UnidadesFuncionais makeUF(int n = 2) {
    UnidadesFuncionais uf;
    for (int i = 0; i < n; i++) uf.acessar_memoria.push_back(UF{false, i, "", {}, {}});
    for (int i = 0; i < n; i++) uf.ula_int_basico.push_back(UF{false, i, "", {}, {}});
    for (int i = 0; i < n; i++) uf.ula_int_mult_div.push_back(UF{false, i, "", {}, {}});
    for (int i = 0; i < n; i++) uf.ula_float_basico.push_back(UF{false, i, "", {}, {}});
    for (int i = 0; i < n; i++) uf.ula_float_mult_div.push_back(UF{false, i, "", {}, {}});
    uf.wr = 2;
    return uf;
}

int main() {

    // ────────────────────────────────────────────────────────
    secao("RS() — construtor");
    // ────────────────────────────────────────────────────────
    {
        RS r("load0");
        check("getId() == 'load0'",            r.getId() == "load0");
        check("getBusy() == false",            r.getBusy() == false);
        check("getContagemRegressiva() == -1", r.getContagemRegressiva() == -1);
        check("getPosicaoUF() == -1",          r.getPosicaoUF() == -1);
        check("getQj() vazio",                 r.getQj().empty());
        check("getQk() vazio",                 r.getQk().empty());
        check("getTempos() vazio",             r.getTempos().empty());
        check("getInstrucoes() vazio",         r.getInstrucoes().empty());
    }

    // ────────────────────────────────────────────────────────
    secao("addIssue() — instrução sem dependências (ADD R3, R1, R2)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("int0");
        CDB cdb = makeCDB();
        Instrucao instr(0, "ADD R3, R1, R2");
        // R1, R2 livres → Qj='', Qk=''
        // dest R3 deve ficar marcado no CDB após o issue
        bool ok = rs.addIssue(instr, cdb, 1);
        check("addIssue() retorna true",       ok);
        check("getBusy() == true",             rs.getBusy());
        check("getFaseInstrucao() == ISSUE",   rs.getFaseInstrucao() == FaseInstrucao::ISSUE);
        check("getQj() vazio (R1 livre)",      rs.getQj().empty());
        check("getQk() vazio (R2 livre)",      rs.getQk().empty());
        check("getInstrucoes()[0] == 'ADD R3, R1, R2'",
              rs.getInstrucoes().size() == 1 && rs.getInstrucoes()[0] == "ADD R3, R1, R2");
        check("getTempos()[0] == 1 (ciclo de issue)",
              rs.getTempos().size() == 1 && rs.getTempos()[0] == 1);
        // CDB deve marcar R3 → int0
        check("CDB.R[3].getRSatual() == 'int0'", cdb.R[3].getRSatual() == "int0");
        // addIssue numa RS já ocupada retorna false
        Instrucao instr2(1, "SUB R5, R1, R2");
        bool dup = rs.addIssue(instr2, cdb, 2);
        check("addIssue em RS ocupada retorna false", !dup);
    }

    // ────────────────────────────────────────────────────────
    secao("addIssue() — dependência em Qj (MUL.D F4, F2, F0 quando F2 pendente)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("fmul0");
        CDB cdb = makeCDB();
        // Simula load0 produzindo F2
        std::string prod = "load0";
        cdb.F[2].alocarRS(prod, 1);

        Instrucao instr(1, "MUL.D F4, F2, F0");
        rs.addIssue(instr, cdb, 2);
        // F2 está pendente em load0 → Qj = "load0"
        // F0 livre                  → Qk = ""
        check("Qj == 'load0' (F2 pendente)", rs.getQj() == "load0");
        check("Qk vazio (F0 livre)",         rs.getQk().empty());
        // CDB.F[4] deve apontar para fmul0
        check("CDB.F[4] → 'fmul0'",          cdb.F[4].getRSatual() == "fmul0");
    }

    // ────────────────────────────────────────────────────────
    secao("addIssue() — 'ADD R1, R1, R2' sem auto-dependência");
    // ────────────────────────────────────────────────────────
    {
        // Bug clássico: RS marca R1 no CDB e depois lê a própria tag como Qj.
        // Após o fix de addIssue, isso não deve acontecer.
        RS rs("int1");
        CDB cdb = makeCDB();
        Instrucao instr(0, "ADD R1, R1, R2");
        rs.addIssue(instr, cdb, 1);
        check("Sem auto-dependência: Qj vazio (R1 estava livre)", rs.getQj().empty());
        check("Sem auto-dependência: Qk vazio (R2 estava livre)", rs.getQk().empty());
    }

    // ────────────────────────────────────────────────────────
    secao("atualizarDependencias() — instrução pronta (sem Qj/Qk)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("int2");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        Instrucao instr(0, "ADD R3, R1, R2");
        rs.addIssue(instr, cdb, 1);

        // Qj e Qk já são '' → deve alocar UF e começar EX
        bool iniciou = rs.atualizarDependencias(cdb, uf, 2);
        check("atualizarDependencias retorna true (pronta)",  iniciou);
        check("fase == EX após atualizarDependencias",        rs.getFaseInstrucao() == FaseInstrucao::EX);
        check("getContagemRegressiva() == latEX == 1",        rs.getContagemRegressiva() == 1);
        check("getPosicaoUF() >= 0 (UF alocada)",             rs.getPosicaoUF() >= 0);

        // Segunda chamada no mesmo ciclo não deve reiniciar (contagem != -1)
        bool segunda = rs.atualizarDependencias(cdb, uf, 2);
        check("Segunda chamada retorna false (já em EX)",     !segunda);
    }

    // ────────────────────────────────────────────────────────
    secao("atualizarDependencias() — aguarda Qj ser liberado");
    // ────────────────────────────────────────────────────────
    {
        RS rs("fmul1");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        // F2 produzido por load0
        std::string prod = "load0";
        cdb.F[2].alocarRS(prod, 1);

        Instrucao instr(1, "MUL.D F4, F2, F0");
        rs.addIssue(instr, cdb, 2);
        check("Qj == 'load0' antes de liberar", rs.getQj() == "load0");

        // Antes de liberar: não executa
        bool antes = rs.atualizarDependencias(cdb, uf, 3);
        check("atualizarDependencias retorna false com Qj pendente", !antes);

        // load0 termina: CDB.F[2].getRSatual() fica vazio
        cdb.F[2].desalocarRS("load0", 1, 3);
        bool depois = rs.atualizarDependencias(cdb, uf, 4);
        check("atualizarDependencias retorna true após Qj liberado", depois);
        check("fase == EX após Qj liberado", rs.getFaseInstrucao() == FaseInstrucao::EX);
    }

    // ────────────────────────────────────────────────────────
    secao("atualizaContagem() — INT_BASICO (latEX=1)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("int3");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        Instrucao instr(0, "ADD R3, R1, R2"); // latEX=1
        rs.addIssue(instr, cdb, 1);
        rs.atualizarDependencias(cdb, uf, 2); // inicia EX, contagem=1

        // ciclo 2: contagem 1→0 → fase WB
        bool terminou = rs.atualizaContagem(uf, 2);
        check("atualizaContagem retorna true (EX terminou)", terminou);
        check("fase == WB após EX de 1 ciclo",               rs.getFaseInstrucao() == FaseInstrucao::WB);
        check("getPosicaoUF() == -1 (UF liberada)",          rs.getPosicaoUF() == -1);
    }

    // ────────────────────────────────────────────────────────
    secao("atualizaContagem() — LOAD (EX→MEM→WB)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("load0");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        Instrucao instr(0, "L.D F2, 0(R1)"); // latEX=1, latMEM=1
        rs.addIssue(instr, cdb, 1);
        rs.atualizarDependencias(cdb, uf, 2); // inicia EX

        // EX termina (contagem 1→0): fase muda para MEM, contagem volta a -1
        // (aguarda atualizarDependencias iniciar MEM no próximo ciclo)
        bool fim_ex = rs.atualizaContagem(uf, 2);
        check("LOAD: atualizaContagem sinaliza fim do EX", fim_ex);
        check("LOAD: fase == MEM após EX",                 rs.getFaseInstrucao() == FaseInstrucao::MEM);
        check("LOAD: contagem == -1 (aguarda iniciar MEM)", rs.getContagemRegressiva() == -1);

        // atualizarDependencias inicia MEM (K=R1 livre → sem Qk pendente)
        bool iniciou_mem = rs.atualizarDependencias(cdb, uf, 3);
        check("LOAD: atualizarDependencias inicia MEM no ciclo 3", iniciou_mem);
        check("LOAD: contagem == latMEM == 1 após iniciar MEM", rs.getContagemRegressiva() == 1);

        // MEM termina (contagem 1→0)
        bool fim_mem = rs.atualizaContagem(uf, 3);
        check("LOAD: atualizaContagem sinaliza fim do MEM", fim_mem);
        check("LOAD: fase == WB após MEM",                  rs.getFaseInstrucao() == FaseInstrucao::WB);
    }

    // ────────────────────────────────────────────────────────
    secao("atualizaContagem() — STORE (EX→espera dado→MEM→WB)");
    // ────────────────────────────────────────────────────────
    {
        RS rs("store0");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        // F6 pendente em float_basico0
        std::string prod = "float_basico0";
        cdb.F[6].alocarRS(prod, 1);

        Instrucao instr(0, "S.D F6, 0(R2)"); // latEX=1, latMEM=1
        rs.addIssue(instr, cdb, 2);
        // K=R2 livre → EX pode começar; J=F6 pendente → MEM aguarda
        rs.atualizarDependencias(cdb, uf, 3); // inicia EX

        // EX termina → contagem vai para -1 (aguarda Qj)
        bool fim_ex = rs.atualizaContagem(uf, 3);
        check("STORE: fim do EX sinalizado", fim_ex);
        check("STORE: contagem == -1 após EX (aguarda dado)", rs.getContagemRegressiva() == -1);

        // Libera F6
        cdb.F[6].desalocarRS("float_basico0", 1, 4);
        // atualizarDependencias agora permite iniciar MEM
        bool mem_ok = rs.atualizarDependencias(cdb, uf, 4);
        check("STORE: atualizarDependencias inicia MEM após dado liberado", mem_ok);
        check("STORE: fase == MEM", rs.getFaseInstrucao() == FaseInstrucao::MEM);

        // MEM termina
        bool fim_mem = rs.atualizaContagem(uf, 4);
        check("STORE: fim do MEM sinalizado", fim_mem);
        check("STORE: fase == WB", rs.getFaseInstrucao() == FaseInstrucao::WB);
    }

    // ────────────────────────────────────────────────────────
    secao("liberar()");
    // ────────────────────────────────────────────────────────
    {
        RS rs("int4");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        Instrucao instr(0, "ADD R3, R1, R2");
        rs.addIssue(instr, cdb, 1);
        rs.atualizarDependencias(cdb, uf, 2);
        rs.atualizaContagem(uf, 2); // chega em WB

        rs.liberar(3); // libera no ciclo 3
        check("getBusy() == false após liberar",           !rs.getBusy());
        check("getContagemRegressiva() == -1",              rs.getContagemRegressiva() == -1);
        check("getPosicaoUF() == -1",                       rs.getPosicaoUF() == -1);
        check("getQj() vazio",                              rs.getQj().empty());
        check("getQk() vazio",                              rs.getQk().empty());
        // getTempos() agora tem [ciclo_issue, ciclo_libera] = [1, 3]
        check("getTempos() tem 2 entradas após liberar",   rs.getTempos().size() == 2);
        check("getTempos()[1] == 3 (ciclo de liberação)",  rs.getTempos()[1] == 3);
    }

    // ────────────────────────────────────────────────────────
    secao("UF esgotada → addIssue ok mas atualizarDependencias retorna false");
    // ────────────────────────────────────────────────────────
    {
        // Com apenas 1 UF de int_basico, a segunda RS não consegue alocar
        UnidadesFuncionais uf;
        uf.ula_int_basico.push_back(UF{false, 0, "", {}, {}});
        uf.acessar_memoria.push_back(UF{false, 0, "", {}, {}});
        uf.ula_int_mult_div.push_back(UF{false, 0, "", {}, {}});
        uf.ula_float_basico.push_back(UF{false, 0, "", {}, {}});
        uf.ula_float_mult_div.push_back(UF{false, 0, "", {}, {}});
        uf.wr = 1;

        CDB cdb = makeCDB();
        RS rs0("int0"), rs1("int1");
        Instrucao i0(0, "ADD R3, R1, R2");
        Instrucao i1(1, "SUB R5, R3, R4");
        rs0.addIssue(i0, cdb, 1);
        rs1.addIssue(i1, cdb, 1);

        rs0.atualizarDependencias(cdb, uf, 2); // ocupa a única UF
        bool bloqueado = rs1.atualizarDependencias(cdb, uf, 2);
        check("Segunda RS bloqueada quando UF esgotada", !bloqueado);
        check("rs1 ainda em ISSUE",
              rs1.getFaseInstrucao() == FaseInstrucao::ISSUE);
    }

    // ────────────────────────────────────────────────────────
    secao("liberar() após LOAD — caminho de liberação por PC");
    // ────────────────────────────────────────────────────────
    {
        // LOAD chega a WB pelo caminho EX→MEM→WB.
        // Entre o fim do EX e o início do MEM é necessário chamar
        // atualizarDependencias() para que a fase MEM seja alocada.
        // Após liberar(), a RS deve estar limpa e pronta para reusar.
        RS rs("load1");
        CDB cdb = makeCDB();
        UnidadesFuncionais uf = makeUF();
        Instrucao instr(0, "L.D F4, 0(R2)"); // latEX=1, latMEM=1

        rs.addIssue(instr, cdb, 1);
        rs.atualizarDependencias(cdb, uf, 2); // inicia EX
        rs.atualizaContagem(uf, 2);           // EX termina → fase MEM, contagem=-1
        rs.atualizarDependencias(cdb, uf, 3); // inicia MEM
        rs.atualizaContagem(uf, 3);           // MEM termina → fase WB

        check("LOAD antes de liberar: fase == WB",  rs.getFaseInstrucao() == FaseInstrucao::WB);
        check("LOAD antes de liberar: busy == true", rs.getBusy());

        rs.liberar(4); // commit no ciclo 4

        check("LOAD: busy == false após liberar",        !rs.getBusy());
        check("LOAD: contagem == -1 após liberar",        rs.getContagemRegressiva() == -1);
        check("LOAD: posicaoUF == -1 após liberar",       rs.getPosicaoUF() == -1);
        check("LOAD: Qj vazio após liberar",              rs.getQj().empty());
        check("LOAD: Qk vazio após liberar",              rs.getQk().empty());
        // tempos: [ciclo_issue=1, ciclo_libera=4]
        check("LOAD: getTempos().size() == 2",            rs.getTempos().size() == 2);
        check("LOAD: getTempos()[0] == 1 (issue)",        rs.getTempos()[0] == 1);
        check("LOAD: getTempos()[1] == 4 (liberação)",    rs.getTempos()[1] == 4);
        // RS pode ser reusada imediatamente
        Instrucao instr2(1, "L.D F6, 0(R3)");
        bool reuso = rs.addIssue(instr2, cdb, 5);
        check("LOAD: RS pode ser reusada após liberar",   reuso);
    }

    // ────────────────────────────────────────────────────────
    std::cout << "\n─────────────────────────────\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
