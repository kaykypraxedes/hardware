// ──────────────────────────────────────────────────────────────────────────
//  tb_Instruction.cpp  —  Testbench isolado do módulo Instruction
//  Compile: g++ -o tb_Instruction tb_Instruction.cpp ../Components.cpp ../Instruction.cpp ../Instruction/InstructionMips32.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Instruction.h"
#include "../Instruction/headers/InstructionMips32.h"
#include "../headers/InstructionFactory.h"
#include "tb_helpers.h"
#include <memory>
#include <vector>

using namespace processor;

// Público:
// Helper do testbench: monta uma instrução MIPS32 na posição 'position' via
// InstructionFactory (MESMO caminho que a Thread usa). As linhas "dummy"
// anteriores são necessárias porque a Factory atribui a posição pelo índice
// da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> linhas;
    for (int p = 0; p < position; p++)
        linhas.push_back("ADD R0, R0, R0"); // dummy: apenas ocupa a posição
    linhas.push_back(line);
    std::vector<std::unique_ptr<Instruction>> parsed =
        InstructionFactory::ParseTrace(linhas, ARCHITECTURE::MIPS_32);
    return std::shared_ptr<Instruction>(std::move(parsed[position]));
}

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO BÁSICO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO BÁSICO");

    secao("1.1 Instruction() — construtor padrão (via InstructionMips32, que é concreta)");
    {
        InstructionMips32 i;
        check("GetPosition() == -1",                   i.GetPosition() == -1);
        check("GetInstructionType() == INVALID",       i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",                   i.GetExLatency() == 0);
        check("GetMemLatency() == 0",                  i.GetMemLatency() == 0);
    }

    secao("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "ADD R1, R2, R3");
        check("GetPosition() == 7",                            i->GetPosition() == 7);
        check("GetInstructionString() == 'ADD    R1, R2, R3'", i->GetInstructionString() == "ADD    R1, R2, R3");
    }

    secao("1.3 InstructionFactory — arquitetura de trace (MIPS32 por ora)");
    {
        std::vector<std::string> trace = {"ADD R1, R2, R3", "L.D F2, 0(R1)"};
        auto parsed = InstructionFactory::ParseTrace(trace, ARCHITECTURE::MIPS_32);
        check("2 instruções parseadas",            parsed.size() == 2);
        check("posição 0 == 0",                    parsed[0]->GetPosition() == 0);
        check("posição 1 == 1",                    parsed[1]->GetPosition() == 1);
        check("posição 0 é INT_BASIC",             parsed[0]->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("posição 1 é LOAD",                  parsed[1]->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    secao("[ABORT] String vazia deve abortar");
    {
        InstructionMips32 i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionMips32 i(10);
        i.Parse("XPTO R1, R2, R3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    secao("2.1 LOAD (L.D)");
    {
        auto i = make_inst(0, "L.D F2, 0(R1)");
        check("L.D: tipo == LOAD",              i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("L.D: exLatency  == 1",           i->GetExLatency()  == 1);
        check("L.D: memLatency == 1",           i->GetMemLatency() == 1);
        check("L.D: dest[0] tipo='F'",          i->GetDestRegisters()[0].GetType() == 'F');
        check("L.D: dest[0] id=34 (F2 global)", i->GetDestRegisters()[0].GetId()   == 34);
        check("L.D: 1 destino",                 i->GetDestRegisters().size() == 1);
        check("L.D: source[0] tipo='R'",        i->GetSourceRegisters()[0].GetType() == 'R');
        check("L.D: source[0] id=1",            i->GetSourceRegisters()[0].GetId()   == 1);
        check("L.D: 1 fonte",                   i->GetSourceRegisters().size() == 1);
    }

    secao("2.2 STORE (S.D)");
    {
        auto i = make_inst(1, "S.D F6, 0(R2)");
        check("S.D: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("S.D: exLatency  == 1",               i->GetExLatency()  == 1);
        check("S.D: memLatency == 1",               i->GetMemLatency() == 1);
        check("S.D: sem destino",                   i->GetDestRegisters().empty());
        check("S.D: source[0] tipo='F' (dado)",     i->GetSourceRegisters()[0].GetType() == 'F');
        check("S.D: source[0] id=38 (F6 global)",   i->GetSourceRegisters()[0].GetId()   == 38);
        check("S.D: source[1] tipo='R' (endereço)", i->GetSourceRegisters()[1].GetType() == 'R');
        check("S.D: source[1] id=2",                i->GetSourceRegisters()[1].GetId()   == 2);
    }

    secao("2.3 INT_BASIC (DADDIU, ADD)");
    {
        auto i = make_inst(2, "DADDIU R1, R1, #8");
        check("DADDIU: tipo == INT_BASIC",       i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("DADDIU: exLatency == 1",          i->GetExLatency() == 1);
        check("DADDIU: dest[0] tipo='R'",        i->GetDestRegisters()[0].GetType() == 'R');
        check("DADDIU: dest[0] id=1",            i->GetDestRegisters()[0].GetId()   == 1);
        check("DADDIU: source[0] tipo='R'",      i->GetSourceRegisters()[0].GetType() == 'R');
        check("DADDIU: source[0] id=1",          i->GetSourceRegisters()[0].GetId()   == 1);
        check("DADDIU: imediato não vira fonte", i->GetSourceRegisters().size() == 1);

        auto add = make_inst(3, "ADD R3, R1, R2");
        check("ADD: dest[0] id=3",   add->GetDestRegisters()[0].GetId() == 3);
        check("ADD: source[0] id=1", add->GetSourceRegisters()[0].GetId() == 1);
        check("ADD: source[1] id=2", add->GetSourceRegisters()[1].GetId() == 2);
    }

    secao("2.4 INT_MUL e INT_DIV (MUL, DIV)");
    {
        auto mul = make_inst(5, "MUL R3, R1, R2");
        check("MUL: tipo == INT_MUL",        mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("MUL: exLatency == 4",         mul->GetExLatency() == 4);

        auto div = make_inst(6, "DIV R3, R1, R2");
        check("DIV: tipo == INT_DIV",        div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("DIV: exLatency == 10",        div->GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    secao("3.1 BNEZ e BEQ — operandos padrão");
    {
        auto i = make_inst(4, "BNEZ R3, foo");
        check("BNEZ: tipo == BRANCH",        i->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("BNEZ: exLatency == 1",        i->GetExLatency() == 1);
        check("BNEZ: sem destino",           i->GetDestRegisters().empty());
        check("BNEZ: source[0] tipo='R'",    i->GetSourceRegisters()[0].GetType() == 'R');
        check("BNEZ: source[0] id=3",        i->GetSourceRegisters()[0].GetId()   == 3);
        check("BNEZ: label não vira fonte",  i->GetSourceRegisters().size() == 1);

        auto beq = make_inst(5, "BEQ R1, R2, label");
        check("BEQ: source[0] tipo='R'", beq->GetSourceRegisters()[0].GetType() == 'R');
        check("BEQ: source[0] id=1",     beq->GetSourceRegisters()[0].GetId()   == 1);
        check("BEQ: source[1] tipo='R'", beq->GetSourceRegisters()[1].GetType() == 'R');
        check("BEQ: source[1] id=2",     beq->GetSourceRegisters()[1].GetId()   == 2);
    }

    secao("3.2 Outros opcodes: JR, J, BGTZ");
    {
        auto jr = make_inst(0, "JR R1");
        check("JR: source[0] tipo='R'",   jr->GetSourceRegisters()[0].GetType() == 'R');

        auto j = make_inst(1, "J loop");
        check("J: sem fontes (só label)", j->GetSourceRegisters().empty());

        auto bgtz = make_inst(2, "BGTZ R4, done");
        check("BGTZ: source[0] id=4",     bgtz->GetSourceRegisters()[0].GetId() == 4);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    secao("4.1 FLOAT_BASIC (ADD.D)");
    {
        auto i = make_inst(7, "ADD.D F6, F4, F6");
        check("ADD.D: tipo == FLOAT_BASIC",         i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("ADD.D: exLatency == 9",              i->GetExLatency() == 9);
        check("ADD.D: dest[0] tipo='F'",            i->GetDestRegisters()[0].GetType() == 'F');
        check("ADD.D: dest[0] id=38 (F6 global)",   i->GetDestRegisters()[0].GetId()   == 38);
        check("ADD.D: source[0] tipo='F'",          i->GetSourceRegisters()[0].GetType() == 'F');
        check("ADD.D: source[0] id=36 (F4 global)", i->GetSourceRegisters()[0].GetId()   == 36);
        check("ADD.D: source[1] tipo='F'",          i->GetSourceRegisters()[1].GetType() == 'F');
        check("ADD.D: source[1] id=38 (F6 global)", i->GetSourceRegisters()[1].GetId()   == 38);
    }

    secao("4.2 FLOAT_MUL (MUL.D)");
    {
        auto i = make_inst(8, "MUL.D F4, F2, F0");
        check("MUL.D: tipo == FLOAT_MUL",           i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("MUL.D: exLatency == 14",             i->GetExLatency() == 14);
        check("MUL.D: dest[0] tipo='F'",            i->GetDestRegisters()[0].GetType() == 'F');
        check("MUL.D: dest[0] id=36 (F4 global)",   i->GetDestRegisters()[0].GetId()   == 36);
        check("MUL.D: source[0] tipo='F'",          i->GetSourceRegisters()[0].GetType() == 'F');
        check("MUL.D: source[0] id=34 (F2 global)", i->GetSourceRegisters()[0].GetId()   == 34);
        check("MUL.D: source[1] tipo='F'",          i->GetSourceRegisters()[1].GetType() == 'F');
        check("MUL.D: source[1] id=32 (F0 global)", i->GetSourceRegisters()[1].GetId()   == 32);
    }

    secao("4.3 FLOAT_DIV (DIV.D)");
    {
        auto i = make_inst(9, "DIV.D F4, F2, F0");
        check("DIV.D: tipo == FLOAT_DIV", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("DIV.D: exLatency == 40",   i->GetExLatency() == 40);
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LATÊNCIAS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LATÊNCIAS");

    secao("5.1 base_ex_latencies / base_mem_latencies — tabelas estáticas");
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

    secao("5.2 SetExLatency / SetMemLatency");
    {
        auto i = make_inst(11, "L.D F0, 0(R0)");
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
        auto i1 = make_inst(0, "add r1, r2, r3");
        check("lowercase -> uppercase", i1->GetInstructionString() ==     "ADD    R1, R2, R3");

        auto i2 = make_inst(1, "   l.d     f2 ,    0(r1)   ");
        check("espacos extras + tabs", i2->GetInstructionString() ==      "L.D    F2, 0(R1)");

        auto i3 = make_inst(2, "ADD R1 R2 R3"); // sem vírgulas
        check("apenas espaços -> vírgulas", i3->GetInstructionString() == "ADD    R1, R2, R3");

        auto i4 = make_inst(3, "SW R1 4(R2)");
        check("STORE sem vírgula", i4->GetInstructionString() ==          "SW     R1, 4(R2)");
    }

    secao("6.2 BRANCH — normalização de labels");
    {
        auto i1 = make_inst(0, "BNEZ r3, LOOP");
        check("label vira minúsculo", i1->GetInstructionString() ==       "BNEZ   R3, loop");

        auto i2 = make_inst(1, "J END");
        check("J label vira minúsculo", i2->GetInstructionString() ==     "J      end");

        // Este é o caso que expõe o bug do SetAttributes:
        auto i3 = make_inst(2, "BEQZ R2, retry");
        check("label 'retry' (começa com R) não vira Register", i3->GetSourceRegisters()[0].GetType() == 'R');
        check("label 'retry' não vira fonte",                   i3->GetSourceRegisters().size() == 1);
    }

    secao("6.3 Opcode minúsculo reconhecido (mul.d)");
    {
        auto i = make_inst(0, "mul.d f4, f2, f0");
        check("opcode minúsculo é reconhecido", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
