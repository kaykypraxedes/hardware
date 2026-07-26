// ────────────────────────────────────────────────────────────────────────────
//  tb_Componentes.cpp  —  Testbench isolado de Componentes.cpp
//  Compile: g++ -o tb_Componentes tb_Componentes.cpp ../Componentes.cpp
// ────────────────────────────────────────────────────────────────────────────
#include "../headers/Componentes.h"
#include <iostream>
#include <string>
#include <vector>

// ── utilidades ──────────────────────────────────────────────
static int passou = 0, falhou = 0;

static void check(const std::string& teste, bool condicao) {
    if (condicao) { std::cout << "  [OK]  " << teste << "\n"; passou++; }
    else          { std::cout << "  [FALHOU] " << teste << "\n"; falhou++; }
}

static void secao(const std::string& nome) {
    std::cout << "\n══ " << nome << " ══\n";
}

int main() {
    // ────────────────────────────────────────────────────────
    secao("Registrador() — construtor padrão");
    // ────────────────────────────────────────────────────────
    {
        Registrador r;
        // tipo padrão é 'Z', id não definido, busy = false
        check("getTipo() == 'Z'",  r.getTipo() == 'Z');
        check("getBusy() == false", r.getBusy() == false);
        check("getRSatual() vazio", r.getRSatual().empty());
        check("getTempoAlocacao() vazio", r.getTempoAlocacao().empty());
        check("getRSalocadas() vazio",    r.getRSalocadas().empty());
    }

    // ────────────────────────────────────────────────────────
    secao("Registrador(string) — construtor com nome");
    // ────────────────────────────────────────────────────────
    {
        // Registrador inteiro R5
        Registrador r("R5");
        check("R5: getTipo() == 'R'", r.getTipo() == 'R');
        check("R5: getId()   == 5",   r.getId()   == 5);
        check("R5: getBusy() == false", r.getBusy() == false);

        // Registrador float F12
        Registrador f("F12");
        check("F12: getTipo() == 'F'", f.getTipo() == 'F');
        check("F12: getId()   == 12",  f.getId()   == 12);

        // Registrador de índice 0
        Registrador r0("R0");
        check("R0: getId() == 0", r0.getId() == 0);

        // String inválida → tipo 'Z', id -1
        Registrador inv("XYZ");
        check("XYZ: getTipo() == 'Z'", inv.getTipo() == 'Z');
        check("XYZ: getId()   == -1",  inv.getId()   == -1);

        // String vazia → tipo 'Z'
        Registrador vazio("");
        check("'': getTipo() == 'Z'", vazio.getTipo() == 'Z');
    }

    // ────────────────────────────────────────────────────────
    secao("trocaBusy()");
    // ────────────────────────────────────────────────────────
    {
        Registrador r("R1");
        check("antes: getBusy() == false", r.getBusy() == false);
        r.trocaBusy();
        check("depois 1 troca: getBusy() == true",  r.getBusy() == true);
        r.trocaBusy();
        check("depois 2 trocas: getBusy() == false", r.getBusy() == false);
    }

    // ────────────────────────────────────────────────────────
    secao("alocarRS(rs, inicio)");
    // ────────────────────────────────────────────────────────
    {
        Registrador r("F4");
        std::string rs1 = "load0";
        r.alocarRS(rs1, 3); // aloca no ciclo 3

        check("busy == true após alocarRS",         r.getBusy() == true);
        check("getRSatual() == 'load0'",             r.getRSatual() == "load0");
        check("getRSalocadas()[0] == 'load0'",       r.getRSalocadas().size() == 1
                                                  && r.getRSalocadas()[0] == "load0");
        // getTempoAlocacao() retorna pares {inicio, fim}; fim == -1 enquanto pendente.
        // Após 1 alocação sem desalocar: [{3, -1}] → size=2
        check("getTempoAlocacao().size() == 2 (par inicio/fim com fim pendente)",
              r.getTempoAlocacao().size() == 2);
        check("getTempoAlocacao()[0] == 3 (inicio)",
              r.getTempoAlocacao()[0] == 3);
        check("getTempoAlocacao()[1] == -1 (fim pendente)",
              r.getTempoAlocacao()[1] == -1);

        // Segunda alocação (instrução futura no mesmo registrador)
        std::string rs2 = "load1";
        r.alocarRS(rs2, 7);
        check("2ª alocação: getRSatual() == 'load1'",     r.getRSatual() == "load1");
        check("2ª alocação: getRSalocadas().size() == 2", r.getRSalocadas().size() == 2);
        // Dois pares pendentes: [{3,-1},{7,-1}] → size=4
        check("2ª alocação: getTempoAlocacao().size() == 4 (dois pares pendentes)",
              r.getTempoAlocacao().size() == 4);
    }

    // ────────────────────────────────────────────────────────
    secao("desalocarRS(rs_id, ciclo_inicio, ciclo_fim)");
    // ────────────────────────────────────────────────────────
    {
        Registrador r("R2");
        std::string rs = "int_basico0";
        r.alocarRS(rs, 5);              // início = 5
        r.desalocarRS("int_basico0", 5, 8); // fim = 8

        check("busy == false após desalocarRS",       r.getBusy() == false);
        check("getRSatual() vazio após desalocar",    r.getRSatual().empty());
        // tempos: [5, 8]
        check("tempos: [5, 8]",
              r.getTempoAlocacao().size() == 2
           && r.getTempoAlocacao()[0] == 5
           && r.getTempoAlocacao()[1] == 8);
        // RS alocadas ainda registra a entrada
        check("getRSalocadas() ainda contém 'int_basico0'",
              !r.getRSalocadas().empty() && r.getRSalocadas()[0] == "int_basico0");
    }

    // ────────────────────────────────────────────────────────
    secao("Ciclo completo: aloca → desaloca → aloca novamente");
    // ────────────────────────────────────────────────────────
    {
        // Simula F6 sendo escrito em ciclo 2 (load1) e depois em ciclo 10 (load2)
        Registrador f("F6");
        std::string rs_a = "load1";
        std::string rs_b = "load2";
        f.alocarRS(rs_a, 2);
        f.desalocarRS("load1", 2, 4);
        f.alocarRS(rs_b, 10);
        f.desalocarRS("load2", 10, 12);

        check("busy == false ao final",    f.getBusy() == false);
        check("2 RS alocadas no histórico", f.getRSalocadas().size() == 2);
        // tempos: [2, 4, 10, 12]
        auto t = f.getTempoAlocacao();
        check("tempos: [2,4,10,12]",
              t.size() == 4 && t[0]==2 && t[1]==4 && t[2]==10 && t[3]==12);
    }

    // ────────────────────────────────────────────────────────
    std::cout << "\n─────────────────────────────\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
