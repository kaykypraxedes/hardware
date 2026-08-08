/* tb_InstructionSimplified.cpp */
// Testbench isolado do módulo Instruction - Arquitetura Simplificada
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::SIMPLIFIED;

// Helper do testbench: monta uma instrução Mips Simplificada (SIMPLIFIED) em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
// - As linhas "dummy" anteriores são necessárias porque a Factory atribui a posição pelo índice da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> lines;
    for (int p = 0; p < position; p++)
        lines.push_back("add r0, r0, r0"); // dummy: apenas ocupa a posição
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

    section("1.1 Instruction() — construtor padrão (via InstructionSimplified, que é concreta)");
    {
        InstructionSimplified i;
        check("GetPosition() == -1",                   i.GetPosition() == -1);
        check("GetInstructionType() == INVALID",       i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",                   i.GetExLatency() == 0);
        check("GetMemLatency() == 0",                  i.GetMemLatency() == 0);
    }

    section("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add r1, r2, r3");
        check("GetPosition() == 7",                            i->GetPosition() == 7);
        check("GetInstructionString() == 'add    r1, r2, r3'", i->GetInstructionString() == "add    r1, r2, r3");
    }

    section("1.3 InstructionFactory — arquitetura de trace (SIMPLIFIED)");
    {
        std::vector<std::string> trace = {"add r1, r2, r3", "l.d f2, 0(r1)"};
        auto parsed = InstructionFactory::ParseTrace(trace, ARCH);
        check("2 instruções parseadas",            parsed.size() == 2);
        check("posição 0 == 0",                    parsed[0]->GetPosition() == 0);
        check("posição 1 == 1",                    parsed[1]->GetPosition() == 1);
        check("posição 0 é INT_BASIC",             parsed[0]->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("posição 1 é LOAD",                  parsed[1]->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    section("[ABORT] String vazia deve abortar");
    {
        InstructionSimplified i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionSimplified i(10);
        i.Parse("xpto r1, r2, r3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    section("2.1 LOAD (l.d)");
    {
        auto i = make_inst(0, "l.d f2, 0(r1)");
        check("l.d: tipo == LOAD",              i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("l.d: exLatency  == 1",           i->GetExLatency()  == 1);
        check("l.d: memLatency == 1",           i->GetMemLatency() == 1);
        check("l.d: dest[0] tipo='F'",          i->GetDestRegisters()[0].GetType() == 'F');
        check("l.d: dest[0] id=34 (F2 global)", i->GetDestRegisters()[0].GetId()   == 34);
        check("l.d: 1 destino",                 i->GetDestRegisters().size() == 1);
        check("l.d: source[0] tipo='R'",        i->GetSourceRegisters()[0].GetType() == 'R');
        check("l.d: source[0] id=1",            i->GetSourceRegisters()[0].GetId()   == 1);
        check("l.d: 1 fonte",                   i->GetSourceRegisters().size() == 1);
    }

    section("2.2 STORE (s.d)");
    {
        auto i = make_inst(1, "s.d f6, 0(r2)");
        check("s.d: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("s.d: exLatency  == 1",               i->GetExLatency()  == 1);
        check("s.d: memLatency == 1",               i->GetMemLatency() == 1);
        check("s.d: sem destino",                   i->GetDestRegisters().empty());
        check("s.d: source[0] tipo='F' (dado)",     i->GetSourceRegisters()[0].GetType() == 'F');
        check("s.d: source[0] id=38 (F6 global)",   i->GetSourceRegisters()[0].GetId()   == 38);
        check("s.d: source[1] tipo='R' (endereço)", i->GetSourceRegisters()[1].GetType() == 'R');
        check("s.d: source[1] id=2",                i->GetSourceRegisters()[1].GetId()   == 2);
    }

    section("2.3 INT_BASIC (daddiu, add)");
    {
        auto i = make_inst(2, "daddiu r1, r1, #8");
        check("daddiu: tipo == INT_BASIC",       i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("daddiu: exLatency == 1",          i->GetExLatency() == 1);
        check("daddiu: dest[0] tipo='R'",        i->GetDestRegisters()[0].GetType() == 'R');
        check("daddiu: dest[0] id=1",            i->GetDestRegisters()[0].GetId()   == 1);
        check("daddiu: source[0] tipo='R'",      i->GetSourceRegisters()[0].GetType() == 'R');
        check("daddiu: source[0] id=1",          i->GetSourceRegisters()[0].GetId()   == 1);
        check("daddiu: imediato não vira fonte", i->GetSourceRegisters().size() == 1);

        auto add = make_inst(3, "add r3, r1, r2");
        check("add: dest[0] id=3",   add->GetDestRegisters()[0].GetId() == 3);
        check("add: source[0] id=1", add->GetSourceRegisters()[0].GetId() == 1);
        check("add: source[1] id=2", add->GetSourceRegisters()[1].GetId() == 2);
    }

    section("2.4 INT_MUL e INT_DIV (mul, div)");
    {
        auto mul = make_inst(5, "mul r3, r1, r2");
        check("mul: tipo == INT_MUL",        mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mul: exLatency == 4",         mul->GetExLatency() == 4);

        auto div = make_inst(6, "div r3, r1, r2");
        check("div: tipo == INT_DIV",        div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("div: exLatency == 10",        div->GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    section("3.1 bnez e beq — operandos padrão");
    {
        auto i = make_inst(4, "bnez r3, foo");
        check("bnez: tipo == BRANCH",        i->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("bnez: exLatency == 1",        i->GetExLatency() == 1);
        check("bnez: sem destino",           i->GetDestRegisters().empty());
        check("bnez: source[0] tipo='R'",    i->GetSourceRegisters()[0].GetType() == 'R');
        check("bnez: source[0] id=3",        i->GetSourceRegisters()[0].GetId()   == 3);
        check("bnez: label não vira fonte",  i->GetSourceRegisters().size() == 1);

        auto beq = make_inst(5, "beq r1, r2, label");
        check("beq: source[0] tipo='R'", beq->GetSourceRegisters()[0].GetType() == 'R');
        check("beq: source[0] id=1",     beq->GetSourceRegisters()[0].GetId()   == 1);
        check("beq: source[1] tipo='R'", beq->GetSourceRegisters()[1].GetType() == 'R');
        check("beq: source[1] id=2",     beq->GetSourceRegisters()[1].GetId()   == 2);
    }

    section("3.2 Outros opcodes: jr, j, bgtz");
    {
        auto jr = make_inst(0, "jr r1");
        check("jr: source[0] tipo='R'",   jr->GetSourceRegisters()[0].GetType() == 'R');

        auto j = make_inst(1, "j loop");
        check("j: sem fontes (só label)", j->GetSourceRegisters().empty());

        auto bgtz = make_inst(2, "bgtz r4, done");
        check("bgtz: source[0] id=4",     bgtz->GetSourceRegisters()[0].GetId() == 4);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    section("4.1 FLOAT_BASIC (add.d)");
    {
        auto i = make_inst(7, "add.d f6, f4, f6");
        check("add.d: tipo == FLOAT_BASIC",         i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("add.d: exLatency == 9",              i->GetExLatency() == 9);
        check("add.d: dest[0] tipo='F'",            i->GetDestRegisters()[0].GetType() == 'F');
        check("add.d: dest[0] id=38 (f6 global)",   i->GetDestRegisters()[0].GetId()   == 38);
        check("add.d: source[0] tipo='F'",          i->GetSourceRegisters()[0].GetType() == 'F');
        check("add.d: source[0] id=36 (f4 global)", i->GetSourceRegisters()[0].GetId()   == 36);
        check("add.d: source[1] tipo='F'",          i->GetSourceRegisters()[1].GetType() == 'F');
        check("add.d: source[1] id=38 (f6 global)", i->GetSourceRegisters()[1].GetId()   == 38);
    }

    section("4.2 FLOAT_MUL (mul.d)");
    {
        auto i = make_inst(8, "mul.d f4, f2, f0");
        check("mul.d: tipo == FLOAT_MUL",           i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("mul.d: exLatency == 14",             i->GetExLatency() == 14);
        check("mul.d: dest[0] tipo='F'",            i->GetDestRegisters()[0].GetType() == 'F');
        check("mul.d: dest[0] id=36 (f4 global)",   i->GetDestRegisters()[0].GetId()   == 36);
        check("mul.d: source[0] tipo='F'",          i->GetSourceRegisters()[0].GetType() == 'F');
        check("mul.d: source[0] id=34 (f2 global)", i->GetSourceRegisters()[0].GetId()   == 34);
        check("mul.d: source[1] tipo='F'",          i->GetSourceRegisters()[1].GetType() == 'F');
        check("mul.d: source[1] id=32 (f0 global)", i->GetSourceRegisters()[1].GetId()   == 32);
    }

    section("4.3 FLOAT_DIV (div.d)");
    {
        auto i = make_inst(9, "div.d f4, f2, f0");
        check("div.d: tipo == FLOAT_DIV", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("div.d: exLatency == 40",   i->GetExLatency() == 40);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LATÊNCIAS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LATÊNCIAS");

    section("5.1 base_ex_latencies / base_mem_latencies — tabelas estáticas");
    {
        check("latEX[NONEXISTENT]=0",    Instruction::base_ex_latencies[0]  == 0);
        check("latEX[LOAD]=1",           Instruction::base_ex_latencies[1]  == 1);
        check("latEX[INT_MUL]=4",        Instruction::base_ex_latencies[5]  == 4);
        check("latEX[INT_DIV]=10",       Instruction::base_ex_latencies[6]  == 10);
        check("latEX[FLOAT_BASIC]=9",    Instruction::base_ex_latencies[7]  == 9);
        check("latEX[FLOAT_MUL]=14",     Instruction::base_ex_latencies[8]  == 14);
        check("latEX[FLOAT_DIV]=40",     Instruction::base_ex_latencies[9]  == 40);
        check("latMEM[LOAD]=1",          Instruction::base_mem_latencies[0] == 1);
        check("latMEM[STORE]=1",         Instruction::base_mem_latencies[1] == 1);
    }

    section("5.2 SetExLatency / SetMemLatency");
    {
        auto i = make_inst(11, "l.d f0, 0(r0)");
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
        auto i1 = make_inst(0, "ADD R1, R2, R3"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==        "add    r1, r2, r3");

        auto i2 = make_inst(1, "   l.d     f2 ,    0(r1)   ");
        check("espacos extras + tabs", i2->GetInstructionString() ==         "l.d    f2, 0(r1)");

        auto i3 = make_inst(2, "ADD R1 R2 R3"); // Sem vírgulas
        check("uppercase + apenas espaços -> lowercase + vírgulas",
            i3->GetInstructionString() ==                                    "add    r1, r2, r3");

        auto i4 = make_inst(3, "SW R1 4(R2)");
        check("uppercase + STORE sem vírgula", i4->GetInstructionString() == "sw     r1, 4(r2)");
    }

    section("6.2 BRANCH — normalização de labels");
    {
        auto i1 = make_inst(0, "bnez r3, LOOP");
        check("label se mantém", i1->GetInstructionString() ==               "bnez   r3, LOOP");

        auto i2 = make_inst(1, "J END");
        check("J lowercase + label igual", i2->GetInstructionString() ==     "j      END");

        // Este é o caso que expõe o bug do SetAttributes:
        auto i3 = make_inst(2, "beqz r2, retry");
        check("label 'retry' (começa com R) não vira Register", i3->GetSourceRegisters()[0].GetType() == 'R');
        check("label 'retry' não vira fonte",                   i3->GetSourceRegisters().size() == 1);
    }

    section("6.3 [PEGADINHA] Label com prefixo de registrador fora da faixa — case preservado");
    {
        // 'R99' não existe na tabela (faixa r0-31), então NÃO é registrador:
        // não vira fonte e o case do label é preservado (IsRegister table-based).
        auto i = make_inst(0, "beqz r3, R99");
        check("label 'R99' preserva o case (fora da tabela = não é registrador)",
            i->GetInstructionString() == "beqz   r3, R99");
        check("beqz r3, R99: só r3 vira fonte",
            i->GetSourceRegisters().size() == 1 && i->GetSourceRegisters()[0].GetType() == 'R');
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
