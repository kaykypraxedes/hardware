/* tb_ReservationStations.cpp */
// Testbench isolado de ReservationStations.cpp
#include "../headers/ReservationStations.h"
#include "../headers/Components.h"
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <vector>

using namespace processor;

// Público:
// Helper do testbench: monta uma instrução pelo MESMO caminho que a Thread
// usa (InstructionFactory::ParseTrace -> shared_ptr), pois o construtor
// legado "Instruction(posição, string)" não existe mais (D14).
static std::shared_ptr<Instruction> make_inst(const std::string& line) {
    std::vector<std::string> lines{line};
    std::vector<std::unique_ptr<Instruction>> parsed =
        InstructionFactory::ParseTrace(lines, ARCHITECTURE::SIMPLIFIED);
    // A Factory atribui a posição pelo índice da linha (aqui, sempre 0);
    // nenhum teste desta suíte depende da posição.
    return std::shared_ptr<Instruction>(std::move(parsed[0]));
}

static CDB makeCDB() {
    // Mesmo caminho da Thread (InstructionFactory::MakeCDB): gera os slots
    // físicos com os ids corretos (R 0-31, F 32-63).
    return InstructionFactory::MakeCDB(ARCHITECTURE::SIMPLIFIED);
}

// Helpers de acesso ao CDB pelo nome arquitetural (F(cdb, 4) == F4 == slot 36).
// GetReg pesquisa apenas pelo id físico global — F<n> fica em 32 + n (faixa 32-63).
static Register& R(CDB& c, const int i) { return GetReg(c, Register('R', i)); }
static Register& F(CDB& c, const int i) { return GetReg(c, Register('F', 32 + i)); }

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

    section("1.1 ReservationStation() — construtor");
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

    section("2.1 AddIssue() — instrução sem dependências (add r3, r1, r2)");
    {
        ReservationStation rs("int0");
        CDB cdb = makeCDB();
        auto instr = make_inst("add r3, r1, r2");
        bool ok = rs.AddIssue(instr, cdb, 1);
        check("AddIssue() retorna true",
            ok);
        check("IsBusy() == true",
            rs.IsBusy());
        check("GetInstructionPhase() == ISSUE",
            rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
        check("GetQj() vazio (r1 livre)",
            rs.GetQj().empty());
        check("GetQk() vazio (r2 livre)",
            rs.GetQk().empty());
        check("GetInstructions()[0] == 'add    r3, r1, r2'",
              rs.GetInstructions().size() == 1 && rs.GetInstructions()[0] == "add    r3, r1, r2");
        check("GetTimes()[0] == 1 (ciclo de issue)",
              rs.GetTimes().size() == 1 && rs.GetTimes()[0] == 1);
        check("CDB.R[3].GetCurrentRS() == 'int0'",
            R(cdb, 3).GetCurrentRS() == "int0");
    }

    section("2.2 AddIssue() — RS ocupada retorna false");
    {
        ReservationStation rs("int0");
        CDB cdb = makeCDB();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, cdb, 1);

        auto instr2 = make_inst("sub r5, r1, r2");
        bool dup = rs.AddIssue(instr2, cdb, 2);
        check("AddIssue em RS ocupada retorna false", !dup);
    }

    section("2.3 AddIssue() — dependência em Qj (mul.d f4, f2, f0 quando f2 pendente)");
    {
        ReservationStation rs("fmul0");
        CDB cdb = makeCDB();
        std::string prod = "load0";
        F(cdb, 2).AllocateRS(prod, 1);

        auto instr = make_inst("mul.d f4, f2, f0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load0' (f2 pendente)", rs.GetQj() == "load0");
        check("Qk vazio (f0 livre)",         rs.GetQk().empty());
        check("CDB.F[4] -> 'fmul0'",         F(cdb, 4).GetCurrentRS() == "fmul0");
    }

    section("2.4 AddIssue() — 'add r1, r1, r2' sem auto-dependência");
    {
        ReservationStation rs("int1");
        CDB cdb = makeCDB();
        auto instr = make_inst("add r1, r1, r2");
        rs.AddIssue(instr, cdb, 1);
        check("Sem auto-dependência: Qj vazio (r1 estava livre)", rs.GetQj().empty());
        check("Sem auto-dependência: Qk vazio (r2 estava livre)", rs.GetQk().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. DEPENDÊNCIAS — ENTRADA EM EX (UpdateDependencies)");

    section("3.1 UpdateDependencies() — instrução pronta (sem Qj/Qk) entra em EX");
    {
        ReservationStation rs("int2");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, cdb, 1);

        bool started = rs.UpdateDependencies(cdb, fu, 2);
        check("UpdateDependencies retorna true (pronta)",  started);
        check("fase == EX após UpdateDependencies",        rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);
        check("GetCountdown() == exLat == 1",              rs.GetCountdown() == 1);
        check("GetFUPosition() >= 0 (FU alocada)",         rs.GetFUPosition() >= 0);

        bool second = rs.UpdateDependencies(cdb, fu, 2);
        check("Segunda chamada retorna false (já em EX)",     !second);
    }

    section("3.2 UpdateDependencies() — aguarda Qj ser liberado");
    {
        ReservationStation rs("fmul1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        std::string prod = "load0";
        F(cdb, 2).AllocateRS(prod, 1);

        auto instr = make_inst("mul.d f4, f2, f0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load0' antes de liberar", rs.GetQj() == "load0");

        bool before = rs.UpdateDependencies(cdb, fu, 3);
        check("UpdateDependencies retorna false com Qj pendente", !before);

        F(cdb, 2).DeallocateRS("load0", 1, 3);
        bool after = rs.UpdateDependencies(cdb, fu, 4);
        check("UpdateDependencies retorna true após Qj liberado", after);
        check("fase == EX após Qj liberado", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);
    }

    section("3.3 STORE com endereço pronto (Qk) mas dado pendente (Qj) entra em EX");
    {
        ReservationStation rs("store1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        F(cdb, 8).AllocateRS("fmul2", 1); // dado (f8) pendente; endereço (r1) livre

        auto instr = make_inst("s.d f8, 0(r1)");
        rs.AddIssue(instr, cdb, 2);
        check("Qk vazio (endereço livre)",        rs.GetQk().empty());
        check("Qj == 'fmul2' (dado pendente)",    rs.GetQj() == "fmul2");

        bool entered_ex = rs.UpdateDependencies(cdb, fu, 3);
        check("STORE entra em EX mesmo com dado pendente", entered_ex);

        rs.UpdateCountdown(fu, 3);
        check("fase == MEM aguardando dado",      rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);

        bool blocked = rs.UpdateDependencies(cdb, fu, 4);
        check("MEM bloqueado enquanto Qj não resolve", !blocked);

        F(cdb, 8).DeallocateRS("fmul2", 1, 5);
        bool mem_ok = rs.UpdateDependencies(cdb, fu, 5);
        check("MEM inicia após dado resolvido",   mem_ok);
    }

    section("3.4 STORE com endereço pendente (Qk) NÃO entra em EX");
    {
        ReservationStation rs("store2");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        R(cdb, 9).AllocateRS("int5", 1); // endereço pendente

        auto instr = make_inst("s.d f0, 0(r9)");
        rs.AddIssue(instr, cdb, 2);
        check("Qk == 'int5' (endereço pendente)", rs.GetQk() == "int5");

        bool blocked = rs.UpdateDependencies(cdb, fu, 3);
        check("STORE não entra em EX com endereço pendente", !blocked);
        check("fase permanece ISSUE",             rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
    }

    section("3.5 ResolveDependency() — captura direta de Vj via broadcast simulado");
    {
        ReservationStation rs("fmul3");
        CDB cdb = makeCDB();
        F(cdb, 2).AllocateRS("load2", 1);

        auto instr = make_inst("mul.d f4, f2, f0");
        rs.AddIssue(instr, cdb, 2);
        check("Qj == 'load2' antes do broadcast", rs.GetQj() == "load2");

        rs.ResolveDependency("load2", F(cdb, 2));
        check("Qj limpo após ResolveDependency",  rs.GetQj().empty());

        // rs_id que não bate não deve afetar nada
        ReservationStation rs2("fmul4");
        F(cdb, 10).AllocateRS("load3", 1);
        auto instr2 = make_inst("mul.d f12, f10, f0");
        rs2.AddIssue(instr2, cdb, 2);
        rs2.ResolveDependency("outro_produtor_qualquer", F(cdb, 10));
        check("ResolveDependency com rs_id que não bate não altera Qj", rs2.GetQj() == "load3");
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] TryAllocateFU com ex_latency == 0 (via SetExLatency)");
    {
        ReservationStation rs("int_abort0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        instr->SetExLatency(0);
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2); // deve abortar dentro de TryAllocateFU (EX)
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] TryAllocateFU com mem_latency == 0 (LOAD, via SetMemLatency)");
    {
        ReservationStation rs("load_abort0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("l.d f2, 0(r1)");
        instr->SetMemLatency(0);
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

    section("4.1 UpdateCountdown() — INT_BASIC (exLat=1): EX -> WR");
    {
        ReservationStation rs("int3");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);

        bool finished = rs.UpdateCountdown(fu, 2);
        check("UpdateCountdown retorna true (EX terminou)", finished);
        check("fase == WR após EX de 1 ciclo",              rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
        check("GetFUPosition() == -1 (FU liberada)",        rs.GetFUPosition() == -1);
    }

    section("4.2 UpdateCountdown() — LOAD (EX->MEM->WR)");
    {
        ReservationStation rs("load0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("l.d f2, 0(r1)");
        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);

        bool ex_done = rs.UpdateCountdown(fu, 2);
        check("LOAD: UpdateCountdown sinaliza fim do EX", ex_done);
        check("LOAD: fase == MEM após EX",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);
        check("LOAD: countdown == -1 (aguarda iniciar MEM)", rs.GetCountdown() == -1);

        bool started_mem = rs.UpdateDependencies(cdb, fu, 3);
        check("LOAD: UpdateDependencies inicia MEM no ciclo 3", started_mem);
        check("LOAD: countdown == latMEM == 1 após iniciar MEM", rs.GetCountdown() == 1);

        bool mem_end = rs.UpdateCountdown(fu, 3);
        check("LOAD: UpdateCountdown sinaliza fim do MEM", mem_end);
        check("LOAD: fase == WR após MEM",                 rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
    }

    section("4.3 UpdateCountdown() — STORE (EX->espera dado->MEM->WR)");
    {
        ReservationStation rs("store0");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        std::string prod = "float_basico0";
        F(cdb, 6).AllocateRS(prod, 1);

        auto instr = make_inst("s.d f6, 0(r2)");
        rs.AddIssue(instr, cdb, 2);
        rs.UpdateDependencies(cdb, fu, 3);

        bool ex_done = rs.UpdateCountdown(fu, 3);
        check("STORE: fim do EX sinalizado", ex_done);
        check("STORE: countdown == -1 após EX (aguarda dado)", rs.GetCountdown() == -1);

        F(cdb, 6).DeallocateRS("float_basico0", 1, 4);
        bool mem_ok = rs.UpdateDependencies(cdb, fu, 4);
        check("STORE: UpdateDependencies inicia MEM após dado liberado", mem_ok);
        check("STORE: fase == MEM", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::MEM);

        bool mem_end = rs.UpdateCountdown(fu, 4);
        check("STORE: fim do MEM sinalizado", mem_end);
        check("STORE: fase == WR", rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LIBERAÇÃO E REUSO (Release)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LIBERAÇÃO E REUSO (Release)");

    section("5.1 Release() — RS de inteiros liberada");
    {
        ReservationStation rs("int4");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("add r3, r1, r2");
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

    section("5.2 Release() após LOAD — caminho de liberação completo e reuso");
    {
        ReservationStation rs("load1");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();
        auto instr = make_inst("l.d f4, 0(r2)");

        rs.AddIssue(instr, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);
        rs.UpdateDependencies(cdb, fu, 3);
        rs.UpdateCountdown(fu, 3);

        check("LOAD antes de Release: fase == WR",  rs.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::WR);
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
        auto instr2 = make_inst("l.d f6, 0(r3)");
        bool reuse = rs.AddIssue(instr2, cdb, 5);
        check("LOAD: RS pode ser reusada após Release",  reuse);
    }

    section("5.3 Reuso de RS — sem autodependência espúria (regressão do resíduo 'tag == id')");
    {
        ReservationStation rs("int7");
        CDB cdb = makeCDB();
        FUNCTIONAL_UNITS fu = makeFU();

        auto i1 = make_inst("add r7, r1, r2");
        rs.AddIssue(i1, cdb, 1);
        rs.UpdateDependencies(cdb, fu, 2);
        rs.UpdateCountdown(fu, 2);           // WR
        R(cdb, 7).DeallocateRS("int7", 1, 3); // simula fim do broadcast
        rs.Release(3);

        auto i2 = make_inst("add r7, r7, r1"); // lê e escreve r7 de novo, mesma RS
        rs.AddIssue(i2, cdb, 4);
        check("Sem autodependência espúria ao reler R7 já resolvido", rs.GetQj().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. HAZARDS ESTRUTURAIS — FU
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. HAZARDS ESTRUTURAIS — FU");

    section("6.1 FU esgotada -> AddIssue ok mas UpdateDependencies retorna false");
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
        auto i0 = make_inst("add r3, r1, r2");
        auto i1 = make_inst("sub r5, r3, r4");
        rs0.AddIssue(i0, cdb, 1);
        rs1.AddIssue(i1, cdb, 1);

        rs0.UpdateDependencies(cdb, fu, 2);
        bool blocked = rs1.UpdateDependencies(cdb, fu, 2);
        check("Segunda RS bloqueada quando FU esgotada", !blocked);
        check("rs1 ainda em ISSUE",
              rs1.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::IS);
    }

    section("6.2 Grupos de FU são independentes — EX (int_basic_alu) vs MEM (memory_access)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, mais fácil de saturar
        CDB cdb = makeCDB();

        ReservationStation rsA("load2"), rsB("load3");
        auto ldA = make_inst("l.d f2, 0(r1)");
        auto ldB = make_inst("l.d f4, 0(r2)");
        rsA.AddIssue(ldA, cdb, 1);
        rsB.AddIssue(ldB, cdb, 1);

        rsA.UpdateDependencies(cdb, fu, 2); // ocupa int_basic_alu
        rsA.UpdateCountdown(fu, 2);         // libera int_basic_alu, vai para MEM (sem FU ainda)

        bool rsB_ex = rsB.UpdateDependencies(cdb, fu, 2); // int_basic_alu livre de novo
        check("rsB usa int_basic_alu livre após rsA liberar", rsB_ex && rsB.GetInstructionPhase() == INSTRUCTION_PHASE_TOMASULO::EX);

        bool memA = rsA.UpdateDependencies(cdb, fu, 3); // memory_access livre
        check("rsA inicia MEM (memory_access livre)", memA);

        rsB.UpdateCountdown(fu, 3);         // libera int_basic_alu, tenta ir para MEM
        bool memB = rsB.UpdateDependencies(cdb, fu, 3);
        check("rsB bloqueado no MEM: memory_access já ocupado por rsA", !memB);
    }

    section("6.3 Roteamento de FU — INT_MUL/INT_DIV usam int_mult_div_alu; int_basic_alu não é afetado");
    {
        FUNCTIONAL_UNITS fu = makeFU(1); // 1 FU por grupo, fácil de saturar
        CDB cdb = makeCDB();

        ReservationStation rsMul("intmul0"), rsDiv("intdiv0"), rsAdd("intbasic0");
        auto iMul = make_inst("mult r3, r1, r2");
        auto iDiv = make_inst("div r5, r1, r2");
        auto iAdd = make_inst("add r6, r1, r2");
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

    section("6.4 Roteamento de FU — FLOAT_BASIC usa float_basic_alu (independente de float_mult_div_alu)");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        CDB cdb = makeCDB();

        ReservationStation rsFadd("fbasic0"), rsFadd2("fbasic1"), rsFmul("fmul5");
        auto iFadd = make_inst("add.d f2, f0, f4");
        auto iFadd2 = make_inst("sub.d f8, f0, f4");
        auto iFmul = make_inst("mul.d f6, f0, f4");
        rsFadd.AddIssue(iFadd, cdb, 1);
        rsFadd2.AddIssue(iFadd2, cdb, 1);
        rsFmul.AddIssue(iFmul, cdb, 1);

        bool faddEx = rsFadd.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_BASIC (add.d) entra em EX e ocupa float_basic_alu", faddEx);
        check("FLOAT_BASIC: countdown == exLat == 9", rsFadd.GetCountdown() == 9);

        bool fadd2Ex = rsFadd2.UpdateDependencies(cdb, fu, 2);
        check("Segundo FLOAT_BASIC (sub.d) bloqueado: float_basic_alu saturado", !fadd2Ex);

        bool fmulEx = rsFmul.UpdateDependencies(cdb, fu, 2);
        check("FLOAT_MUL não é afetado pela saturação de float_basic_alu", fmulEx);
        check("FLOAT_MUL: countdown == exLat == 14", rsFmul.GetCountdown() == 14);
    }

    section("6.5 Roteamento de FU — FLOAT_MUL/FLOAT_DIV compartilham float_mult_div_alu");
    {
        FUNCTIONAL_UNITS fu = makeFU(1);
        CDB cdb = makeCDB();

        ReservationStation rsFmul("fmul6"), rsFdiv("fdiv0"), rsFadd("fbasic2");
        auto iFmul = make_inst("mul.d f6, f0, f4");
        auto iFdiv = make_inst("div.d f10, f0, f4");
        auto iFadd = make_inst("add.d f12, f0, f4");
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
        check("FLOAT_DIV: exLat esperado == 40 (checado isoladamente)", make_inst("div.d f0, f2, f4")->GetExLatency() == 40);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
