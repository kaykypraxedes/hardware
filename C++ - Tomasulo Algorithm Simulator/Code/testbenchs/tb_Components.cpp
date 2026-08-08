/* tb_Components.cpp */
//Testbench isolado de Components.cpp
#include "../headers/Components.h"
#include "tb_Helpers.h"
#include <iostream>
#include <vector>

using namespace processor;

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E PARSING
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E PARSING");

    section("1.1 Register() — construtor padrão");
    {
        Register r;
        check("GetType() == 'Z'",           r.GetType() == 'Z');
        check("GetBusy() == false",         r.GetBusy() == false);
        check("GetCurrentRS() vazio",       r.GetCurrentRS().empty());
        check("GetAllocationTimes() vazio", r.GetAllocationTimes().empty());
        check("GetAllocatedRS() vazio",     r.GetAllocatedRS().empty());
    }

    section("1.2 Register(char, id) — construtor por classe e id");
    {
        Register r('R', 5);
        check("R5: GetType() == 'R'",   r.GetType() == 'R');
        check("R5: GetId()   == 5",     r.GetId()   == 5);
        check("R5: GetBusy() == false", r.GetBusy() == false);

        Register f('F', 12);
        check("F12: GetType() == 'F'", f.GetType() == 'F');
        check("F12: GetId()   == 12",  f.GetId()   == 12);

        Register r0('R', 0);
        check("R0: GetId() == 0",      r0.GetId() == 0);

        // Registrador "vazio" = construtor padrão (sem classe nem id).
        Register empty_reg;
        check("'': GetType() == 'Z'",       empty_reg.GetType() == 'Z');
        check("'': GetId() == -1 (sem id)", empty_reg.GetId() == -1);
    }

    section("1.3 Register(char, id) — id alto (limite)");
    {
        Register ok('R', 31);
        check("R31: válido (limite superior)", ok.GetId() == 31);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] Register('XYZ') — registrador inválido deve abortar");
    {
        Register inv("XYZ");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Register('R') — tipo sem id deve abortar");
    {
        Register r("R");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Register('R32') — id acima do limite deve abortar");
    {
        Register invalido("R32");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. ESTADO DE BUSY
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. ESTADO DE BUSY");

    section("2.1 ToggleBusy() — alternância");
    {
        Register r('R', 1);
        check("antes: GetBusy() == false",           r.GetBusy() == false);
        r.ToggleBusy();
        check("depois 1 troca: GetBusy() == true",   r.GetBusy() == true);
        r.ToggleBusy();
        check("depois 2 trocas: GetBusy() == false", r.GetBusy() == false);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. ALOCAÇÃO E DESALOCAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. ALOCAÇÃO E DESALOCAÇÃO");

    section("3.1 AllocateRS(rs, start) — primeira alocação");
    {
        Register r('F', 4);
        std::string rs1 = "load0";
        r.AllocateRS(rs1, 3);

        check("busy == true após AllocateRS",
            r.GetBusy() == true);
        check("GetCurrentRS() == 'load0'",
            r.GetCurrentRS() == "load0");
        check("GetAllocatedRS()[0] == 'load0'",
            r.GetAllocatedRS().size() == 1 && r.GetAllocatedRS()[0] == "load0");
        check("GetAllocationTimes().size() == 2 (par start/fim com fim pendente)",
            r.GetAllocationTimes().size() == 2);
        check("GetAllocationTimes()[0] == 3 (start)",
            r.GetAllocationTimes()[0] == 3);
        check("GetAllocationTimes()[1] == -1 (fim pendente)",
            r.GetAllocationTimes()[1] == -1);

        std::string rs2 = "load1";
        r.AllocateRS(rs2, 7);
        check("2a alocacao: GetCurrentRS() == 'load1'",
            r.GetCurrentRS() == "load1");
        check("2a alocacao: GetAllocatedRS().size() == 2",
            r.GetAllocatedRS().size() == 2);
        check("2a alocacao: GetAllocationTimes().size() == 4",
            r.GetAllocationTimes().size() == 4);
    }

    section("3.2 DeallocateRS(rs_id, start_cycle, end_cycle) — desalocação simples");
    {
        Register r('R', 2);
        std::string rs = "int_basic0";
        r.AllocateRS(rs, 5);
        r.DeallocateRS("int_basic0", 5, 8);

        check("busy == false após DeallocateRS",
            r.GetBusy() == false);
        check("GetCurrentRS() vazio após desalocar",
            r.GetCurrentRS().empty());
        check("tempos: [5, 8]",
            r.GetAllocationTimes().size() == 2 &&
            r.GetAllocationTimes()[0] == 5 &&
            r.GetAllocationTimes()[1] == 8);
        check("GetAllocatedRS() ainda contem 'int_basic0'",
            !r.GetAllocatedRS().empty() && r.GetAllocatedRS()[0] == "int_basic0");
    }

    section("3.3 DeallocateRS() — casos de erro");
    {
        Register r('R', 9);
        r.AllocateRS("x", 1);
        bool result = r.DeallocateRS("nao_existe", 1, 5);
        check("DeallocateRS retorna false p/ rs_id inexistente", result == false);
        check("estado não foi alterado (ainda busy)", r.GetBusy() == true);

        bool result2 = r.DeallocateRS("x", 999, 5); // start_cycle errado
        check("DeallocateRS retorna false p/ start_cycle errado", result2 == false);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. CONSULTAS DE DEPENDÊNCIA
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. CONSULTAS DE DEPENDÊNCIA");

    section("4.1 GetRSCycleStart(rs_id)");
    {
        Register r('R', 3);
        r.AllocateRS("load0", 5);
        check("start_cycle correto para RS pendente", r.GetRSCycleStart("load0") == 5);
        check("RS inexistente retorna -1",            r.GetRSCycleStart("nope") == -1);

        r.DeallocateRS("load0", 5, 9);
        check("RS já desalocada retorna -1 (não é mais pendente)",
              r.GetRSCycleStart("load0") == -1);
    }

    section("4.2 IsDependencyResolved(rs_id, start_cycle)");
    {
        Register r('F', 2);
        r.AllocateRS("mul0", 3);
        check("pendente: não resolvido",  !r.IsDependencyResolved("mul0", 3));

        r.DeallocateRS("mul0", 3, 10);
        check("desalocado: resolvido",     r.IsDependencyResolved("mul0", 3));

        check("start_cycle errado: não encontrado", !r.IsDependencyResolved("mul0", 99));
        check("rs_id errado: não encontrado",       !r.IsDependencyResolved("outro", 3));
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. MÚLTIPLOS PRODUTORES (WAW)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. MÚLTIPLOS PRODUTORES (WAW)");

    section("5.1 WAW — múltiplos produtores pendentes");
    {
        Register r('R', 4);
        r.AllocateRS("add0", 1);
        r.AllocateRS("add1", 6); // segundo produtor pendente (WAW) para o mesmo registrador

        check("GetCurrentRS() retorna o mais recente pendente (add1)",
              r.GetCurrentRS() == "add1");
        check("busy continua true com 2 pendentes", r.GetBusy() == true);

        // Desaloca o mais antigo primeiro — busy deve continuar true (add1 ainda pendente)
        r.DeallocateRS("add0", 1, 4);
        check("busy == true (add1 ainda pendente)", r.GetBusy() == true);
        check("GetCurrentRS() ainda retorna add1",  r.GetCurrentRS() == "add1");

        // Desaloca o último — agora sim busy deve cair
        r.DeallocateRS("add1", 6, 9);
        check("busy == false (nenhum pendente)", r.GetBusy() == false);
        check("GetCurrentRS() vazio",            r.GetCurrentRS().empty());
    }

    section("5.2 Mesmo rs_id reutilizado em ciclos diferentes");
    {
        Register r('R', 7);
        r.AllocateRS("loop_rs", 2);
        r.DeallocateRS("loop_rs", 2, 5);
        r.AllocateRS("loop_rs", 10); // mesmo nome de RS, reutilizado depois

        check("GetRSCycleStart acha a alocação pendente correta (10, não 2)",
              r.GetRSCycleStart("loop_rs") == 10);
        check("IsDependencyResolved(2) == true (já resolvida)",
              r.IsDependencyResolved("loop_rs", 2));
        check("IsDependencyResolved(10) == false (ainda pendente)",
              !r.IsDependencyResolved("loop_rs", 10));
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. INTEGRAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. INTEGRAÇÃO");

    section("6.1 Ciclo completo: aloca -> desaloca -> aloca novamente");
    {
        Register f('F', 6);
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
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
