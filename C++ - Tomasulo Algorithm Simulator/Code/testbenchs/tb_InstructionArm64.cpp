/* tb_InstructionArm64.cpp */
// Testbench isolado do módulo Instruction - Arquitetura ARM64
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::ARM_64;

// Helper do testbench: monta uma instrução ARM64 em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
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

    section("1.1 Instruction() — construtor padrão (via InstructionArm64, que é concreta)");
    {
        InstructionArm64 i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    section("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add x1, x2, x3");
        check("GetPosition() == 7",                            i->GetPosition() == 7);
        check("GetInstructionString() == 'add   x1, x2, x3'",  i->GetInstructionString() == "add   x1, x2, x3");
    }

    section("1.3 InstructionFactory — arquitetura de trace (ARM64)");
    {
        std::vector<std::string> trace = {"add x1, x2, x3", "ldr x0, [x1]"};
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
        InstructionArm64 i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionArm64 i(10);
        i.Parse("xpto x1, x2, x3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    section("2.1 LOAD (ldr)");
    {
        auto i = make_inst(0, "ldr x0, [x1]");
        check("ldr: tipo == LOAD",         i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("ldr: exLatency  == 1",      i->GetExLatency()  == 1);
        check("ldr: memLatency == 1",      i->GetMemLatency() == 1);
        check("ldr: dest[0] id=0 (x0)",    i->GetDestRegisters()[0].GetId()   == 0);
        check("ldr: source[0] id=1 (x1)",  i->GetSourceRegisters()[0].GetId() == 1);

        auto off = make_inst(1, "ldr x0, [x1, #8]");
        check("ldr c/ offset: só a base vira fonte (ids = {1})",
            only_ids(off->GetSourceRegisters(), {1}));
        check("ldr c/ offset: fonte tem a base x1", has_reg(off->GetSourceRegisters(), 'L', 1));

        auto wx = make_inst(2, "ldr w0, [x1]");
        check("ldr 32-bit em base 64-bit: dest classe 'R'", wx->GetDestRegisters()[0].GetType() == 'R');
        check("ldr 32-bit em base 64-bit: source classe 'L'", wx->GetSourceRegisters()[0].GetType() == 'L');
    }

    section("2.2 STORE (str)");
    {
        auto i = make_inst(3, "str x0, [x1]");
        check("str: tipo == STORE",                i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("str: sem destino",                  i->GetDestRegisters().empty());
        check("str: fonte x0 (dado)",              has_reg(i->GetSourceRegisters(), 'L', 0));
        check("str: fonte x1 (base)",              has_reg(i->GetSourceRegisters(), 'L', 1));
        check("str: fontes só x0/x1",              only_ids(i->GetSourceRegisters(), {0, 1}));
    }

    section("2.3 INT_BASIC — sem sufixo 's' não mexe em CPSR");
    {
        auto i = make_inst(4, "add x1, x2, x3");
        check("add: tipo == INT_BASIC",  i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("add: destino x1 (sem CPSR)", has_reg(i->GetDestRegisters(), 'L', 1) && no_type(i->GetDestRegisters(), 'G'));
        check("add: fonte x2",  has_reg(i->GetSourceRegisters(), 'L', 2));
        check("add: fonte x3",  has_reg(i->GetSourceRegisters(), 'L', 3));

        auto imm = make_inst(5, "add x1, x2, #5");
        check("add c/ imediato: única fonte é x2", only_ids(imm->GetSourceRegisters(), {2}));

        auto cmp = make_inst(6, "cmp x1, x2");
        check("cmp: não escreve registrador de dados, só CPSR",
            has_reg(cmp->GetDestRegisters(), 'G', 80) && only_ids(cmp->GetDestRegisters(), {80}));
        check("cmp: fontes x1 e x2", has_reg(cmp->GetSourceRegisters(), 'L', 1) && has_reg(cmp->GetSourceRegisters(), 'L', 2));
    }

    section("2.4 INT_MUL e INT_DIV (mul, sdiv)");
    {
        auto mul = make_inst(7, "mul x3, x1, x2");
        check("mul: tipo == INT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mul: exLatency == 4",  mul->GetExLatency() == 4);

        auto div = make_inst(8, "sdiv x3, x1, x2");
        check("sdiv: tipo == INT_DIV", div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("sdiv: exLatency == 10", div->GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    section("3.1 Condicional (b.eq/b.ne) lê CPSR; incondicional (b) não");
    {
        auto beq = make_inst(0, "b.eq LOOP");
        check("b.eq: tipo == BRANCH",     beq->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("b.eq: 1 fonte (CPSR)",     beq->GetSourceRegisters().size() == 1);
        check("b.eq: source[0] tipo='G'", beq->GetSourceRegisters()[0].GetType() == 'G');

        auto b = make_inst(1, "b LOOP");
        check("b (incondicional): 0 fontes — CPSR não é lido", b->GetSourceRegisters().empty());

        auto blo = make_inst(9, "b.lo LOOP");
        check("b.lo: tipo == BRANCH",     blo->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("b.lo: 1 fonte (CPSR)",     blo->GetSourceRegisters().size() == 1);
        check("b.lo: source[0] tipo='G'", blo->GetSourceRegisters()[0].GetType() == 'G');
    }

    section("3.2 cbz/cbnz e ret");
    {
        // cbz/cbnz leem o registrador de teste; ret lê o link register (x30).
        auto cbz = make_inst(2, "cbz w0, Target");
        check("cbz: operando w0 capturado como fonte", has_reg(cbz->GetSourceRegisters(), 'R', 0));
        check("cbz: fontes só a família de w0/x0", only_ids(cbz->GetSourceRegisters(), {0}));

        auto ret = make_inst(3, "ret");
        check("ret (ARM64): lê x30 (endereço de retorno)", has_reg(ret->GetSourceRegisters(), 'L', 30));
        check("ret: fontes só a família de x30", only_ids(ret->GetSourceRegisters(), {30}));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    section("4.1 FLOAT_BASIC (fadd)");
    {
        auto i = make_inst(4, "fadd d0, d1, d2");
        check("fadd: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fadd: exLatency == 9",      i->GetExLatency() == 9);
        check("fadd: destino d0 (sem CPSR)", has_reg(i->GetDestRegisters(), 'S', 32) && no_type(i->GetDestRegisters(), 'G'));
        check("fadd: fonte d1", has_reg(i->GetSourceRegisters(), 'S', 33));
        check("fadd: fonte d2", has_reg(i->GetSourceRegisters(), 'S', 34));
    }

    section("4.2 FLOAT_MUL (fmul) e FLOAT_DIV (fdiv)");
    {
        auto mul = make_inst(5, "fmul d0, d1, d2");
        check("fmul: tipo == FLOAT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("fmul: exLatency == 14",   mul->GetExLatency() == 14);

        auto div = make_inst(6, "fdiv d0, d1, d2");
        check("fdiv: tipo == FLOAT_DIV", div->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("fdiv: exLatency == 40",   div->GetExLatency() == 40);
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
        auto i = make_inst(11, "ldr x0, [x1]");
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

    section("6.1 NormalizeInstruction — colchetes/'#' não são reconstruídos (join genérico)");
    {
        auto i1 = make_inst(0, "ADD X1, X2, X3"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==      "add   x1, x2, x3");

        auto i2 = make_inst(1, "   LDR   X0 ,  [ X1 , #8 ]  ");
        check("espacos/colchetes/# somem, vira lista simples",
            i2->GetInstructionString() ==                                 "ldr   x0, x1, 8");

        auto i3 = make_inst(2, "ADD X1 X2 X3"); // Sem vírgula
        check("sem vírgula -> normalizado com vírgula",
            i3->GetInstructionString() ==                                 "add   x1, x2, x3");
    }

    section("6.2 BRANCH — só operandos 'parecidos com registrador' são lowercased");
    {
        auto i1 = make_inst(0, "CBZ W0, Target");
        check("cbz: opcode+registrador minúsculos, label preservado",
            i1->GetInstructionString() ==                                 "cbz   w0, Target");
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. CASOS ESPECÍFICOS DA ARQUITETURA (ARM64)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. CASOS ESPECÍFICOS — ARM64");

    section("7.1 Aliasing de largura: Xn/Wn e Dn/Sn compartilham id, mudam classe");
    {
        auto x = make_inst(0, "add x1, x2, x3");
        check("x1: classe 'L', id 1", x->GetDestRegisters()[0].GetType() == 'L' && x->GetDestRegisters()[0].GetId() == 1);

        auto w = make_inst(1, "add w1, w2, w3");
        check("w1: classe 'R', id 1 (mesmo id de x1)",
            w->GetDestRegisters()[0].GetType() == 'R' && w->GetDestRegisters()[0].GetId() == 1);
    }

    section("7.2 [PEGADINHA] Opcode terminado em 's' aciona CPSR mesmo sem ser ADDS/SUBS");
    {
        // fcvtzs é um FLOAT_BASIC (converte float -> inteiro), não tem nada a
        // ver com ADDS/SUBS — mas como o código só checa se a última letra do
        // opcode é 's', ele acaba tratando fcvtzs como se atualizasse CPSR.
        auto i = make_inst(0, "fcvtzs w0, d1");
        check("fcvtzs: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fcvtzs: escreve w0 e CPSR — quirk do sufixo 's'",
            has_reg(i->GetDestRegisters(), 'R', 0) && has_reg(i->GetDestRegisters(), 'G', 80));

        // Contraste: fmul termina em 'l', não dispara o quirk.
        auto fm = make_inst(1, "fmul d0, d1, d2");
        check("fmul: destino d0, sem CPSR (sem o quirk)",
            has_reg(fm->GetDestRegisters(), 'S', 32) && no_type(fm->GetDestRegisters(), 'G'));
    }

    section("7.3 [PEGADINHA] Label numérico/parecido com registrador — case preservado agora");
    {
        // IsRegister é table-based (helper compartilhado): "X10" existe na tabela
        // (x10 é registrador de verdade), então ainda é lowercased — o alvo de
        // branch incondicional não usa registrador algum, mas o token casa.
        auto i = make_inst(0, "b X10");
        check("label 'X10' ainda vira 'x10' na string normalizada (x10 é registrador real)",
            i->GetInstructionString() == "b     x10");
        check("b: 0 fontes (opcode 'b' não resolve operando como registrador)",
            i->GetSourceRegisters().empty());

        // NOVO (falso-positivo corrigido): "X99" não existe na tabela (faixa x0-31),
        // então NÃO é registrador → case preservado, sem corromper o label.
        auto i2 = make_inst(1, "b X99");
        check("label 'X99' preserva o case (fora da tabela = não é registrador)",
            i2->GetInstructionString() == "b     X99");
        check("b X99: 0 fontes", i2->GetSourceRegisters().empty());

        // Zero register continua fora: xzr não é registrador de verdade e
        // não vira fonte mesmo em desvio registrador.
        auto i3 = make_inst(2, "cbz xzr, LOOP");
        check("cbz xzr: xzr não vira fonte (IsZeroRegister)",
            i3->GetSourceRegisters().empty());
    }

    section("7.4 LDP/STP (par de registradores)");
    {
        // ldp x0, x1, [x2]: 2 destinos (x0, x1) + 1 fonte (x2, base).
        auto ldp = make_inst(0, "ldp x0, x1, x2"); // equivalente pós-split de "ldp x0, x1, [x2]"
        check("ldp: destinos x0 e x1",
            has_reg(ldp->GetDestRegisters(), 'L', 0) && has_reg(ldp->GetDestRegisters(), 'L', 1));
        check("ldp: destinos só x0/x1 (sem CPSR)", only_ids(ldp->GetDestRegisters(), {0, 1}));
        check("ldp: 1 fonte (base x2)",
            has_reg(ldp->GetSourceRegisters(), 'L', 2) && only_ids(ldp->GetSourceRegisters(), {2}));

        // stp x0, x1, [x2]: os 2 dados (x0, x1) + a base (x2) como fontes.
        auto stp = make_inst(1, "stp x0, x1, x2");
        check("stp: fontes x0, x1 e base x2",
            has_reg(stp->GetSourceRegisters(), 'L', 0) &&
            has_reg(stp->GetSourceRegisters(), 'L', 1) &&
            has_reg(stp->GetSourceRegisters(), 'L', 2));
        check("stp: sem destinos", stp->GetDestRegisters().empty());

        // Convenção real de frame: stp x29, x30, [sp, #-16]!
        auto stp_sp = make_inst(2, "stp x29, x30, sp");
        check("stp com sp: base sp (id 31) entre as fontes",
            has_reg(stp_sp->GetSourceRegisters(), 'L', 31));
        check("stp com sp: fontes só x29/x30/sp", only_ids(stp_sp->GetSourceRegisters(), {29, 30, 31}));
    }

    section("7.5 Aliases sp/lr e zero register (xzr)");
    {
        auto sp_ldr = make_inst(0, "ldr x0, [sp]");
        check("ldr com base sp: fonte id 31, classe 'L'",
            sp_ldr->GetSourceRegisters().size() == 1 &&
            sp_ldr->GetSourceRegisters()[0].GetId() == 31 && sp_ldr->GetSourceRegisters()[0].GetType() == 'L');

        auto lr_ret = make_inst(1, "ret");
        check("ret == x30: id 30", lr_ret->GetSourceRegisters()[0].GetId() == 30);

        // bl escreve o link register (x30) além do salto.
        auto bl = make_inst(2, "bl FUNC");
        check("bl: escreve x30 (dest)",
            bl->GetDestRegisters().size() == 1 && bl->GetDestRegisters()[0].GetId() == 30);
        check("bl: não lê registrador (label não é fonte)", bl->GetSourceRegisters().empty());

        auto xzr = make_inst(3, "mov x0, xzr");
        check("mov x0, xzr: xzr não vira fonte (zero register)",
            xzr->GetSourceRegisters().empty());

        auto wzr = make_inst(4, "str wzr, [x0]");
        check("str wzr, [x0]: só a base vira fonte",
            only_ids(wzr->GetSourceRegisters(), {0}));
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
