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
    std::vector<std::string> linhas;
    for (int p = 0; p < position; p++)
        linhas.push_back("add x0, x0, x0"); // dummy: apenas ocupa a posição
    linhas.push_back(line);
    std::vector<std::unique_ptr<Instruction>> parsed =
        InstructionFactory::ParseTrace(linhas, ARCH);
    return std::shared_ptr<Instruction>(std::move(parsed[position]));
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO BÁSICO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO BÁSICO");

    secao("1.1 Instruction() — construtor padrão (via InstructionArm64, que é concreta)");
    {
        InstructionArm64 i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    secao("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add x1, x2, x3");
        check("GetPosition() == 7",                            i->GetPosition() == 7);
        check("GetInstructionString() == 'add   x1, x2, x3'",  i->GetInstructionString() == "add   x1, x2, x3");
    }

    secao("1.3 InstructionFactory — arquitetura de trace (ARM64)");
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

    secao("[ABORT] String vazia deve abortar");
    {
        InstructionArm64 i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] Instrução desconhecida deve abortar");
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

    secao("2.1 LOAD (ldr)");
    {
        auto i = make_inst(0, "ldr x0, [x1]");
        check("ldr: tipo == LOAD",         i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("ldr: exLatency  == 1",      i->GetExLatency()  == 1);
        check("ldr: memLatency == 1",      i->GetMemLatency() == 1);
        check("ldr: dest[0] id=0 (x0)",    i->GetDestRegisters()[0].GetId()   == 0);
        check("ldr: source[0] id=1 (x1)",  i->GetSourceRegisters()[0].GetId() == 1);

        auto off = make_inst(1, "ldr x0, [x1, #8]");
        check("ldr c/ offset: só a base vira fonte", off->GetSourceRegisters().size() == 1);
        check("ldr c/ offset: source[0] id=1 (x1)",  off->GetSourceRegisters()[0].GetId() == 1);

        auto wx = make_inst(2, "ldr w0, [x1]");
        check("ldr 32-bit em base 64-bit: dest classe 'R'", wx->GetDestRegisters()[0].GetType() == 'R');
        check("ldr 32-bit em base 64-bit: source classe 'L'", wx->GetSourceRegisters()[0].GetType() == 'L');
    }

    secao("2.2 STORE (str)");
    {
        auto i = make_inst(3, "str x0, [x1]");
        check("str: tipo == STORE",                i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("str: sem destino",                  i->GetDestRegisters().empty());
        check("str: source[0] id=0 (x0, dado)",    i->GetSourceRegisters()[0].GetId() == 0);
        check("str: source[1] id=1 (x1, base)",    i->GetSourceRegisters()[1].GetId() == 1);
    }

    secao("2.3 INT_BASIC — sem sufixo 's' não mexe em CPSR");
    {
        auto i = make_inst(4, "add x1, x2, x3");
        check("add: tipo == INT_BASIC",  i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("add: 1 destino (sem CPSR)", i->GetDestRegisters().size() == 1);
        check("add: dest id=1",  i->GetDestRegisters()[0].GetId() == 1);
        check("add: source[0] id=2", i->GetSourceRegisters()[0].GetId() == 2);
        check("add: source[1] id=3", i->GetSourceRegisters()[1].GetId() == 3);

        auto imm = make_inst(5, "add x1, x2, #5");
        check("add c/ imediato: 1 única fonte", imm->GetSourceRegisters().size() == 1);

        auto cmp = make_inst(6, "cmp x1, x2");
        check("cmp: não escreve registrador de dados, só CPSR",
            cmp->GetDestRegisters().size() == 1 && cmp->GetDestRegisters()[0].GetType() == 'G');
        check("cmp: 2 fontes (x1, x2)", cmp->GetSourceRegisters().size() == 2);
    }

    secao("2.4 INT_MUL e INT_DIV (mul, sdiv)");
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

    secao("3.1 Condicional (b.eq/b.ne) lê CPSR; incondicional (b) não");
    {
        auto beq = make_inst(0, "b.eq LOOP");
        check("b.eq: tipo == BRANCH",     beq->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("b.eq: 1 fonte (CPSR)",     beq->GetSourceRegisters().size() == 1);
        check("b.eq: source[0] tipo='G'", beq->GetSourceRegisters()[0].GetType() == 'G');

        auto b = make_inst(1, "b LOOP");
        check("b (incondicional): 0 fontes — CPSR não é lido", b->GetSourceRegisters().empty());
    }

    secao("3.2 cbz/cbnz e ret");
    {
        // Limitação conhecida: o SetAttributes de BRANCH só verifica se o
        // opcode contém ".eq"/".ne" para decidir se lê CPSR — ele NUNCA olha
        // para o operando de registrador do cbz/cbnz, então esse registrador
        // simplesmente não é capturado como fonte.
        auto cbz = make_inst(2, "cbz w0, Target");
        check("cbz: registrador operando (w0) não é capturado como fonte",
            cbz->GetSourceRegisters().empty());

        auto ret = make_inst(3, "ret");
        check("ret (ARM64): 0 fontes — contrasta com x86, que sempre usa EFLAGS",
            ret->GetSourceRegisters().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    secao("4.1 FLOAT_BASIC (fadd)");
    {
        auto i = make_inst(4, "fadd d0, d1, d2");
        check("fadd: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fadd: exLatency == 9",      i->GetExLatency() == 9);
        check("fadd: dest[0] id=32 (d0)",  i->GetDestRegisters()[0].GetId()   == 32);
        check("fadd: 1 destino (sem CPSR)",i->GetDestRegisters().size() == 1);
        check("fadd: source[0] id=33 (d1)",i->GetSourceRegisters()[0].GetId() == 33);
        check("fadd: source[1] id=34 (d2)",i->GetSourceRegisters()[1].GetId() == 34);
    }

    secao("4.2 FLOAT_MUL (fmul) e FLOAT_DIV (fdiv)");
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

    secao("5.1 base_ex_latencies / base_mem_latencies — tabelas estáticas (compartilhadas)");
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

    secao("5.2 SetExLatency / SetMemLatency");
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

    secao("6.1 NormalizeInstruction — colchetes/'#' não são reconstruídos (join genérico)");
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

    secao("6.2 BRANCH — só operandos 'parecidos com registrador' são lowercased");
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

    secao("7.1 Aliasing de largura: Xn/Wn e Dn/Sn compartilham id, mudam classe");
    {
        auto x = make_inst(0, "add x1, x2, x3");
        check("x1: classe 'L', id 1", x->GetDestRegisters()[0].GetType() == 'L' && x->GetDestRegisters()[0].GetId() == 1);

        auto w = make_inst(1, "add w1, w2, w3");
        check("w1: classe 'R', id 1 (mesmo id de x1)",
            w->GetDestRegisters()[0].GetType() == 'R' && w->GetDestRegisters()[0].GetId() == 1);
    }

    secao("7.2 [PEGADINHA] Opcode terminado em 's' aciona CPSR mesmo sem ser ADDS/SUBS");
    {
        // fcvtzs é um FLOAT_BASIC (converte float -> inteiro), não tem nada a
        // ver com ADDS/SUBS — mas como o código só checa se a última letra do
        // opcode é 's', ele acaba tratando fcvtzs como se atualizasse CPSR.
        auto i = make_inst(0, "fcvtzs w0, d1");
        check("fcvtzs: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fcvtzs: 2 destinos (w0 + CPSR) — quirk do sufixo 's'",
            i->GetDestRegisters().size() == 2);
        check("fcvtzs: dest[1] é CPSR", i->GetDestRegisters()[1].GetType() == 'G');

        // Contraste: fmul termina em 'l', não dispara o quirk.
        auto fm = make_inst(1, "fmul d0, d1, d2");
        check("fmul: 1 destino só (sem o quirk)", fm->GetDestRegisters().size() == 1);
    }

    secao("7.3 [PEGADINHA] Label numérico/parecido com registrador tem o case corrompido");
    {
        // 'X10' bate no padrão IsRegister (letra X/W/D/S + dígitos), então a
        // normalização o transforma em minúsculo mesmo sendo, na verdade, o
        // NOME de um label (branch incondicional não usa registrador algum).
        auto i = make_inst(0, "b X10");
        check("label 'X10' vira 'x10' na string normalizada (case corrompido)",
            i->GetInstructionString() == "b     x10");
        // Apesar da corrupção de case, como o opcode 'b' não contém .eq/.ne,
        // nenhuma tentativa de lookup de registrador é feita — não há abort.
        check("b: mesmo assim, 0 fontes (não tenta resolver 'x10' como registrador aqui)",
            i->GetSourceRegisters().empty());
    }

    secao("7.4 [PEGADINHA] LDP/STP (par de registradores) são mal interpretados");
    {
        // ldp x0, x1, [x2]: deveria ter 2 destinos (x0, x1) e 1 fonte (x2, base).
        // Mas o código só lê tokens[1] como destino e tokens[2] como fonte —
        // então x1 (segundo destino real) vira "fonte", e x2 (base real) se perde.
        auto ldp = make_inst(0, "ldp x0, x1, x2"); // equivalente pós-split de "ldp x0, x1, [x2]"
        check("ldp: só 1 destino capturado (x0) — x1 é perdido como destino",
            ldp->GetDestRegisters().size() == 1 && ldp->GetDestRegisters()[0].GetId() == 0);
        check("ldp: 'fonte' capturada é x1, não a base real (x2)",
            ldp->GetSourceRegisters().size() == 1 && ldp->GetSourceRegisters()[0].GetId() == 1);

        // stp x0, x1, [x2]: os 2 dados (x0, x1) são capturados corretamente como
        // fonte, mas a base real (x2) nunca é lida — nenhuma fonte extra é
        // adicionada para o endereço.
        auto stp = make_inst(1, "stp x0, x1, x2");
        check("stp: 2 fontes capturadas são os dados (x0, x1)",
            stp->GetSourceRegisters().size() == 2 &&
            stp->GetSourceRegisters()[0].GetId() == 0 && stp->GetSourceRegisters()[1].GetId() == 1);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
