/* tb_InstructionMips32.cpp */
// Testbench isolado do módulo Instruction - Arquitetura MIPS32
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::MIPS_32;

// Helper do testbench: monta uma instrução MIPS32 em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
// - As linhas "dummy" anteriores são necessárias porque a Factory atribui a posição pelo índice da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> lines;
    for (int p = 0; p < position; p++)
        lines.push_back("add $t0, $t0, $t0"); // dummy: apenas ocupa a posição
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

    section("1.1 Instruction() — construtor padrão (via InstructionMips32, que é concreta)");
    {
        InstructionMips32 i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    section("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add $t1, $t2, $t3");
        check("GetPosition() == 7",                                 i->GetPosition() == 7);
        check("GetInstructionString() == 'add    $t1, $t2, $t3'",   i->GetInstructionString() == "add    $t1, $t2, $t3");
    }

    section("1.3 InstructionFactory — arquitetura de trace (MIPS32)");
    {
        std::vector<std::string> trace = {"add $t1, $t2, $t3", "l.d $f2, 0($t1)"};
        auto parsed = InstructionFactory::ParseTrace(trace, ARCH);
        check("2 instruções parseadas",  parsed.size() == 2);
        check("posição 0 == 0",          parsed[0]->GetPosition() == 0);
        check("posição 1 == 1",          parsed[1]->GetPosition() == 1);
        check("posição 0 é INT_BASIC",   parsed[0]->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("posição 1 é LOAD",        parsed[1]->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] String vazia deve abortar");
    {
        InstructionMips32 i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionMips32 i(10);
        i.Parse("xpto $t1, $t2, $t3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Registrador inválido deve abortar");
    {
        InstructionMips32 i(11);
        i.Parse("add $zzz, $t1, $t2");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Load/store truncado (sem base) deve abortar");
    {
        InstructionMips32 i(12);
        i.Parse("lw $t0, 4");
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
        auto i = make_inst(0, "lw $t0, 4($sp)");
        check("lw: tipo == LOAD",              i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lw: exLatency  == 1",           i->GetExLatency()  == 1);
        check("lw: memLatency == 1",           i->GetMemLatency() == 1);
        check("lw: dest[0] tipo='R'",          i->GetDestRegisters()[0].GetType() == 'R');
        check("lw: dest[0] id=8 ($t0)",        i->GetDestRegisters()[0].GetId()   == 8);
        check("lw: 1 destino",                 i->GetDestRegisters().size() == 1);
        check("lw: source[0] tipo='R'",        i->GetSourceRegisters()[0].GetType() == 'R');
        check("lw: source[0] id=29 ($sp)",     i->GetSourceRegisters()[0].GetId()   == 29);
        check("lw: offset '4' não vira fonte", i->GetSourceRegisters().size() == 1);

        auto iz = make_inst(1, "lw $t1, 0($zero)");
        check("lw com $zero: source[0] id=0", iz->GetSourceRegisters()[0].GetId() == 0);
    }

    section("2.2 STORE (sw)");
    {
        auto i = make_inst(2, "sw $t0, 4($sp)");
        check("sw: tipo == STORE",                  i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("sw: exLatency  == 1",                i->GetExLatency()  == 1);
        check("sw: memLatency == 1",                i->GetMemLatency() == 1);
        check("sw: sem destino",                    i->GetDestRegisters().empty());
        check("sw: source[0] tipo='R' (dado)",      i->GetSourceRegisters()[0].GetType() == 'R');
        check("sw: source[0] id=8 ($t0, dado)",     i->GetSourceRegisters()[0].GetId()   == 8);
        check("sw: source[1] tipo='R' (endereço)",  i->GetSourceRegisters()[1].GetType() == 'R');
        check("sw: source[1] id=29 ($sp, base)",    i->GetSourceRegisters()[1].GetId()   == 29);
    }

    section("2.3 INT_BASIC (daddiu, add)");
    {
        auto i = make_inst(3, "daddiu $t1, $t1, 8");
        check("daddiu: tipo == INT_BASIC",       i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("daddiu: exLatency == 1",          i->GetExLatency() == 1);
        check("daddiu: dest[0] id=9 ($t1)",      i->GetDestRegisters()[0].GetId()   == 9);
        check("daddiu: source[0] id=9 ($t1)",    i->GetSourceRegisters()[0].GetId() == 9);
        check("daddiu: imediato não vira fonte", i->GetSourceRegisters().size() == 1);

        auto add = make_inst(4, "add $v0, $a0, $a1");
        check("add: dest[0] id=2 ($v0)",   add->GetDestRegisters()[0].GetId() == 2);
        check("add: source[0] id=4 ($a0)", add->GetSourceRegisters()[0].GetId() == 4);
        check("add: source[1] id=5 ($a1)", add->GetSourceRegisters()[1].GetId() == 5);
    }

    section("2.4 INT_MUL e INT_DIV (mul, div)");
    {
        auto mul = make_inst(5, "mul $t3, $t1, $t2");
        check("mul: tipo == INT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mul: exLatency == 4",  mul->GetExLatency() == 4);
        check("mul: dest id=11 ($t3)",   mul->GetDestRegisters()[0].GetId() == 11);
        check("mul: source[0] id=9",     mul->GetSourceRegisters()[0].GetId() == 9);
        check("mul: source[1] id=10",    mul->GetSourceRegisters()[1].GetId() == 10);

        auto div = make_inst(6, "div $t3, $t1, $t2");
        check("div: tipo == INT_DIV", div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("div: exLatency == 10", div->GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    section("3.1 bnez e beq — operandos padrão");
    {
        auto i = make_inst(4, "bnez $t3, foo");
        check("bnez: tipo == BRANCH",       i->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("bnez: exLatency == 1",       i->GetExLatency() == 1);
        check("bnez: sem destino",          i->GetDestRegisters().empty());
        check("bnez: source[0] id=11",      i->GetSourceRegisters()[0].GetId() == 11);
        check("bnez: label não vira fonte", i->GetSourceRegisters().size() == 1);

        auto beq = make_inst(5, "beq $t1, $t2, label");
        check("beq: source[0] id=9",  beq->GetSourceRegisters()[0].GetId() == 9);
        check("beq: source[1] id=10", beq->GetSourceRegisters()[1].GetId() == 10);
    }

    section("3.2 Outros opcodes: jr, j, bgtz");
    {
        auto jr = make_inst(0, "jr $ra");
        check("jr: source[0] id=31 ($ra)", jr->GetSourceRegisters()[0].GetId() == 31);

        auto j = make_inst(1, "j loop");
        check("j: sem fontes (só label)", j->GetSourceRegisters().empty());

        auto bgtz = make_inst(2, "bgtz $a0, done");
        check("bgtz: source[0] id=4 ($a0)", bgtz->GetSourceRegisters()[0].GetId() == 4);
    }

    section("3.3 jal/jalr/bltzal — $ra (link register) como destino");
    {
        auto jal = make_inst(3, "jal FUNC");
        check("jal: tipo == BRANCH", jal->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("jal: dest[0] id=31 ($ra)", jal->GetDestRegisters()[0].GetId() == 31);
        check("jal: sem fontes (label não é registrador)", jal->GetSourceRegisters().empty());

        auto bltzal = make_inst(4, "bltzal $a0, done");
        check("bltzal: dest $ra (id 31)", has_reg(bltzal->GetDestRegisters(), 'R', 31));
        check("bltzal: fonte $a0 (id 4)", has_reg(bltzal->GetSourceRegisters(), 'R', 4));

        // jalr com 1 operando: retorno implícito em $ra.
        auto jalr1 = make_inst(5, "jalr $t9");
        check("jalr $t9: dest $ra (id 31)", has_reg(jalr1->GetDestRegisters(), 'R', 31));
        check("jalr $t9: fonte $t9 (id 25)", has_reg(jalr1->GetSourceRegisters(), 'R', 25));

        // jalr com rd explícito: retorno em $rd, salto para $rs.
        auto jalr2 = make_inst(6, "jalr $t0, $t9");
        check("jalr $t0,$t9: dest $t0 (id 8)", has_reg(jalr2->GetDestRegisters(), 'R', 8));
        check("jalr $t0,$t9: fonte $t9 (id 25)", has_reg(jalr2->GetSourceRegisters(), 'R', 25));
        check("jalr $t0,$t9: $ra não é destino", !has_reg(jalr2->GetDestRegisters(), 'R', 31));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    section("4.1 FLOAT_BASIC (add.d)");
    {
        auto i = make_inst(7, "add.d $f6, $f4, $f6");
        check("add.d: tipo == FLOAT_BASIC",      i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("add.d: exLatency == 9",           i->GetExLatency() == 9);
        check("add.d: dest[0] id=38 ($f6)",      i->GetDestRegisters()[0].GetId()   == 38);
        check("add.d: source[0] id=36 ($f4)",    i->GetSourceRegisters()[0].GetId() == 36);
        check("add.d: source[1] id=38 ($f6)",    i->GetSourceRegisters()[1].GetId() == 38);
    }

    section("4.2 FLOAT_MUL (mul.d)");
    {
        auto i = make_inst(8, "mul.d $f4, $f2, $f0");
        check("mul.d: tipo == FLOAT_MUL",     i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("mul.d: exLatency == 14",       i->GetExLatency() == 14);
        check("mul.d: dest[0] id=36 ($f4)",   i->GetDestRegisters()[0].GetId()   == 36);
        check("mul.d: source[0] id=34 ($f2)", i->GetSourceRegisters()[0].GetId() == 34);
        check("mul.d: source[1] id=32 ($f0)", i->GetSourceRegisters()[1].GetId() == 32);
    }

    section("4.3 FLOAT_DIV (div.d)");
    {
        auto i = make_inst(9, "div.d $f4, $f2, $f0");
        check("div.d: tipo == FLOAT_DIV", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("div.d: exLatency == 40",   i->GetExLatency() == 40);
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
        auto i = make_inst(11, "l.d $f0, 0($zero)");
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
        auto i1 = make_inst(0, "ADD $T1, $T2, $T3"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==        "add    $t1, $t2, $t3");

        auto i2 = make_inst(1, "   l.d     $f2 ,    0($t1)   ");
        check("espacos extras + tabs", i2->GetInstructionString() ==         "l.d    $f2, 0($t1)");

        auto i3 = make_inst(2, "ADD $T1 $T2 $T3"); // Sem vírgulas
        check("uppercase + apenas espaços -> lowercase + vírgulas",
            i3->GetInstructionString() ==                                    "add    $t1, $t2, $t3");

        auto i4 = make_inst(3, "SW $T1 4($T2)");
        check("uppercase + STORE sem vírgula", i4->GetInstructionString() == "sw     $t1, 4($t2)");
    }

    section("6.2 BRANCH — normalização de labels e registradores mistos");
    {
        auto i1 = make_inst(0, "bnez $t3, LOOP");
        check("label se mantém", i1->GetInstructionString() ==               "bnez   $t3, LOOP");

        auto i2 = make_inst(1, "J END");
        check("J lowercase + label igual", i2->GetInstructionString() ==     "j      END");

        auto i3 = make_inst(2, "BEQ $T1, $T2, Label");
        check("registradores minúsculos, label preservado",
            i3->GetInstructionString() ==                                    "beq    $t1, $t2, Label");
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. CASOS ESPECÍFICOS DA ARQUITETURA (MIPS32)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. CASOS ESPECÍFICOS — MIPS32");

    section("7.1 Aliases apontam para o mesmo registrador físico ($8 == $t0, $0 == $zero)");
    {
        auto i = make_inst(0, "add $8, $t0, $0");
        check("$8 e $t0 == mesmo id (8)",
            i->GetDestRegisters()[0].GetId() == 8 && i->GetSourceRegisters()[0].GetId() == 8);
        check("$0 == $zero (id 0)", i->GetSourceRegisters()[1].GetId() == 0);
    }

    section("7.2 '$' é obrigatório para virar registrador — imediatos numéricos nunca colidem");
    {
        auto i = make_inst(1, "addi $t1, $t1, 100");
        check("imediato '100' (sem '$') não vira fonte", i->GetSourceRegisters().size() == 1);
    }

    section("7.3 lui — só dest, nenhuma fonte (imediato de 16 bits, não é '$')");
    {
        auto i = make_inst(2, "lui $t0, 4097");
        check("lui: 1 destino",  i->GetDestRegisters().size() == 1);
        check("lui: 0 fontes",   i->GetSourceRegisters().empty());
    }

    section("7.4 Grupos de aliases vizinhos não colidem ($t8/$t9 vs $k0/$k1)");
    {
        auto i = make_inst(3, "add $t8, $k0, $t9");
        check("$t8 -> id 24", i->GetDestRegisters()[0].GetId()   == 24);
        check("$k0 -> id 26", i->GetSourceRegisters()[0].GetId() == 26);
        check("$t9 -> id 25", i->GetSourceRegisters()[1].GetId() == 25);
    }

    section("7.5 Label parecido com nome de registrador (sem '$') NÃO é confundido");
    {
        // Diferente de ARM64/RISC-V/Simplified (que usam padrão letra+dígito),
        // o MIPS32 exige o prefixo '$' para reconhecer um registrador — então
        // um label chamado 't1' (igual ao nome do registrador, só sem o '$')
        // é tratado com segurança como label.
        auto i = make_inst(4, "beqz $a0, t1");
        check("source[0] só $a0",       i->GetSourceRegisters().size() == 1);
        check("source[0] id=4 ($a0)",   i->GetSourceRegisters()[0].GetId() == 4);
    }

    section("7.6 Labels locais do assembler ('$L2') NÃO são registradores");
    {
        // gas/gcc geram labels locais como $L2/$LC0 — começam com '$' mas não
        // estão na RegisterTable: não viram fonte nem corrompem o case.
        auto i = make_inst(0, "beq $t0, $t1, $L2");
        check("beq c/ label $L2: 2 fontes", i->GetSourceRegisters().size() == 2);
        check("fonte $t0 (id 8)",  has_reg(i->GetSourceRegisters(), 'R', 8));
        check("fonte $t1 (id 9)",  has_reg(i->GetSourceRegisters(), 'R', 9));
        check("label '$L2' preservado na string",
            i->GetInstructionString() == "beq    $t0, $t1, $L2");

        auto up = make_inst(1, "BEQ $T0, $T1, $L2");
        check("maiúsculo: registradores minúsculos + label preservado",
            up->GetInstructionString() == "beq    $t0, $t1, $L2");
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
