// ────────────────────────────────────────────────────────────────────────────
//  tb_Components.cpp  —  Testbench isolado de Components.cpp
#include "../headers/Components.h"
#include "tb_helpers.h"
#include <vector>

using namespace processor;

int main() {
    // ────────────────────────────────────────────────────────
    secao("Register() — construtor padrão");
    // ────────────────────────────────────────────────────────
    {
        Register r;
        check("GetType() == 'Z'",  r.GetType() == 'Z');
        check("GetBusy() == false", r.GetBusy() == false);
        check("GetCurrentRS() vazio", r.GetCurrentRS().empty());
        check("GetAllocationTimes() vazio", r.GetAllocationTimes().empty());
        check("GetAllocatedRS() vazio",    r.GetAllocatedRS().empty());
    }

    secao("Register(string) — construtor com nome");
    {
        Register r("R5");
        check("R5: GetType() == 'R'", r.GetType() == 'R');
        check("R5: GetId()   == 5",   r.GetId()   == 5);
        check("R5: GetBusy() == false", r.GetBusy() == false);

        Register f("F12");
        check("F12: GetType() == 'F'", f.GetType() == 'F');
        check("F12: GetId()   == 12",  f.GetId()   == 12);

        Register r0("R0");
        check("R0: GetId() == 0", r0.GetId() == 0);

        Register inv("XYZ");
        check("XYZ: GetType() == 'Z'", inv.GetType() == 'Z');
        check("XYZ: GetId()   == -1",  inv.GetId()   == -1);

        Register vazio("");
        check("'': GetType() == 'Z'", vazio.GetType() == 'Z');
    }

    secao("ToggleBusy()");
    {
        Register r("R1");
        check("antes: GetBusy() == false", r.GetBusy() == false);
        r.ToggleBusy();
        check("depois 1 troca: GetBusy() == true",  r.GetBusy() == true);
        r.ToggleBusy();
        check("depois 2 trocas: GetBusy() == false", r.GetBusy() == false);
    }

    secao("AllocateRS(rs, start)");
    {
        Register r("F4");
        std::string rs1 = "load0";
        r.AllocateRS(rs1, 3);

        check("busy == true após AllocateRS",         r.GetBusy() == true);
        check("GetCurrentRS() == 'load0'",             r.GetCurrentRS() == "load0");
        check("GetAllocatedRS()[0] == 'load0'",       r.GetAllocatedRS().size() == 1
                                                  && r.GetAllocatedRS()[0] == "load0");
        check("GetAllocationTimes().size() == 2 (par start/fim com fim pendente)",
              r.GetAllocationTimes().size() == 2);
        check("GetAllocationTimes()[0] == 3 (start)",
              r.GetAllocationTimes()[0] == 3);
        check("GetAllocationTimes()[1] == -1 (fim pendente)",
              r.GetAllocationTimes()[1] == -1);

        std::string rs2 = "load1";
        r.AllocateRS(rs2, 7);
        check("2a alocacao: GetCurrentRS() == 'load1'",     r.GetCurrentRS() == "load1");
        check("2a alocacao: GetAllocatedRS().size() == 2", r.GetAllocatedRS().size() == 2);
        check("2a alocacao: GetAllocationTimes().size() == 4",
              r.GetAllocationTimes().size() == 4);
    }

    secao("DeallocateRS(rs_id, start_cycle, end_cycle)");
    {
        Register r("R2");
        std::string rs = "int_basic0";
        r.AllocateRS(rs, 5);
        r.DeallocateRS("int_basic0", 5, 8);

        check("busy == false após DeallocateRS",       r.GetBusy() == false);
        check("GetCurrentRS() vazio após desalocar",    r.GetCurrentRS().empty());
        check("tempos: [5, 8]",
              r.GetAllocationTimes().size() == 2
           && r.GetAllocationTimes()[0] == 5
           && r.GetAllocationTimes()[1] == 8);
        check("GetAllocatedRS() ainda contem 'int_basic0'",
              !r.GetAllocatedRS().empty() && r.GetAllocatedRS()[0] == "int_basic0");
    }

    secao("Ciclo completo: aloca -> desaloca -> aloca novamente");
    {
        Register f("F6");
        std::string rs_a = "load1";
        std::string rs_b = "load2";
        f.AllocateRS(rs_a, 2);
        f.DeallocateRS("load1", 2, 4);
        f.AllocateRS(rs_b, 10);
        f.DeallocateRS("load2", 10, 12);

        check("busy == false ao final",    f.GetBusy() == false);
        check("2 RS alocadas no historico", f.GetAllocatedRS().size() == 2);
        auto t = f.GetAllocationTimes();
        check("tempos: [2,4,10,12]",
              t.size() == 4 && t[0]==2 && t[1]==4 && t[2]==10 && t[3]==12);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
