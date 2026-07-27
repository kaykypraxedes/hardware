// ──────────────────────────────────────────────────────────────────────────
//  tb_ReservationStations.cpp  —  Testbench isolado de RS
//  Compile: g++ -o tb_ReservationStations tb_ReservationStations.cpp ../Componentes.cpp ../Instrucao.cpp ../ReservationStations.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/ReservationStations.h"
#include "../headers/Components.h"
#include "../headers/Instruction.h"
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

static CDB makeCDB() {
    CDB c;
    for (int i = 0; i < 32; i++) {
        c.R.push_back(Register("R" + std::to_string(i)));
        c.F.push_back(Register("F" + std::to_string(i)));
    }
    return c;
}

static FUNCTIONAL_UNITS makeFU(int n = 2) {
    FUNCTIONAL_UNITS fu;
    for (int i = 0; i < n; i++) fu.memory_access.push_back(FU{});
    for (int i = 0; i < n; i++) fu.int_basic_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.int_mult_div_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.float_basic_alu.push_back(FU{});
    for (int i = 0; i < n; i++) fu.float_mult_div_alu.push_back(FU{});
    fu.wr = 2;
    return fu;
}

int main() {

    secao("ReservationStation() — construtor");
    {
        ReservationStation r("load0");
        check("GetId() == 'load0'",            r.GetId() == "load0");
        check("GetBusy() == false",            r.GetBusy() == false);
        check("GetCountdown() == -1",          r.GetCountdown() == -1);
        check("GetFUPosition() == -1",         r.GetFUPosition() == -1);
        check("GetQj() vazio",                 r.GetQj().empty());
        check("GetQk() vazio",                 r.GetQk().empty());
        check("GetTimes() vazio",              r.GetTimes().empty());
        check("GetInstructions() vazio",       r.GetInstructions().empty());
    }

    secao("AddIssue() — instrução sem dependências (ADD R3, R1, R2)");
    {
        ReservationStation rs("int0");
        CDB cdb = makeCDB();
        Instruction instr(0, "ADD R3, R1, R2");
        bool ok = rs.AddIssue(instr, cdb, 1);
        check("AddIssue() retorna true",       ok);
        check("GetBusy() == true",             rs.GetBusy());
        check("GetInstructionPhase() == ISSUE", rs.GetInstructionPhase() == INSTRUCTION_PHASE::ISSUE);
        check("GetQj() vazio (R1 livre)",      rs.GetQj().empty());
        check("GetQk() vazio (R2 livre)",      rs.GetQk().empty());
        check("GetInstructions()[0] == 'ADD R3, R1, R2'",
              rs.GetInstructions().size() == 1 && rs.GetInstructions()[0] == "ADD R3, R1, R2");
        check("GetTimes()[0] == 1 (ciclo de issue)",
              rs.GetTimes().size() == 1 && rs.GetTimes()[0] == 1);
        check("CDB.R[3].GetCurrentRS() == 'int0'", cdb.R[3].GetCurrentRS() == "int0");
        Instruction instr2(1, "SUB R5, R1, R2");
        bool dup = rs.AddIssue(instr2, cdb, 2);
        check("AddIssue em RS ocupada retorna false", !dup);
    }

    secao("AddIssue() — dependência em Qj (MUL.D F4, F2, F0 quando F2 pendente)");
    {
        ReservationStation rs("fmul0");
        CDB cdb = makeCDB();
        std::string prod = "load0";
        cdb.F[2].AllocateRS(prod, 1);

        Instruction instr(1, "MUL.D F4, F2, F0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load0' (F2 pendente)", rs.GetQj() == "load0");
        check("Qk vazio (F0 livre)",         rs.GetQk().empty());
        check("CDB.F[4] -> 'fmul0'",         cdb.F[4].GetCurrentRS() == "fmul0");
    }

    secao("AddIssue() — 'ADD R1, R1, R2' sem auto-dependência");
    {
        ReservationStation rs("int1");
        CDB cdb = makeCDB();
        Instruction instr(0, "ADD R1, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        check("Sem auto-dependência: Qj vazio (R1 estava livre)", rs.GetQj().empty());
        check("Sem auto-dependência: Qk vazio (R2 estava livre)", rs.GetQk().empty());
    }

    secao("UpdateDependencies() — instrução pronta (sem Qj/Qk)");
    {
        ReservationStation rs("int2");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);

        bool iniciou = rs.UpdateDependencies(cdb, fu, 2);
        check("UpdateDependencies retorna true (pronta)",  iniciou);
        check("fase == EX após UpdateDependencies",        rs.GetInstructionPhase() == INSTRUCTION_PHASE::EX);
        check("GetCountdown() == exLat == 1",              rs.GetCountdown() == 1);
        check("GetFUPosition() >= 0 (FU alocada)",         rs.GetFUPosition() >= 0);

        bool segunda = rs.UpdateDependencies(cdb, fu, 2);
        check("Segunda chamada retorna false (já em EX)",     !segunda);
    }

    secao("UpdateDependencies() — aguarda Qj ser liberado");
    {
        ReservationStation rs("fmul1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        std::string prod = "load0";
        cdb.F[2].AllocateRS(prod, 1);

        Instruction instr(1, "MUL.D F4, F2, F0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load0' antes de liberar", rs.GetQj() == "load0");

        bool antes = rs.UpdateDependencies(cdb, fu, 3);
        check("UpdateDependencies retorna false com Qj pendente", !antes);

        cdb.F[2].DeallocateRS("load0", 1, 3);
        bool depois = rs.UpdateDependencies(cdb, fu, 4);
        check("UpdateDependencies retorna true após Qj liberado", depois);
        check("fase == EX após Qj liberado", rs.GetInstructionPhase() == INSTRUCTION_PHASE::EX);
    }

    secao("UpdateCountdown() — INT_BASIC (exLat=1)");
    {
        ReservationStation rs("int3");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);

        bool terminou = rs.UpdateCountdown(fu, 2);
        check("UpdateCountdown retorna true (EX terminou)", terminou);
        check("fase == WB após EX de 1 ciclo",              rs.GetInstructionPhase() == INSTRUCTION_PHASE::WB);
        check("GetFUPosition() == -1 (FU liberada)",        rs.GetFUPosition() == -1);
    }

    secao("UpdateCountdown() — LOAD (EX->MEM->WB)");
    {
        ReservationStation rs("load0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "L.D F2, 0(R1)");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);

        bool fim_ex = rs.UpdateCountdown(fu, 2);
        check("LOAD: UpdateCountdown sinaliza fim do EX", fim_ex);
        check("LOAD: fase == MEM após EX",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE::MEM);
        check("LOAD: countdown == -1 (aguarda iniciar MEM)", rs.GetCountdown() == -1);

        bool iniciou_mem = rs.UpdateDependencies(cdb, fu, 3);
        check("LOAD: UpdateDependencies inicia MEM no ciclo 3", iniciou_mem);
        check("LOAD: countdown == latMEM == 1 após iniciar MEM", rs.GetCountdown() == 1);

        bool fim_mem = rs.UpdateCountdown(fu, 3);
        check("LOAD: UpdateCountdown sinaliza fim do MEM", fim_mem);
        check("LOAD: fase == WB após MEM",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE::WB);
    }

    secao("UpdateCountdown() — STORE (EX->espera dado->MEM->WB)");
    {
        ReservationStation rs("store0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        std::string prod = "float_basico0";
        cdb.F[6].AllocateRS(prod, 1);

        Instruction instr(0, "S.D F6, 0(R2)");
        rs.AddIssue(instr, cdb, 2);
        rs.UpdateDependencies(cdb, fu, 3);

        bool fim_ex = rs.UpdateCountdown(fu, 3);
        check("STORE: fim do EX sinalizado", fim_ex);
        check("STORE: countdown == -1 após EX (aguarda dado)", rs.GetCountdown() == -1);

        cdb.F[6].DeallocateRS("float_basico0", 1, 4);
        bool mem_ok = rs.UpdateDependencies(cdb, fu, 4);
        check("STORE: UpdateDependencies inicia MEM após dado liberado", mem_ok);
        check("STORE: fase == MEM", rs.GetInstructionPhase() == INSTRUCTION_PHASE::MEM);

        bool fim_mem = rs.UpdateCountdown(fu, 4);
        check("STORE: fim do MEM sinalizado", fim_mem);
        check("STORE: fase == WB", rs.GetInstructionPhase() == INSTRUCTION_PHASE::WB);
    }

    secao("Release()");
    {
        ReservationStation rs("int4");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);

        rs.Release(3);
        check("GetBusy() == false após Release",           !rs.GetBusy());
        check("GetCountdown() == -1",                      rs.GetCountdown() == -1);
        check("GetFUPosition() == -1",                     rs.GetFUPosition() == -1);
        check("GetQj() vazio",                              rs.GetQj().empty());
        check("GetQk() vazio",                              rs.GetQk().empty());
        check("GetTimes() tem 2 entradas após Release",    rs.GetTimes().size() == 2);
        check("GetTimes()[1] == 3 (ciclo de release)",     rs.GetTimes()[1] == 3);
    }

    secao("FU esgotada -> AddIssue ok mas UpdateDependencies retorna false");
    {
        FUNCTIONAL_UNITS fu;
        fu.int_basic_alu.push_back(FU{});
        fu.memory_access.push_back(FU{});
        fu.int_mult_div_alu.push_back(FU{});
        fu.float_basic_alu.push_back(FU{});
        fu.float_mult_div_alu.push_back(FU{});
        fu.wr = 1;

        CDB cdb = makeCDB();
        ReservationStation rs0("int0"), rs1("int1");
        Instruction i0(0, "ADD R3, R1, R2");
        Instruction i1(1, "SUB R5, R3, R4");
        rs0.AddIssue(i0, cdb, 1);
        rs1.AddIssue(i1, cdb, 1);

        rs0.UpdateDependencies(cdb, fu, 2);
        bool bloqueado = rs1.UpdateDependencies(cdb, fu, 2);
        check("Segunda RS bloqueada quando FU esgotada", !bloqueado);
        check("rs1 ainda em ISSUE",
              rs1.GetInstructionPhase() == INSTRUCTION_PHASE::ISSUE);
    }

    secao("Release() após LOAD — caminho de liberação por PC");
    {
        ReservationStation rs("load1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "L.D F4, 0(R2)");

        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);
        rs.UpdateDependencies(cdb, fu, 3);
        rs.UpdateCountdown(fu, 3);

        check("LOAD antes de Release: fase == WB",  rs.GetInstructionPhase() == INSTRUCTION_PHASE::WB);
        check("LOAD antes de Release: busy == true", rs.GetBusy());

        rs.Release(4);

        check("LOAD: busy == false após Release",        !rs.GetBusy());
        check("LOAD: countdown == -1 após Release",       rs.GetCountdown() == -1);
        check("LOAD: fuPosition == -1 após Release",       rs.GetFUPosition() == -1);
        check("LOAD: Qj vazio após Release",              rs.GetQj().empty());
        check("LOAD: Qk vazio após Release",              rs.GetQk().empty());
        check("LOAD: GetTimes().size() == 2",            rs.GetTimes().size() == 2);
        check("LOAD: GetTimes()[0] == 1 (issue)",        rs.GetTimes()[0] == 1);
        check("LOAD: GetTimes()[1] == 4 (release)",      rs.GetTimes()[1] == 4);
        Instruction instr2(1, "L.D F6, 0(R3)");
        bool reuso = rs.AddIssue(instr2, cdb, 5);
        check("LOAD: RS pode ser reusada após Release",  reuso);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
