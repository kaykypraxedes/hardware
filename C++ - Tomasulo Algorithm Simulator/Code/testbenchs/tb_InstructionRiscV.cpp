/* tb_InstructionRiscV.cpp */
// Testbench isolado do módulo Instruction - Arquitetura RISC-V
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::RISC_V;

// Helper do testbench: monta uma instrução RISC-V em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
// - As linhas "dummy" anteriores são necessárias porque a Factory atribui a posição pelo índice da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> lines;
    for (int p = 0; p < position; p++)
        lines.push_back("add x0, x0, x0"); // dummy: apenas ocupa a posição
    lines.push_back(line);
    std::vector<std::unique_ptr<Instruction>> parsed =
        InstructionFactory::ParseTrace(lines, ARCH);
    return std::shared_ptr<Instruction>(std::move(parsed[position]));
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO BÁSICO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO BÁSICO");

    section("1.1 Instruction() — construtor padrão (via InstructionRiscV, que é concreta)");
    {
        InstructionRiscV i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    section("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add x1, x2, x3");
        check("GetPosition() == 7",                              i->GetPosition() == 7);
        check("GetInstructionString() == 'add     x1, x2, x3'",  i->GetInstructionString() == "add     x1, x2, x3");
    }

    section("1.3 InstructionFactory — arquitetura de trace (RISCV)");
    {
        std::vector<std::string> trace = {"add x1, x2, x3", "lw x5, 0(x6)"};
        auto parsed = InstructionFactory::ParseTrace(trace, ARCH);
        check("2 instruções parseadas", parsed.size() == 2);
        check("posição 0 == 0",         parsed[0]->GetPosition() == 0);
        check("posição 1 == 1",         parsed[1]->GetPosition() == 1);
        check("posição 0 é INT_BASIC",  parsed[0]->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("posição 1 é LOAD",       parsed[1]->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] String vazia deve abortar");
    {
        InstructionRiscV i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionRiscV i(10);
        i.Parse("xpto x1, x2, x3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Load/store truncado (sem base) deve abortar");
    {
        InstructionRiscV i(11);
        i.Parse("lw x5, 4");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    section("2.1 LOAD (lw)");
    {
        auto i = make_inst(0, "lw x5, 4(x6)");
        check("lw: tipo == LOAD",           i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lw: exLatency  == 1",        i->GetExLatency()  == 1);
        check("lw: memLatency == 1",        i->GetMemLatency() == 1);
        check("lw: dest[0] id=5 (x5)",      i->GetDestRegisters()[0].GetId()   == 5);
        check("lw: source[0] id=6 (x6)",    i->GetSourceRegisters()[0].GetId() == 6);
        check("lw: offset '4' não vira fonte", i->GetSourceRegisters().size() == 1);
    }

    section("2.2 STORE (sw)");
    {
        auto i = make_inst(1, "sw x5, 4(x6)");
        check("sw: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("sw: sem destino",                   i->GetDestRegisters().empty());
        check("sw: source[0] id=5 (x5, dado)",     i->GetSourceRegisters()[0].GetId() == 5);
        check("sw: source[1] id=6 (x6, base)",     i->GetSourceRegisters()[1].GetId() == 6);
    }

    section("2.3 INT_BASIC (add, addi)");
    {
        auto add = make_inst(2, "add x1, x2, x3");
        check("add: tipo == INT_BASIC",  add->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("add: dest id=1",          add->GetDestRegisters()[0].GetId() == 1);
        check("add: source[0] id=2",     add->GetSourceRegisters()[0].GetId() == 2);
        check("add: source[1] id=3",     add->GetSourceRegisters()[1].GetId() == 3);

        auto addi = make_inst(3, "addi x1, x1, 100");
        check("addi: imediato '100' não vira fonte", addi->GetSourceRegisters().size() == 1);
        check("addi: source[0] id=1 (x1, reusado)",  addi->GetSourceRegisters()[0].GetId() == 1);
    }

    section("2.4 INT_MUL e INT_DIV (mul, div)");
    {
        auto mul = make_inst(4, "mul x3, x1, x2");
        check("mul: tipo == INT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mul: exLatency == 4",  mul->GetExLatency() == 4);

        auto div = make_inst(5, "div x3, x1, x2");
        check("div: tipo == INT_DIV", div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("div: exLatency == 10", div->GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    section("3.1 beq — dois registradores viram fonte, label não");
    {
        auto i = make_inst(0, "beq x1, x2, LOOP");
        check("beq: tipo == BRANCH",  i->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("beq: source[0] id=1",  i->GetSourceRegisters()[0].GetId() == 1);
        check("beq: source[1] id=2",  i->GetSourceRegisters()[1].GetId() == 2);
        check("beq: label 'LOOP' não vira fonte", i->GetSourceRegisters().size() == 2);
    }

    section("3.2 jal/jalr — rd é DESTINO (endereço de retorno), não fonte");
    {
        // No RISC-V real, jal rd, label ESCREVE em rd (registrador de retorno).
        // O rd explícito vira destino; sem rd, x1 é o destino implícito.
        auto jal = make_inst(0, "jal x1, LOOP");
        check("jal x1: dest x1 (id 1)", has_reg(jal->GetDestRegisters(), 'L', 1));
        check("jal x1: sem fontes (label não é registrador)", jal->GetSourceRegisters().empty());

        auto jal_impl = make_inst(1, "jal LOOP");
        check("jal sem rd: dest x1 implícito (id 1)",
            jal_impl->GetDestRegisters().size() == 1 && jal_impl->GetDestRegisters()[0].GetId() == 1);

        auto jalr = make_inst(2, "jalr x1, x2, 0");
        check("jalr: dest x1 (rd, id 1)", has_reg(jalr->GetDestRegisters(), 'L', 1));
        check("jalr: fonte x2 (rs1, id 2)", has_reg(jalr->GetSourceRegisters(), 'L', 2));
        check("jalr: sem fonte espúria de rd", only_ids(jalr->GetSourceRegisters(), {2}));
    }

    section("3.3 Pseudo-instruções j e ret");
    {
        // j = jal x0, offset: não escreve nem lê registrador.
        auto j = make_inst(0, "j LOOP");
        check("j: tipo == BRANCH", j->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("j: sem destinos",   j->GetDestRegisters().empty());
        check("j: sem fontes (label não é registrador)", j->GetSourceRegisters().empty());

        // ret = jalr x0, 0(x1): lê x1 (endereço de retorno).
        auto ret = make_inst(1, "ret");
        check("ret: tipo == BRANCH", ret->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("ret: fonte x1 (ra, id 1)", has_reg(ret->GetSourceRegisters(), 'L', 1));
        check("ret: sem destinos", ret->GetDestRegisters().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    section("4.1 FLOAT_BASIC (fadd.d)");
    {
        auto i = make_inst(4, "fadd.d f1, f2, f3");
        check("fadd.d: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fadd.d: exLatency == 9",      i->GetExLatency() == 9);
        check("fadd.d: dest[0] id=33 (f1)",  i->GetDestRegisters()[0].GetId()   == 33);
        check("fadd.d: source[0] id=34 (f2)",i->GetSourceRegisters()[0].GetId() == 34);
        check("fadd.d: source[1] id=35 (f3)",i->GetSourceRegisters()[1].GetId() == 35);
    }

    section("4.2 fcvt.w.s — conversão cruzada entre bancos inteiro e float");
    {
        auto i = make_inst(5, "fcvt.w.s x5, f2");
        check("fcvt.w.s: dest[0] tipo='L' id=5",  i->GetDestRegisters()[0].GetType() == 'L' && i->GetDestRegisters()[0].GetId() == 5);
        check("fcvt.w.s: source[0] tipo='F' id=34", i->GetSourceRegisters()[0].GetType() == 'F' && i->GetSourceRegisters()[0].GetId() == 34);
    }

    section("4.3 FLOAT_MUL (fmul.s) e FLOAT_DIV (fdiv.s)");
    {
        auto mul = make_inst(6, "fmul.s f1, f2, f3");
        check("fmul.s: tipo == FLOAT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("fmul.s: exLatency == 14",   mul->GetExLatency() == 14);

        auto div = make_inst(7, "fdiv.s f1, f2, f3");
        check("fdiv.s: tipo == FLOAT_DIV", div->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("fdiv.s: exLatency == 40",   div->GetExLatency() == 40);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LATÊNCIAS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LATÊNCIAS");

    section("5.1 base_ex_latencies / base_mem_latencies — tabelas estáticas (compartilhadas)");
    {
        check("latEX[NONEXISTENT]=0", Instruction::base_ex_latencies[0]  == 0);
        check("latEX[LOAD]=1",        Instruction::base_ex_latencies[1]  == 1);
        check("latEX[INT_MUL]=4",     Instruction::base_ex_latencies[5]  == 4);
        check("latEX[INT_DIV]=10",    Instruction::base_ex_latencies[6]  == 10);
        check("latEX[FLOAT_BASIC]=9", Instruction::base_ex_latencies[7]  == 9);
        check("latEX[FLOAT_MUL]=14",  Instruction::base_ex_latencies[8]  == 14);
        check("latEX[FLOAT_DIV]=40",  Instruction::base_ex_latencies[9]  == 40);
        check("latMEM[LOAD]=1",       Instruction::base_mem_latencies[0] == 1);
        check("latMEM[STORE]=1",      Instruction::base_mem_latencies[1] == 1);
    }

    section("5.2 SetExLatency / SetMemLatency");
    {
        auto i = make_inst(11, "lw x5, 0(x6)");
        check("antes: exLat == 1",   i->GetExLatency()  == 1);
        check("antes: memLat == 1",  i->GetMemLatency() == 1);
        i->SetExLatency(5);
        i->SetMemLatency(3);
        check("depois: exLat == 5",  i->GetExLatency()  == 5);
        check("depois: memLat == 3", i->GetMemLatency() == 3);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. NORMALIZAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. NORMALIZAÇÃO");

    section("6.1 NormalizeInstruction — casos variados");
    {
        auto i1 = make_inst(0, "ADD X1, X2, X3"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==      "add     x1, x2, x3");

        auto i2 = make_inst(1, "   lw\tx5 ,   4( x6 )  ");
        check("espacos extras + tabs", i2->GetInstructionString() ==       "lw      x5, 4(x6)");

        auto i3 = make_inst(2, "ADD X1 X2 X3"); // Sem vírgulas
        check("sem vírgula -> normalizado com vírgula",
            i3->GetInstructionString() ==                                 "add     x1, x2, x3");
    }

    section("6.2 BRANCH — label 'normal' preserva o case");
    {
        auto i1 = make_inst(0, "beq x1, x2, LOOP");
        check("label 'LOOP' se mantém", i1->GetInstructionString() ==      "beq     x1, x2, LOOP");
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. CASOS ESPECÍFICOS DA ARQUITETURA (RISC-V)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. CASOS ESPECÍFICOS — RISC-V");

    section("7.1 Label que colide com registrador real — vira fonte espúria");
    {
        // 'X5' existe na tabela de registradores (IsRegister é table-based e
        // case-insensitive): a colisão não é só cosmética — 'x5' é registrador
        // de verdade, então o LookupRegister resolve o "label" para o
        // registrador físico x5 e o injeta como 3ª fonte espúria do branch.
        auto i = make_inst(0, "beq x1, x2, X5");
        check("normalização corrompe o case do label ('X5' -> 'x5')",
            i->GetInstructionString() == "beq     x1, x2, x5");
        check("SetAttributes: 3 fontes (x1, x2 + o 'label' X5 virou x5 de verdade)",
            i->GetSourceRegisters().size() == 3);
        check("a 3ª fonte é o registrador x5 (id 5), não um label",
            i->GetSourceRegisters()[2].GetId() == 5);

        // Mesma colisão com nome ABI: 'RA' existe na tabela (ra = x1).
        auto i2 = make_inst(1, "beq x2, x3, RA");
        check("label 'RA' vira 'ra' na string normalizada (ra é registrador ABI real)",
            i2->GetInstructionString() == "beq     x2, x3, ra");
        check("beq x2, x3, RA: 3 fontes (x2, x3 + ra espúrio)",
            i2->GetSourceRegisters().size() == 3 &&
            i2->GetSourceRegisters()[2].GetId() == 1);
    }

    section("7.2 jal — rd vira destino, e o rd implícito é x1");
    {
        // Mesmo teste da seção 3.2, reafirmado aqui como caso específico da
        // arquitetura: JAL semanticamente ESCREVE em rd — com rd explícito
        // ele vira destino; sem rd, x1 é o destino implícito.
        auto jal = make_inst(0, "jal x1, LOOP");
        check("jal x1: x1 registrado como destino (id 1)",
            jal->GetDestRegisters().size() == 1 && jal->GetDestRegisters()[0].GetId() == 1);
        check("jal x1: x1 NÃO aparece em source_registers",
            jal->GetSourceRegisters().empty());
    }

    section("7.3 Offsets/imediatos com sinal ou hexadecimal continuam excluídos da fonte");
    {
        auto neg = make_inst(1, "lw x5, -4(x6)");
        check("offset negativo não vira fonte", neg->GetSourceRegisters().size() == 1);
        check("source[0] ainda é x6 (base)", neg->GetSourceRegisters()[0].GetId() == 6);

        auto hex = make_inst(2, "lw x5, 0x10(x6)");
        check("offset hexadecimal não vira fonte", hex->GetSourceRegisters().size() == 1);
    }

    section("7.4 Apelidos ABI mapeados ao registrador físico");
    {
        auto add = make_inst(0, "add a0, a1, a2");
        check("add a0: dest id=10 (x10)", add->GetDestRegisters()[0].GetId() == 10);
        check("add a1: fonte id=11 (x11)", add->GetSourceRegisters()[0].GetId() == 11);
        check("add a2: fonte id=12 (x12)", add->GetSourceRegisters()[1].GetId() == 12);

        auto sp = make_inst(1, "addi sp, sp, -16");
        check("addi sp: dest id=2 (x2)", sp->GetDestRegisters()[0].GetId() == 2);
        check("addi sp: fonte id=2 (x2)", sp->GetSourceRegisters()[0].GetId() == 2);

        auto lw = make_inst(2, "lw a0, 0(sp)");
        check("lw a0, 0(sp): dest id=10", lw->GetDestRegisters()[0].GetId() == 10);
        check("lw a0, 0(sp): fonte id=2 (sp)", lw->GetSourceRegisters()[0].GetId() == 2);

        auto sd = make_inst(3, "sd ra, 8(sp)");
        check("sd ra: fonte ra (id 1)", has_reg(sd->GetSourceRegisters(), 'L', 1));
        check("sd ra: fonte sp (id 2)", has_reg(sd->GetSourceRegisters(), 'L', 2));
    }

    section("7.5 Pseudo-instruções li/mv/nop/call");
    {
        auto nop = make_inst(0, "nop");
        check("nop: tipo == INT_BASIC", nop->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("nop: sem destinos e sem fontes",
            nop->GetDestRegisters().empty() && nop->GetSourceRegisters().empty());

        auto li = make_inst(1, "li a0, 5");
        check("li: dest id=10 (a0)", has_reg(li->GetDestRegisters(), 'L', 10));
        check("li: sem fontes (imediato não é registrador)", li->GetSourceRegisters().empty());

        auto mv = make_inst(2, "mv a0, a1");
        check("mv: dest id=10 (a0)", has_reg(mv->GetDestRegisters(), 'L', 10));
        check("mv: fonte id=11 (a1)", has_reg(mv->GetSourceRegisters(), 'L', 11));

        auto call = make_inst(3, "call FUNC");
        check("call: tipo == BRANCH", call->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("call: dest x1 (ra, id 1)", has_reg(call->GetDestRegisters(), 'L', 1));
        check("call: sem fontes (label não é registrador)", call->GetSourceRegisters().empty());
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
