// ────────────────────────────────────────────────────────────────────────────────────────────────
//  tb_Instrucao.cpp  —  Testbench isolado de Instrucao.cpp
//  Compile: g++ -o tb_Instrucao tb_Instrucao.cpp ../Componentes.cpp ../Instrucao.cpp
// ────────────────────────────────────────────────────────────────────────────────────────────────
#include "../headers/Instrucao.h"
#include "../headers/Componentes.h"
#include <iostream>
#include <string>

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
    secao("Instrucao() — construtor padrão");
    // ────────────────────────────────────────────────────────
    {
        Instrucao i;
        check("getPC() == -1",                         i.getPC() == -1);
        check("getTipoInstrucao() == NAO_EXISTE",       i.getTipoInstrucao() == TipoInstrucao::NAO_EXISTE);
        check("getLatenciaEX() == 0",                  i.getLatenciaEX() == 0);
        check("getLatenciaMEM() == 0",                 i.getLatenciaMEM() == 0);
    }

    // ────────────────────────────────────────────────────────
    secao("getPC() e getInstrucaoString()");
    // ────────────────────────────────────────────────────────
    {
        Instrucao i(7, "ADD R1, R2, R3");
        check("getPC() == 7",                          i.getPC() == 7);
        check("getInstrucaoString() == 'ADD R1, R2, R3'",
              i.getInstrucaoString() == "ADD R1, R2, R3");
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — LOAD");
    // ────────────────────────────────────────────────────────
    {
        // "L.D F2, 0(R1)"  →  tipo=LOAD, latEX=1, latMEM=1
        //                     dest=F2, J=vazio, K=R1
        Instrucao i(0, "L.D F2, 0(R1)");
        check("L.D: tipo == LOAD",           i.getTipoInstrucao() == TipoInstrucao::LOAD);
        check("L.D: latenciaEX  == 1",       i.getLatenciaEX()  == 1);
        check("L.D: latenciaMEM == 1",       i.getLatenciaMEM() == 1);
        check("L.D: dest tipo='F'",          i.getRegDestino().getTipo() == 'F');
        check("L.D: dest id=2",              i.getRegDestino().getId()   == 2);
        check("L.D: J tipo='Z' (sem J)",     i.getJ().getTipo() == 'Z');
        check("L.D: K tipo='R'",             i.getK().getTipo() == 'R');
        check("L.D: K id=1",                 i.getK().getId()   == 1);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — STORE");
    // ────────────────────────────────────────────────────────
    {
        // "S.D F6, 0(R2)"  →  tipo=STORE, latEX=1, latMEM=1
        //                     dest=vazio, J=F6, K=R2
        Instrucao i(1, "S.D F6, 0(R2)");
        check("S.D: tipo == STORE",          i.getTipoInstrucao() == TipoInstrucao::STORE);
        check("S.D: latenciaEX  == 1",       i.getLatenciaEX()  == 1);
        check("S.D: latenciaMEM == 1",       i.getLatenciaMEM() == 1);
        check("S.D: dest tipo='Z' (sem dest)", i.getRegDestino().getTipo() == 'Z');
        check("S.D: J tipo='F'",             i.getJ().getTipo() == 'F');
        check("S.D: J id=6",                 i.getJ().getId()   == 6);
        check("S.D: K tipo='R'",             i.getK().getTipo() == 'R');
        check("S.D: K id=2",                 i.getK().getId()   == 2);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — INT_BASICO");
    // ────────────────────────────────────────────────────────
    {
        // "DADDIU R1, R1, #8"  →  tipo=INT_BASICO, latEX=1
        //                         dest=R1, J=R1, K=inválido (#8 não começa com R/F)
        Instrucao i(2, "DADDIU R1, R1, #8");
        check("DADDIU: tipo == INT_BASICO",  i.getTipoInstrucao() == TipoInstrucao::INT_BASICO);
        check("DADDIU: latenciaEX == 1",     i.getLatenciaEX() == 1);
        check("DADDIU: dest tipo='R'",       i.getRegDestino().getTipo() == 'R');
        check("DADDIU: dest id=1",           i.getRegDestino().getId()   == 1);
        check("DADDIU: J tipo='R'",          i.getJ().getTipo() == 'R');
        check("DADDIU: J id=1",              i.getJ().getId()   == 1);
        // K é "#8" que não começa com R/F → tipo Z
        check("DADDIU: K tipo='Z' (imediato)", i.getK().getTipo() == 'Z');

        // ADD R3, R1, R2  →  dest=R3, J=R1, K=R2
        Instrucao add(3, "ADD R3, R1, R2");
        check("ADD: dest id=3",  add.getRegDestino().getId() == 3);
        check("ADD: J id=1",     add.getJ().getId()          == 1);
        check("ADD: K id=2",     add.getK().getId()          == 2);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — BRANCH");
    // ────────────────────────────────────────────────────────
    {
        // "BNEZ R3, foo"  →  tipo=BRANCH, latEX=1
        //                    dest=vazio, J=R3, K=vazio (label não é registrador)
        Instrucao i(4, "BNEZ R3, foo");
        check("BNEZ: tipo == BRANCH",        i.getTipoInstrucao() == TipoInstrucao::BRANCH);
        check("BNEZ: latenciaEX == 1",       i.getLatenciaEX() == 1);
        check("BNEZ: dest tipo='Z'",         i.getRegDestino().getTipo() == 'Z');
        check("BNEZ: J tipo='R'",            i.getJ().getTipo() == 'R');
        check("BNEZ: J id=3",                i.getJ().getId()   == 3);
        check("BNEZ: K tipo='Z' (label)",    i.getK().getTipo() == 'Z');

        // BEQ R1, R2, label  →  J=R1, K=R2
        Instrucao beq(5, "BEQ R1, R2, label");
        check("BEQ: J tipo='R'", beq.getJ().getTipo() == 'R');
        check("BEQ: J id=1",     beq.getJ().getId()   == 1);
        check("BEQ: K tipo='R'", beq.getK().getTipo() == 'R');
        check("BEQ: K id=2",     beq.getK().getId()   == 2);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — INT_MUL");
    // ────────────────────────────────────────────────────────
    {
        // "MUL R3, R1, R2"  →  tipo=INT_MUL, latEX=4
        Instrucao i(5, "MUL R3, R1, R2");
        check("MUL: tipo == INT_MUL",        i.getTipoInstrucao() == TipoInstrucao::INT_MUL);
        check("MUL: latenciaEX == 4",        i.getLatenciaEX() == 4);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — INT_DIV");
    // ────────────────────────────────────────────────────────
    {
        // "DIV R3, R1, R2"  →  tipo=INT_DIV, latEX=10
        Instrucao i(6, "DIV R3, R1, R2");
        check("DIV: tipo == INT_DIV",        i.getTipoInstrucao() == TipoInstrucao::INT_DIV);
        check("DIV: latenciaEX == 10",       i.getLatenciaEX() == 10);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — FLOAT_BASICO");
    // ────────────────────────────────────────────────────────
    {
        // "ADD.D F6, F4, F6"  →  tipo=FLOAT_BASICO, latEX=9
        Instrucao i(7, "ADD.D F6, F4, F6");
        check("ADD.D: tipo == FLOAT_BASICO", i.getTipoInstrucao() == TipoInstrucao::FLOAT_BASICO);
        check("ADD.D: latenciaEX == 9",      i.getLatenciaEX() == 9);
        check("ADD.D: dest tipo='F'",        i.getRegDestino().getTipo() == 'F');
        check("ADD.D: dest id=6",            i.getRegDestino().getId()   == 6);
        check("ADD.D: J tipo='F'",           i.getJ().getTipo() == 'F');
        check("ADD.D: J id=4",               i.getJ().getId()   == 4);
        check("ADD.D: K tipo='F'",           i.getK().getTipo() == 'F');
        check("ADD.D: K id=6",               i.getK().getId()   == 6);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — FLOAT_MUL");
    // ────────────────────────────────────────────────────────
    {
        // "MUL.D F4, F2, F0"  →  tipo=FLOAT_MUL, latEX=14
        Instrucao i(8, "MUL.D F4, F2, F0");
        check("MUL.D: tipo == FLOAT_MUL",    i.getTipoInstrucao() == TipoInstrucao::FLOAT_MUL);
        check("MUL.D: latenciaEX == 14",     i.getLatenciaEX() == 14);
        check("MUL.D: dest tipo='F'",        i.getRegDestino().getTipo() == 'F');
        check("MUL.D: dest id=4",            i.getRegDestino().getId()   == 4);
        check("MUL.D: J tipo='F'",           i.getJ().getTipo() == 'F');
        check("MUL.D: J id=2",               i.getJ().getId()   == 2);
        check("MUL.D: K tipo='F'",           i.getK().getTipo() == 'F');
        check("MUL.D: K id=0",               i.getK().getId()   == 0);
    }

    // ────────────────────────────────────────────────────────
    secao("Identificação de tipo e latências — FLOAT_DIV");
    // ────────────────────────────────────────────────────────
    {
        // "DIV.D F4, F2, F0"  →  tipo=FLOAT_DIV, latEX=40
        Instrucao i(9, "DIV.D F4, F2, F0");
        check("DIV.D: tipo == FLOAT_DIV",    i.getTipoInstrucao() == TipoInstrucao::FLOAT_DIV);
        check("DIV.D: latenciaEX == 40",     i.getLatenciaEX() == 40);
    }

    // ────────────────────────────────────────────────────────
    secao("Instrução desconhecida → NAO_EXISTE");
    // ────────────────────────────────────────────────────────
    {
        Instrucao i(10, "XPTO R1, R2, R3");
        check("XPTO: tipo == NAO_EXISTE",    i.getTipoInstrucao() == TipoInstrucao::NAO_EXISTE);
        check("XPTO: latenciaEX == 0",       i.getLatenciaEX() == 0);
    }

    // ────────────────────────────────────────────────────────
    secao("setLatenciaEX / setLatenciaMEM");
    // ────────────────────────────────────────────────────────
    {
        Instrucao i(11, "L.D F0, 0(R0)");
        check("antes: latEX == 1",  i.getLatenciaEX()  == 1);
        check("antes: latMEM == 1", i.getLatenciaMEM() == 1);
        i.setLatenciaEX(5);
        i.setLatenciaMEM(3);
        check("depois: latEX == 5",  i.getLatenciaEX()  == 5);
        check("depois: latMEM == 3", i.getLatenciaMEM() == 3);
    }

    // ────────────────────────────────────────────────────────
    secao("latencias_basicas_EX (vetor estático)");
    // ────────────────────────────────────────────────────────
    {
        // índices: NAO_EXISTE=0, LOAD=1, STORE=2, BRANCH=3,
        //          INT_BASICO=4, INT_MUL=5, INT_DIV=6,
        //          FLOAT_BASICO=7, FLOAT_MUL=8, FLOAT_DIV=9
        // valores: {0, 1, 1, 1, 1, 4, 10, 9, 14, 40}
        check("latEX[NAO_EXISTE]=0",     Instrucao::latencias_basicas_EX[0]  == 0);
        check("latEX[LOAD]=1",           Instrucao::latencias_basicas_EX[1]  == 1);
        check("latEX[INT_MUL]=4",        Instrucao::latencias_basicas_EX[5]  == 4);
        check("latEX[INT_DIV]=10",       Instrucao::latencias_basicas_EX[6]  == 10);
        check("latEX[FLOAT_BASICO]=9",   Instrucao::latencias_basicas_EX[7]  == 9);
        check("latEX[FLOAT_MUL]=14",     Instrucao::latencias_basicas_EX[8]  == 14);
        check("latEX[FLOAT_DIV]=40",     Instrucao::latencias_basicas_EX[9]  == 40);
        check("latMEM[LOAD]=1",          Instrucao::latencias_basicas_MEM[0] == 1);
        check("latMEM[STORE]=1",         Instrucao::latencias_basicas_MEM[1] == 1);
    }

    // ────────────────────────────────────────────────────────
    std::cout << "\n─────────────────────────────\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
