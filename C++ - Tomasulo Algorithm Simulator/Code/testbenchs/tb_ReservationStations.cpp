// ──────────────────────────────────────────────────────────────────────────
//  tb_ReservationStations.cpp  —  Testbench isolado de ReservationStations.cpp
//  Compile: g++ -o tb_ReservationStations tb_ReservationStations.cpp ../Components.cpp ../Instruction.cpp ../ReservationStations.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/ReservationStations.h"
#include "../headers/Components.h"
#include "../headers/Instruction.h"
#include "tb_helpers.h"
#include <vector>

using namespace processor;
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

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO INICIAL
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO INICIAL");

    secao("1.1 ReservationStation() — construtor");
    {
        ReservationStation r("load0");
        check("GetId() == 'load0'",            r.GetId() == "load0");
        check("IsBusy() == false",             r.IsBusy() == false);
        check("GetCountdown() == -1",          r.GetCountdown() == -1);
        check("GetFUPosition() == -1",         r.GetFUPosition() == -1);
        check("GetQj() vazio",                 r.GetQj().empty());
        check("GetQk() vazio",                 r.GetQk().empty());
        check("GetTimes() vazio",              r.GetTimes().empty());
        check("GetInstructions() vazio",       r.GetInstructions().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 2. ISSUE — EMISSÃO NA RS (AddIssue)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. ISSUE — EMISSÃO NA RS (AddIssue)");

    secao("2.1 AddIssue() — instrução sem dependências (ADD R3, R1, R2)");
    {
        ReservationStation rs("int0");
        CDB cdb = makeCDB();
        Instruction instr(0, "ADD R3, R1, R2");
        bool ok = rs.AddIssue(instr, cdb, 1);
        check("AddIssue() retorna true",
            ok);
        check("IsBusy() == true",
            rs.IsBusy());
        check("GetInstructionPhase() == ISSUE",
            rs.GetInstructionPhase() == INSTRUCTION_PHASE::ISSUE);
        check("GetQj() vazio (R1 livre)",
            rs.GetQj().empty());
        check("GetQk() vazio (R2 livre)",
            rs.GetQk().empty());
        check("GetInstructions()[0] == 'ADD R3, R1, R2'",
              rs.GetInstructions().size() == 1 && rs.GetInstructions()[0] == "ADD R3, R1, R2");
        check("GetTimes()[0] == 1 (ciclo de issue)",
              rs.GetTimes().size() == 1 && rs.GetTimes()[0] == 1);
        check("CDB.R[3].GetCurrentRS() == 'int0'",
            cdb.R[3].GetCurrentRS() == "int0");
    }

    secao("2.2 AddIssue() — RS ocupada retorna false");
    {
        ReservationStation rs("int0");
        CDB cdb = makeCDB();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);

        Instruction instr2(1, "SUB R5, R1, R2");
        bool dup = rs.AddIssue(instr2, cdb, 2);
        check("AddIssue em RS ocupada retorna false", !dup);
    }

    secao("2.3 AddIssue() — dependência em Qj (MUL.D F4, F2, F0 quando F2 pendente)");
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

    secao("2.4 AddIssue() — 'ADD R1, R1, R2' sem auto-dependência");
    {
        ReservationStation rs("int1");
        CDB cdb = makeCDB();
        Instruction instr(0, "ADD R1, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        check("Sem auto-dependência: Qj vazio (R1 estava livre)", rs.GetQj().empty());
        check("Sem auto-dependência: Qk vazio (R2 estava livre)", rs.GetQk().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)");

    secao("3.1 UpdateDependencies() — instrução pronta (sem Qj/Qk) entra em EX");
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

    secao("3.2 UpdateDependencies() — aguarda Qj ser liberado");
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

    secao("3.3 STORE com endereço pronto (Qk) mas dado pendente (Qj) entra em EX");
    {
        ReservationStation rs("store1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        cdb.F[8].AllocateRS("fmul2", 1); // dado (F8) pendente; endereço (R1) livre

        Instruction instr(0, "S.D F8, 0(R1)");
        rs.AddIssue(instr, cdb, 2);
        check("Qk vazio (endereço livre)",        rs.GetQk().empty());
        check("Qj == 'fmul2' (dado pendente)",    rs.GetQj() == "fmul2");

        bool entrou_ex = rs.UpdateDependencies(cdb, fu, 3);
        check("STORE entra em EX mesmo com dado pendente", entrou_ex);

        rs.UpdateCountdown(fu, 3);
        check("fase == MEM aguardando dado",      rs.GetInstructionPhase() == INSTRUCTION_PHASE::MEM);

        bool bloqueado = rs.UpdateDependencies(cdb, fu, 4);
        check("MEM bloqueado enquanto Qj não resolve", !bloqueado);

        cdb.F[8].DeallocateRS("fmul2", 1, 5);
        bool mem_ok = rs.UpdateDependencies(cdb, fu, 5);
        check("MEM inicia após dado resolvido",   mem_ok);
    }

    secao("3.4 STORE com endereço pendente (Qk) NÃO entra em EX");
    {
        ReservationStation rs("store2");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        cdb.R[9].AllocateRS("int5", 1); // endereço pendente

        Instruction instr(0, "S.D F0, 0(R9)");
        rs.AddIssue(instr, cdb, 2);
        check("Qk == 'int5' (endereço pendente)", rs.GetQk() == "int5");

        bool bloqueado = rs.UpdateDependencies(cdb, fu, 3);
        check("STORE não entra em EX com endereço pendente", !bloqueado);
        check("fase permanece ISSUE",             rs.GetInstructionPhase() == INSTRUCTION_PHASE::ISSUE);
    }

    secao("3.5 ResolveDependency() — captura direta de Vj via broadcast simulado");
    {
        ReservationStation rs("fmul3");
        CDB cdb = makeCDB();
        cdb.F[2].AllocateRS("load2", 1);

        Instruction instr(0, "MUL.D F4, F2, F0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load2' antes do broadcast", rs.GetQj() == "load2");

        rs.ResolveDependency("load2", cdb.F[2]);
        check("Qj limpo após ResolveDependency",  rs.GetQj().empty());

        // rs_id que não bate não deve afetar nada
        ReservationStation rs2("fmul4");
        cdb.F[10].AllocateRS("load3", 1);
        Instruction instr2(1, "MUL.D F12, F10, F0");
        rs2.AddIssue(instr2, cdb, 2);
        rs2.ResolveDependency("outro_produtor_qualquer", cdb.F[10]);
        check("ResolveDependency com rs_id que não bate não altera Qj", rs2.GetQj() == "load3");
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    secao("[ABORT] TryAllocateFU com ex_latency == 0 (via SetExLatency)");
    {
        ReservationStation rs("int_abort0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        instr.SetExLatency(0);
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2); // deve abortar dentro de TryAllocateFU (EX)
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] TryAllocateFU com mem_latency == 0 (LOAD, via SetMemLatency)");
    {
        ReservationStation rs("load_abort0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "L.D F2, 0(R1)");
        instr.SetMemLatency(0);
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2); // EX normal (exLat de LOAD == 1)
        rs.UpdateCountdown(fu, 2);         // fase avança para MEM
        rs.UpdateDependencies(cdb, fu, 3); // deve abortar dentro de TryAllocateFU (MEM, latency 0)
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 4. CONTAGEM DE CICLOS — PROGRESSÃO DE FASES (UpdateCountdown)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. CONTAGEM DE CICLOS — PROGRESSÃO DE FASES (UpdateCountdown)");

    secao("4.1 UpdateCountdown() — INT_BASIC (exLat=1): EX -> WR");
    {
        ReservationStation rs("int3");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);

        bool terminou = rs.UpdateCountdown(fu, 2);
        check("UpdateCountdown retorna true (EX terminou)", terminou);
        check("fase == WR após EX de 1 ciclo",              rs.GetInstructionPhase() == INSTRUCTION_PHASE::WR);
        check("GetFUPosition() == -1 (FU liberada)",        rs.GetFUPosition() == -1);
    }

    secao("4.2 UpdateCountdown() — LOAD (EX->MEM->WR)");
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

        bool mem_end = rs.UpdateCountdown(fu, 3);
        check("LOAD: UpdateCountdown sinaliza fim do MEM", mem_end);
        check("LOAD: fase == WR após MEM",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE::WR);
    }

    secao("4.3 UpdateCountdown() — STORE (EX->espera dado->MEM->WR)");
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

        bool mem_end = rs.UpdateCountdown(fu, 4);
        check("STORE: fim do MEM sinalizado", mem_end);
        check("STORE: fase == WR", rs.GetInstructionPhase() == INSTRUCTION_PHASE::WR);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LIBERAÇÃO E REUSO (Release)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LIBERAÇÃO E REUSO (Release)");

    secao("5.1 Release() — RS de inteiros liberada");
    {
        ReservationStation rs("int4");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        Instruction instr(0, "ADD R3, R1, R2");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);

        rs.Release(3);
        check("IsBusy() == false após Release",         !rs.IsBusy());
        check("GetCountdown() == -1",                   rs.GetCountdown() == -1);
        check("GetFUPosition() == -1",                  rs.GetFUPosition() == -1);
        check("GetQj() vazio",                          rs.GetQj().empty());
        check("GetQk() vazio",                          rs.GetQk().empty());
        check("GetTimes() tem 2 entradas após Release", rs.GetTimes().size() == 2);
        check("GetTimes()[1] == 3 (ciclo de release)",  rs.GetTimes()[1] == 3);
    }

    secao("5.2 Release() após LOAD — caminho de liberação completo e reuso");
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

        check("LOAD antes de Release: fase == WR",  rs.GetInstructionPhase() == INSTRUCTION_PHASE::WR);
        check("LOAD antes de Release: busy == true", rs.IsBusy());

        rs.Release(4);

        check("LOAD: busy == false após Release",    !rs.IsBusy());
        check("LOAD: countdown == -1 após Release",  rs.GetCountdown() == -1);
        check("LOAD: fuPosition == -1 após Release", rs.GetFUPosition() == -1);
        check("LOAD: Qj vazio após Release",         rs.GetQj().empty());
        check("LOAD: Qk vazio após Release",         rs.GetQk().empty());
        check("LOAD: GetTimes().size() == 2",        rs.GetTimes().size() == 2);
        check("LOAD: GetTimes()[0] == 1 (issue)",    rs.GetTimes()[0] == 1);
        check("LOAD: GetTimes()[1] == 4 (release)",  rs.GetTimes()[1] == 4);
        Instruction instr2(1, "L.D F6, 0(R3)");
        bool reuso = rs.AddIssue(instr2, cdb, 5);
        check("LOAD: RS pode ser reusada após Release",  reuso);
    }

    secao("5.3 Reuso de RS — sem autodependência espúria (regressão do resíduo 'tag == id')");
    {
        ReservationStation rs("int7");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();

        Instruction i1(0, "ADD R7, R1, R2");
        rs.AddIssue(i1, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);           // WR
        cdb.R[7].DeallocateRS("int7", 1, 3); // simula fim do broadcast
        rs.Release(3);

        Instruction i2(1, "ADD R7, R7, R1"); // lê e escreve R7 de novo, mesma RS
        rs.AddIssue(i2, cdb, 4);
        check("Sem autodependência espúria ao reler R7 já resolvido", rs.GetQj().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. HAZARDS ESTRUTURAIS — FU
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. HAZARDS ESTRUTURAIS — FU");

    secao("6.1 FU esgotada -> AddIssue ok mas UpdateDependencies retorna false");
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

    secao("6.2 Grupos de FU são independentes — EX (int_basic_alu) vs MEM (memory_access)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, mais fácil de saturar
        CDB cdb = makeCDB();

        ReservationStation rsA("load2"), rsB("load3");
        Instruction ldA(0, "L.D F2, 0(R1)");
        Instruction ldB(1, "L.D F4, 0(R2)");
        rsA.AddIssue(ldA, cdb, 1);
        rsB.AddIssue(ldB, cdb, 1);

        rsA.UpdateDependencies(cdb, fu, 2); // ocupa int_basic_alu
        rsA.UpdateCountdown(fu, 2);         // libera int_basic_alu, vai para MEM (sem FU ainda)

        bool rsB_ex = rsB.UpdateDependencies(cdb, fu, 2); // int_basic_alu livre de novo
        check("rsB usa int_basic_alu livre após rsA liberar", rsB_ex && rsB.GetInstructionPhase() == INSTRUCTION_PHASE::EX);

        bool memA = rsA.UpdateDependencies(cdb, fu, 3); // memory_access livre
        check("rsA inicia MEM (memory_access livre)", memA);

        rsB.UpdateCountdown(fu, 3);         // libera int_basic_alu, tenta ir para MEM
        bool memB = rsB.UpdateDependencies(cdb, fu, 3);
        check("rsB bloqueado no MEM: memory_access já ocupado por rsA", !memB);
    }

    secao("6.3 Roteamento de FU — INT_MUL/INT_DIV usam int_mult_div_alu; int_basic_alu não é afetado");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, fácil de saturar
        CDB cdb = makeCDB();

        ReservationStation rsMul("intmul0"), rsDiv("intdiv0"), rsAdd("intbasic0");
        Instruction iMul(0, "MULT R3, R1, R2");
        Instruction iDiv(1, "DIV R5, R1, R2");
        Instruction iAdd(2, "ADD R6, R1, R2");
        rsMul.AddIssue(iMul, cdb, 1);
        rsDiv.AddIssue(iDiv, cdb, 1);
        rsAdd.AddIssue(iAdd, cdb, 1);

        bool mulEx = rsMul.UpdateDependencies(cdb, fu, 2);
        check("INT_MUL entra em EX e ocupa int_mult_div_alu", mulEx);
        check("INT_MUL: countdown == exLat == 4", rsMul.GetCountdown() == 4);

        bool divEx = rsDiv.UpdateDependencies(cdb, fu, 2);
        check("INT_DIV bloqueado: int_mult_div_alu saturado pelo MULT", !divEx);

        bool addEx = rsAdd.UpdateDependencies(cdb, fu, 2);
        check("INT_BASIC não é afetado pela saturação de int_mult_div_alu", addEx);
        check("INT_BASIC: countdown == exLat == 1", rsAdd.GetCountdown() == 1);
    }

    secao("6.4 Roteamento de FU — FLOAT_BASIC usa float_basic_alu (independente de float_mult_div_alu)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        CDB cdb = makeCDB();

        ReservationStation rsFadd("fbasic0"), rsFadd2("fbasic1"), rsFmul("fmul5");
        Instruction iFadd (0, "ADD.D F2, F0, F4");
        Instruction iFadd2(1, "SUB.D F8, F0, F4");
        Instruction iFmul (2, "MUL.D F6, F0, F4");
        rsFadd.AddIssue(iFadd, cdb, 1);
        rsFadd2.AddIssue(iFadd2, cdb, 1);
        rsFmul.AddIssue(iFmul, cdb, 1);

        bool faddEx = rsFadd.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_BASIC (ADD.D) entra em EX e ocupa float_basic_alu", faddEx);
        check("FLOAT_BASIC: countdown == exLat == 9", rsFadd.GetCountdown() == 9);

        bool fadd2Ex = rsFadd2.UpdateDependencies(cdb, fu, 2);
        check("Segundo FLOAT_BASIC (SUB.D) bloqueado: float_basic_alu saturado", !fadd2Ex);

        bool fmulEx = rsFmul.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_MUL não é afetado pela saturação de float_basic_alu", fmulEx);
        check("FLOAT_MUL: countdown == exLat == 14", rsFmul.GetCountdown() == 14);
    }

    secao("6.5 Roteamento de FU — FLOAT_MUL/FLOAT_DIV compartilham float_mult_div_alu");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        CDB cdb = makeCDB();

        ReservationStation rsFmul("fmul6"), rsFdiv("fdiv0"), rsFadd("fbasic2");
        Instruction iFmul(0, "MUL.D F6, F0, F4");
        Instruction iFdiv(1, "DIV.D F10, F0, F4");
        Instruction iFadd(2, "ADD.D F12, F0, F4");
        rsFmul.AddIssue(iFmul, cdb, 1);
        rsFdiv.AddIssue(iFdiv, cdb, 1);
        rsFadd.AddIssue(iFadd, cdb, 1);

        bool fmulEx = rsFmul.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_MUL entra em EX e ocupa float_mult_div_alu", fmulEx);
        check("FLOAT_MUL: countdown == exLat == 14", rsFmul.GetCountdown() == 14);

        bool fdivEx = rsFdiv.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_DIV bloqueado: float_mult_div_alu saturado pelo FLOAT_MUL", !fdivEx);

        bool faddEx = rsFadd.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_BASIC não é afetado pela saturação de float_mult_div_alu", faddEx);
        check("FLOAT_DIV: exLat esperado == 40 (checado isoladamente)", Instruction(0, "DIV.D F0, F2, F4").GetExLatency() == 40);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
