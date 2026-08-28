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
        check("GetCurrentProducer() == -1", r.GetCurrentProducer() == -1);
        check("GetAllocationTimes() vazio", r.GetAllocationTimes().empty());
        check("GetAllocatedRS() vazio",     r.GetAllocatedRS().empty());
        check("GetProducerPositions() vazio", r.GetProducerPositions().empty());
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

    section("3.1 AllocateProducer(position, rs, cycle) — primeira alocação");
    {
        Register r('F', 4);
        std::string rs1 = "load0";
        r.AllocateProducer(4, rs1, 3);

        check("busy == true após AllocateProducer",
            r.GetBusy() == true);
        check("GetCurrentProducer() == 4",
            r.GetCurrentProducer() == 4);
        check("GetProducerPositions()[0] == 4",
            r.GetProducerPositions().size() == 1 && r.GetProducerPositions()[0] == 4);
        check("GetAllocatedRS()[0] == 'load0'",
            r.GetAllocatedRS().size() == 1 && r.GetAllocatedRS()[0] == "load0");
        check("GetAllocationTimes().size() == 2 (par start/fim com fim pendente)",
            r.GetAllocationTimes().size() == 2);
        check("GetAllocationTimes()[0] == 3 (start)",
            r.GetAllocationTimes()[0] == 3);
        check("GetAllocationTimes()[1] == -1 (fim pendente)",
            r.GetAllocationTimes()[1] == -1);

        std::string rs2 = "load1";
        r.AllocateProducer(9, rs2, 7);
        check("2a alocacao: GetCurrentProducer() == 9",
            r.GetCurrentProducer() == 9);
        check("2a alocacao: GetAllocatedRS().size() == 2",
            r.GetAllocatedRS().size() == 2);
        check("2a alocacao: GetAllocationTimes().size() == 4",
            r.GetAllocationTimes().size() == 4);
    }

    section("3.2 DeallocateProducer(position, cycle) — desalocação simples");
    {
        Register r('R', 2);
        std::string rs = "int_basic0";
        r.AllocateProducer(2, rs, 5);
        r.DeallocateProducer(2, 8);

        check("busy == false após DeallocateProducer",
            r.GetBusy() == false);
        check("GetCurrentProducer() == -1 após desalocar",
            r.GetCurrentProducer() == -1);
        check("tempos: [5, 8]",
            r.GetAllocationTimes().size() == 2 &&
            r.GetAllocationTimes()[0] == 5 &&
            r.GetAllocationTimes()[1] == 8);
        check("GetAllocatedRS() ainda contem 'int_basic0'",
            !r.GetAllocatedRS().empty() && r.GetAllocatedRS()[0] == "int_basic0");
    }

    section("3.3 DeallocateProducer() — casos de erro");
    {
        Register r('R', 9);
        r.AllocateProducer(3, "x", 1);
        bool result = r.DeallocateProducer(99, 5);
        check("DeallocateProducer retorna false p/ posição inexistente", result == false);
        check("estado não foi alterado (ainda busy)", r.GetBusy() == true);

        r.DeallocateProducer(3, 5);
        check("DeallocateProducer repetido retorna false", !r.DeallocateProducer(3, 6));
    }

    section("3.4 AllocateProducer() — posições inválidas abortam");
    {
        check("posição negativa aborta", Aborts([]() {
            Register r('R', 1);
            r.AllocateProducer(-1, "int0", 1);
        }));
        check("posição duplicada aborta", Aborts([]() {
            Register r('R', 1);
            r.AllocateProducer(4, "int0", 1);
            r.AllocateProducer(4, "int1", 2);
        }));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. CONSULTAS DE DEPENDÊNCIA
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. CONSULTAS DE DEPENDÊNCIA");

    section("4.1 GetCurrentProducer()");
    {
        Register r('R', 3);
        r.AllocateProducer(12, "load0", 5);
        check("produtor pendente correto", r.GetCurrentProducer() == 12);
        r.DeallocateProducer(12, 9);
        check("produtor finalizado deixa de ser atual", r.GetCurrentProducer() == -1);
    }

    section("4.2 IsDependencyResolved(position)");
    {
        Register r('F', 2);
        r.AllocateProducer(7, "mul0", 3);
        check("pendente: não resolvido",  !r.IsDependencyResolved(7));

        r.DeallocateProducer(7, 10);
        check("desalocado: resolvido",     r.IsDependencyResolved(7));

        check("posição desconhecida não está resolvida", !r.IsDependencyResolved(99));
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. MÚLTIPLOS PRODUTORES (WAW)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. MÚLTIPLOS PRODUTORES (WAW)");

    section("5.1 WAW — múltiplos produtores pendentes");
    {
        Register r('R', 4);
        r.AllocateProducer(2, "add0", 1);
        r.AllocateProducer(5, "add1", 6); // segundo produtor pendente (WAW) para o mesmo registrador

        check("GetCurrentProducer() retorna o mais recente pendente",
              r.GetCurrentProducer() == 5);
        check("busy continua true com 2 pendentes", r.GetBusy() == true);

        // Desaloca o mais antigo primeiro — busy deve continuar true (add1 ainda pendente)
        r.DeallocateProducer(2, 4);
        check("busy == true (add1 ainda pendente)", r.GetBusy() == true);
        check("produtor atual continua sendo a posição 5", r.GetCurrentProducer() == 5);
        check("produtor antigo resolvido", r.IsDependencyResolved(2));
        check("produtor novo continua pendente", !r.IsDependencyResolved(5));

        // Desaloca o último — agora sim busy deve cair
        r.DeallocateProducer(5, 9);
        check("busy == false (nenhum pendente)", r.GetBusy() == false);
        check("GetCurrentProducer() == -1",     r.GetCurrentProducer() == -1);
    }

    section("5.2 Mesmo rs_id reutilizado em ciclos diferentes");
    {
        Register r('R', 7);
        r.AllocateProducer(2, "loop_rs", 2);
        r.DeallocateProducer(2, 5);
        r.AllocateProducer(10, "loop_rs", 10); // mesmo nome físico, novo produtor lógico

        check("IsDependencyResolved(2) == true (já resolvida)",
              r.IsDependencyResolved(2));
        check("IsDependencyResolved(10) == false (ainda pendente)",
              !r.IsDependencyResolved(10));
        check("histórico preserva o mesmo nome físico duas vezes",
              r.GetAllocatedRS() == std::vector<std::string>{"loop_rs", "loop_rs"});
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
        f.AllocateProducer(1, rs_a, 2);
        f.DeallocateProducer(1, 4);
        f.AllocateProducer(2, rs_b, 10);
        f.DeallocateProducer(2, 12);

        check("busy == false ao final",    f.GetBusy() == false);
        check("2 RS alocadas no historico", f.GetAllocatedRS().size() == 2);
        check("posições [1,2] preservadas", f.GetProducerPositions() == std::vector<int>{1, 2});
        auto t = f.GetAllocationTimes();
        check("tempos: [2,4,10,12]",
              t.size() == 4 && t[0]==2 && t[1]==4 && t[2]==10 && t[3]==12);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
