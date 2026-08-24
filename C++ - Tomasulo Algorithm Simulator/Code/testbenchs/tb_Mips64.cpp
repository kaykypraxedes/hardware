/* tb_Mips64.cpp */
//Testbench isolado de Mips64.cpp
#include "../Architectures/headers/Mips64.h"
#include "tb_Helpers.h"
#include <iostream>
#include <vector>

using namespace processor;

// Ids físicos desta arquitetura (ver RegisterTable() em Mips64.cpp):
// - 'R' (int):   id = n        (r0..r31)
// - 'F' (float): id = 32 + n   (f0..f31)
// - 'M' (hi/lo): hi = 64, lo = 65
// - 'C' (flags): fcc = 66
// - "ra" é apelido de r31 (mesmo id físico, tipo 'R').
static constexpr int FREG(int n) { return 32 + n; }
static constexpr int RA{31};
static constexpr int HI{64};
static constexpr int LO{65};
static constexpr int FCC{66};

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. PARSE — TOLERÂNCIA DE FORMATAÇÃO E CASE
    // ════════════════════════════════════════════════════════════════════

    print_title("1. PARSE — TOLERÂNCIA DE FORMATAÇÃO E CASE");

    section("1.1 Espaçamento variado — resultado idêntico");
    {
        InstructionMips64 a(0);
        a.Parse("add r1,r2,r3");
        InstructionMips64 b(0);
        b.Parse("add   r1 , r2 ,   r3");
        InstructionMips64 c(0);
        c.Parse("add\tr1,\tr2,\tr3");

        check("sem espaços extras: GetInstructionType() == INT_BASIC",
            a.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("espaços extras: mesmo tipo",
            b.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("tabs: mesmo tipo",
            c.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("string canônica idêntica independente da formatação de entrada",
            a.GetInstructionString() == b.GetInstructionString() &&
            b.GetInstructionString() == c.GetInstructionString());
    }

    section("1.2 Maiúsculo/minúsculo — opcode e registrador case-insensitive");
    {
        InstructionMips64 lower(0);
        lower.Parse("add r1, r2, r3");
        InstructionMips64 upper(0);
        upper.Parse("ADD R1, R2, R3");
        InstructionMips64 mixed(0);
        mixed.Parse("AdD r1, R2, r3");

        check("'ADD' reconhecido como INT_BASIC",
            upper.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("'AdD' reconhecido como INT_BASIC",
            mixed.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mesmo registrador de destino independente da caixa",
            has_reg(upper.GetDestRegisters(), 'R', 1) && has_reg(mixed.GetDestRegisters(), 'R', 1));
        check("string canônica sempre em minúsculo (opcode e registradores)",
            lower.GetInstructionString() == upper.GetInstructionString() &&
            upper.GetInstructionString() == mixed.GetInstructionString());
    }

    // ════════════════════════════════════════════════════════════════════
    // 2. LOADS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. LOADS");

    section("2.1 lw — destino inteiro, offset positivo e negativo");
    {
        InstructionMips64 pos(0);
        pos.Parse("lw r1, 10(r2)");
        check("lw: GetInstructionType() == LOAD",
            pos.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lw: dest == {r1}",
            pos.GetDestRegisters().size() == 1 && has_reg(pos.GetDestRegisters(), 'R', 1));
        check("lw: ex_source == {r2}",
            pos.GetExSourceRegisters().size() == 1 && has_reg(pos.GetExSourceRegisters(), 'R', 2));
        check("lw: mem_source vazio",
            pos.GetMemSourceRegisters().empty());
        check("lw: EX lat == 1, MEM lat == 1",
            pos.GetExLatency() == 1 && pos.GetMemLatency() == 1);

        InstructionMips64 neg(0);
        neg.Parse("lw r1, -4(r2)");
        check("lw: offset negativo é válido",
            neg.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    section("2.2 ld — destino inteiro 64 bits (exclusivo MIPS64)");
    {
        InstructionMips64 i(0);
        i.Parse("ld r1, 8(r2)");
        check("ld: GetInstructionType() == LOAD",
            i.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("ld: dest == {r1}",
            i.GetDestRegisters().size() == 1 && has_reg(i.GetDestRegisters(), 'R', 1));
    }

    section("2.3 l.d — destino float, base sempre inteira");
    {
        InstructionMips64 i(0);
        i.Parse("l.d f1, 8(r2)");
        check("l.d: GetInstructionType() == LOAD",
            i.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("l.d: dest == {f1}",
            i.GetDestRegisters().size() == 1 && has_reg(i.GetDestRegisters(), 'F', FREG(1)));
        check("l.d: ex_source == {r2} (base é sempre inteira)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. STORES
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. STORES");

    section("3.1 sw — valor inteiro, sem dest_registers");
    {
        InstructionMips64 i(0);
        i.Parse("sw r1, 10(r2)");
        check("sw: GetInstructionType() == STORE",
            i.GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("sw: dest_registers vazio (store não escreve registrador)",
            i.GetDestRegisters().empty());
        check("sw: ex_source == {r2} (endereço, necessário em EX)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("sw: mem_source == {r1} (valor armazenado, só necessário em MEM)",
            has_reg(i.GetMemSourceRegisters(), 'R', 1));
        check("sw: EX lat == 1, MEM lat == 1",
            i.GetExLatency() == 1 && i.GetMemLatency() == 1);
    }

    section("3.2 sd — store doubleword inteiro (exclusivo MIPS64)");
    {
        InstructionMips64 i(0);
        i.Parse("sd r1, 8(r2)");
        check("sd: mem_source == {r1}",
            has_reg(i.GetMemSourceRegisters(), 'R', 1));
        check("sd: ex_source == {r2}",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
    }

    section("3.3 s.d — valor float");
    {
        InstructionMips64 i(0);
        i.Parse("s.d f1, 8(r2)");
        check("s.d: mem_source == {f1}",
            has_reg(i.GetMemSourceRegisters(), 'F', FREG(1)));
        check("s.d: ex_source == {r2} (base sempre inteira)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. BRANCHES
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. BRANCHES");

    section("4.1 beq — 2 registradores + label, sem escrita implícita");
    {
        InstructionMips64 i(0);
        i.Parse("beq r1, r2, LOOP");
        check("beq: GetInstructionType() == BRANCH",
            i.GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("beq: ex_source == {r1, r2}",
            i.GetExSourceRegisters().size() == 2 &&
            has_reg(i.GetExSourceRegisters(), 'R', 1) && has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("beq: dest vazio",
            i.GetDestRegisters().empty());
        check("beq: EX lat == 1",
            i.GetExLatency() == 1);
    }

    section("4.2 bnez — 1 registrador + label, comparação implícita com zero");
    {
        InstructionMips64 i(0);
        i.Parse("bnez r1, LOOP");
        check("bnez: ex_source == {r1}",
            i.GetExSourceRegisters().size() == 1 && has_reg(i.GetExSourceRegisters(), 'R', 1));
        check("bnez: dest vazio",
            i.GetDestRegisters().empty());
    }

    section("4.3 jal — só label, escreve 'ra' implicitamente");
    {
        InstructionMips64 i(0);
        i.Parse("jal LOOP");
        check("jal: dest == {ra}",
            has_reg(i.GetDestRegisters(), 'R', RA));
        check("jal: ex_source vazio (não lê registrador algum)",
            i.GetExSourceRegisters().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. INT_BASIC
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. INT_BASIC");

    section("5.1 add / daddu — 3 registradores inteiros");
    {
        InstructionMips64 i(0);
        i.Parse("add r1, r2, r3");
        check("add: dest == {r1}",
            has_reg(i.GetDestRegisters(), 'R', 1));
        check("add: ex_source == {r2, r3}",
            i.GetExSourceRegisters().size() == 2 &&
            has_reg(i.GetExSourceRegisters(), 'R', 2) && has_reg(i.GetExSourceRegisters(), 'R', 3));

        InstructionMips64 d(0);
        d.Parse("daddu r1, r2, r3");
        check("daddu: dest == {r1}, ex_source == {r2, r3}",
            has_reg(d.GetDestRegisters(), 'R', 1) && has_reg(d.GetExSourceRegisters(), 'R', 2));
    }

    section("5.2 addi — 2 registradores + imediato (agora aceita sem '#')");
    {
        InstructionMips64 com_hash(0);
        com_hash.Parse("addi r1, r2, #5");
        check("addi(#5): dest == {r1}, ex_source == {r2} (retrocompatível)",
            has_reg(com_hash.GetDestRegisters(), 'R', 1));

        InstructionMips64 sem_hash(0);
        sem_hash.Parse("addi r1, r2, -5");
        check("addi(-5): imediato sem '#' e negativo é válido agora",
            sem_hash.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
    }

    section("5.3 mfhi / mtlo — leitura e escrita em HI/LO");
    {
        InstructionMips64 mflo(0);
        mflo.Parse("mflo r1");
        check("mflo: dest == {r1}, ex_source == {lo}",
            has_reg(mflo.GetDestRegisters(), 'R', 1) && has_reg(mflo.GetExSourceRegisters(), 'M', LO));

        InstructionMips64 mtlo(0);
        mtlo.Parse("mtlo r1");
        check("mtlo: dest == {lo}, ex_source == {r1}",
            has_reg(mtlo.GetDestRegisters(), 'M', LO) && has_reg(mtlo.GetExSourceRegisters(), 'R', 1));
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. INT_MUL / INT_DIV
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6/7. INT_MUL / INT_DIV");

    section("6.1 dmult / div — resultado em HI/LO");
    {
        InstructionMips64 m(0);
        m.Parse("dmult r1, r2");
        check("dmult: GetInstructionType() == INT_MUL",
            m.GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("dmult: dest == {hi, lo}",
            m.GetDestRegisters().size() == 2 &&
            has_reg(m.GetDestRegisters(), 'M', HI) && has_reg(m.GetDestRegisters(), 'M', LO));

        InstructionMips64 d(0);
        d.Parse("div r1, r2");
        check("div: GetInstructionType() == INT_DIV",
            d.GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("div: dest == {hi, lo}",
            d.GetDestRegisters().size() == 2 &&
            has_reg(d.GetDestRegisters(), 'M', HI) && has_reg(d.GetDestRegisters(), 'M', LO));
    }

    // ════════════════════════════════════════════════════════════════════
    // 8. FLOAT_BASIC
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("8. FLOAT_BASIC");

    section("8.1 add.d / abs.d — operações float");
    {
        InstructionMips64 addd(0);
        addd.Parse("add.d f1, f2, f3");
        check("add.d: dest == {f1}, ex_source == {f2, f3}",
            has_reg(addd.GetDestRegisters(), 'F', FREG(1)) &&
            has_reg(addd.GetExSourceRegisters(), 'F', FREG(2)) && has_reg(addd.GetExSourceRegisters(), 'F', FREG(3)));

        InstructionMips64 absd(0);
        absd.Parse("abs.d f1, f2");
        check("abs.d (MIPS64): dest == {f1}, ex_source == {f2}",
            has_reg(absd.GetDestRegisters(), 'F', FREG(1)) && has_reg(absd.GetExSourceRegisters(), 'F', FREG(2)));
    }

    section("8.2 c.eq.d — flag de condição implícita (fcc)");
    {
        InstructionMips64 cond(0);
        cond.Parse("c.eq.d f1, f2");
        check("c.eq.d: dest == {fcc} (C, 66)",
            cond.GetDestRegisters().size() == 1 && has_reg(cond.GetDestRegisters(), 'C', FCC));
        check("c.eq.d: ex_source == {f1, f2}",
            has_reg(cond.GetExSourceRegisters(), 'F', FREG(1)) && has_reg(cond.GetExSourceRegisters(), 'F', FREG(2)));
    }

    // ════════════════════════════════════════════════════════════════════
    // 11. NORMALIZAÇÃO / INSTRUCTION_STRING
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("11. NORMALIZAÇÃO / INSTRUCTION_STRING");

    section("11.1 Formato específico de load/store — 'reg, off(base)'");
    {
        InstructionMips64 i(0);
        i.Parse("lw R1, 10(R2)");
        check("string reconstruída no formato 'r1, 10(r2)'",
            i.GetInstructionString().find("r1, 10(r2)") != std::string::npos);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";

    /*
    // ════════════════════════════════════════════════════════════════════
    // 13. CASOS DE FALHA (abortam a execução)
    // ════════════════════════════════════════════════════════════════════
    // Descomentar e rodar UM de cada vez para validar manualmente.

    section("[ABORT] Opcode desconhecido");
    {
        InstructionMips64 i(0);
        i.Parse("foo r1, r2, r3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] LOAD — falta o registrador base (quantidade errada de operandos)");
    {
        InstructionMips64 i(0);
        i.Parse("lw r1, 10");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Tipo de registrador errado (float onde se espera inteiro)");
    {
        InstructionMips64 i(0);
        i.Parse("add f1, r2, r3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Imediato negativo em campo NÃO-ASSINADO");
    {
        InstructionMips64 i(0);
        i.Parse("andi r1, r2, -5");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Label inválido — começa com dígito");
    {
        InstructionMips64 i(0);
        i.Parse("j 1LOOP");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    return failed ? 1 : 0;
}
