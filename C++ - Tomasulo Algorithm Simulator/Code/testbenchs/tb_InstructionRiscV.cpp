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

    secao("1.1 Instruction() — construtor padrão (via InstructionRiscV, que é concreta)");
    {
        InstructionRiscV i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    secao("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add x1, x2, x3");
        check("GetPosition() == 7",                              i->GetPosition() == 7);
        check("GetInstructionString() == 'add     x1, x2, x3'",  i->GetInstructionString() == "add     x1, x2, x3");
    }

    secao("1.3 InstructionFactory — arquitetura de trace (RISCV)");
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

    secao("[ABORT] String vazia deve abortar");
    {
        InstructionRiscV i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionRiscV i(10);
        i.Parse("xpto x1, x2, x3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] Nome ABI (ra/sp/t0) não é suportado — só x0..x31/f0..f31");
    {
        // Diferente do MIPS32 (que tem uma tabela rica de aliases), este
        // RegisterTable só cadastra nomes numéricos. Um nome ABI como 'ra'
        // aborta por não ser encontrado.
        InstructionRiscV i(11);
        i.Parse("jalr ra, x1, 0");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    secao("2.1 LOAD (lw)");
    {
        auto i = make_inst(0, "lw x5, 4(x6)");
        check("lw: tipo == LOAD",           i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lw: exLatency  == 1",        i->GetExLatency()  == 1);
        check("lw: memLatency == 1",        i->GetMemLatency() == 1);
        check("lw: dest[0] id=5 (x5)",      i->GetDestRegisters()[0].GetId()   == 5);
        check("lw: source[0] id=6 (x6)",    i->GetSourceRegisters()[0].GetId() == 6);
        check("lw: offset '4' não vira fonte", i->GetSourceRegisters().size() == 1);
    }

    secao("2.2 STORE (sw)");
    {
        auto i = make_inst(1, "sw x5, 4(x6)");
        check("sw: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("sw: sem destino",                   i->GetDestRegisters().empty());
        check("sw: source[0] id=5 (x5, dado)",     i->GetSourceRegisters()[0].GetId() == 5);
        check("sw: source[1] id=6 (x6, base)",     i->GetSourceRegisters()[1].GetId() == 6);
    }

    secao("2.3 INT_BASIC (add, addi)");
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

    secao("2.4 INT_MUL e INT_DIV (mul, div)");
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

    secao("3.1 beq — dois registradores viram fonte, label não");
    {
        auto i = make_inst(0, "beq x1, x2, LOOP");
        check("beq: tipo == BRANCH",  i->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("beq: source[0] id=1",  i->GetSourceRegisters()[0].GetId() == 1);
        check("beq: source[1] id=2",  i->GetSourceRegisters()[1].GetId() == 2);
        check("beq: label 'LOOP' não vira fonte", i->GetSourceRegisters().size() == 2);
    }

    secao("3.2 [PEGADINHA] jal/jalr — o registrador de destino (rd) é capturado como se fosse fonte");
    {
        // No RISC-V real, jal rd, label ESCREVE em rd (é o registrador de
        // retorno) — não o lê. Mas o SetAttributes de BRANCH trata qualquer
        // token "parecido com registrador" genericamente como fonte,
        // independente da posição, então rd acaba entrando como fonte.
        auto jal = make_inst(0, "jal x1, LOOP");
        check("jal: x1 (na verdade o destino/rd) aparece como fonte",
            jal->GetSourceRegisters().size() == 1 && jal->GetSourceRegisters()[0].GetId() == 1);

        auto jalr = make_inst(1, "jalr x1, x2, 0");
        check("jalr: x1 (rd) e x2 (rs1) ambos viram fonte",
            jalr->GetSourceRegisters().size() == 2 &&
            jalr->GetSourceRegisters()[0].GetId() == 1 && jalr->GetSourceRegisters()[1].GetId() == 2);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    secao("4.1 FLOAT_BASIC (fadd.d)");
    {
        auto i = make_inst(4, "fadd.d f1, f2, f3");
        check("fadd.d: tipo == FLOAT_BASIC", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("fadd.d: exLatency == 9",      i->GetExLatency() == 9);
        check("fadd.d: dest[0] id=33 (f1)",  i->GetDestRegisters()[0].GetId()   == 33);
        check("fadd.d: source[0] id=34 (f2)",i->GetSourceRegisters()[0].GetId() == 34);
        check("fadd.d: source[1] id=35 (f3)",i->GetSourceRegisters()[1].GetId() == 35);
    }

    secao("4.2 fcvt.w.s — conversão cruzada entre bancos inteiro e float");
    {
        auto i = make_inst(5, "fcvt.w.s x5, f2");
        check("fcvt.w.s: dest[0] tipo='L' id=5",  i->GetDestRegisters()[0].GetType() == 'L' && i->GetDestRegisters()[0].GetId() == 5);
        check("fcvt.w.s: source[0] tipo='F' id=34", i->GetSourceRegisters()[0].GetType() == 'F' && i->GetSourceRegisters()[0].GetId() == 34);
    }

    secao("4.3 FLOAT_MUL (fmul.s) e FLOAT_DIV (fdiv.s)");
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

    secao("6.1 NormalizeInstruction — casos variados");
    {
        auto i1 = make_inst(0, "ADD X1, X2, X3"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==      "add     x1, x2, x3");

        auto i2 = make_inst(1, "   lw\tx5 ,   4( x6 )  ");
        check("espacos extras + tabs", i2->GetInstructionString() ==       "lw      x5, 4(x6)");

        auto i3 = make_inst(2, "ADD X1 X2 X3"); // Sem vírgulas
        check("sem vírgula -> normalizado com vírgula",
            i3->GetInstructionString() ==                                 "add     x1, x2, x3");
    }

    secao("6.2 BRANCH — label 'normal' preserva o case");
    {
        auto i1 = make_inst(0, "beq x1, x2, LOOP");
        check("label 'LOOP' se mantém", i1->GetInstructionString() ==      "beq     x1, x2, LOOP");
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. CASOS ESPECÍFICOS DA ARQUITETURA (RISC-V)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. CASOS ESPECÍFICOS — RISC-V");

    secao("7.1 [PEGADINHA CENTRAL] Label numérico colide SILENCIOSAMENTE com um registrador real");
    {
        // 'X5' bate no padrão IsRegister (letra X/F + dígitos) igual a ARM64.
        // A diferença é que aqui a colisão não é só cosmética: 'x5' É um
        // registrador válido de verdade (x5), então o LookupRegister não
        // aborta — ele silenciosamente resolve o "label" para o registrador
        // físico x5 e o injeta como uma 3ª fonte espúria da instrução.
        auto i = make_inst(0, "beq x1, x2, X5");
        check("normalização corrompe o case do label ('X5' -> 'x5')",
            i->GetInstructionString() == "beq     x1, x2, x5");
        check("SetAttributes: 3 fontes (x1, x2 + o 'label' X5 virou x5 de verdade)",
            i->GetSourceRegisters().size() == 3);
        check("a 3ª fonte é o registrador x5 (id 5), não um label",
            i->GetSourceRegisters()[2].GetId() == 5);
    }

    secao("7.2 [PEGADINHA] jal/jalr — rd tratado como fonte, não como destino (revisão)");
    {
        // Mesmo teste da seção 3.2, reafirmado aqui como pegadinha específica
        // da arquitetura: JAL semanticamente ESCREVE em rd, mas o parser
        // genérico de BRANCH não distingue posição de operando.
        auto jal = make_inst(0, "jal x1, LOOP");
        check("jal: nenhum destino é registrado (dest_registers vazio)",
            jal->GetDestRegisters().empty());
        check("jal: x1 aparece em source_registers em vez de dest_registers",
            jal->GetSourceRegisters().size() == 1 && jal->GetSourceRegisters()[0].GetId() == 1);
    }

    secao("7.3 Offsets/imediatos com sinal ou hexadecimal continuam excluídos da fonte");
    {
        auto neg = make_inst(1, "lw x5, -4(x6)");
        check("offset negativo não vira fonte", neg->GetSourceRegisters().size() == 1);
        check("source[0] ainda é x6 (base)", neg->GetSourceRegisters()[0].GetId() == 6);

        auto hex = make_inst(2, "lw x5, 0x10(x6)");
        check("offset hexadecimal não vira fonte", hex->GetSourceRegisters().size() == 1);
    }

    // NOTA: este RegisterTable só cadastra nomes numéricos ('x0'..'x31',
    // 'f0'..'f31') — diferente do MIPS32, que tem uma tabela rica de aliases
    // ABI ('$ra', '$sp', '$t0', ...). Usar um nome ABI aqui (ex.: "ra") faz
    // o LookupRegister abortar o programa (ver bloco [ABORT] comentado
    // na seção 1).

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
