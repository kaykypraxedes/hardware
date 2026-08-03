// ──────────────────────────────────────────────────────────────────────────
//  tb_Instruction.cpp  —  Testbench isolado de Instruction.cpp
//  Compile: g++ -o tb_Instruction tb_Instruction.cpp ../Components.cpp ../Instruction.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Instruction.h"
#include "../headers/Components.h"
#include "tb_helpers.h"

using namespace processor;

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO E ESTADO BÁSICO
    // ════════════════════════════════════════════════════════════════════

    print_title("1. CONSTRUÇÃO E ESTADO BÁSICO");

    secao("1.1 Instruction() — construtor padrão");
    {
        Instruction i;
        check("GetPosition() == -1",                   i.GetPosition() == -1);
        check("GetInstructionType() == NONEXISTENT",   i.GetInstructionType() == INSTRUCTION_TYPE::NONEXISTENT);
        check("GetExLatency() == 0",                   i.GetExLatency() == 0);
        check("GetMemLatency() == 0",                  i.GetMemLatency() == 0);
    }

    secao("1.2 Construtor(posição, string) — GetPosition e GetInstructionString");
    {
        Instruction i(7, "ADD R1, R2, R3");
        check("GetPosition() == 7",                         i.GetPosition() == 7);
        check("GetInstructionString() == 'ADD    R1, R2, R3'", i.GetInstructionString() == "ADD    R1, R2, R3");
    }

    secao("1.3 String vazia — position vira -1");
    {
        Instruction i(5, "");
        check("position vira -1 se string vazia", i.GetPosition() == -1);
    }

    /*
    // Teste das flags de segurança do programa (abortam a execução):

    secao("[ABORT] Instrução desconhecida deve abortar");
    {
        Instruction i(10, "XPTO R1, R2, R3");
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
        Instruction i(0, "L.D F2, 0(R1)");
        check("L.D: tipo == LOAD",           i.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("L.D: exLatency  == 1",        i.GetExLatency()  == 1);
        check("L.D: memLatency == 1",        i.GetMemLatency() == 1);
        check("L.D: dest tipo='F'",          i.GetDestRegister().GetType() == 'F');
        check("L.D: dest id=2",              i.GetDestRegister().GetId()   == 2);
        check("L.D: J tipo='Z' (sem J)",     i.GetJ().GetType() == 'Z');
        check("L.D: K tipo='R'",             i.GetK().GetType() == 'R');
        check("L.D: K id=1",                 i.GetK().GetId()   == 1);
    }

    secao("2.2 STORE (S.D)");
    {
        Instruction i(1, "S.D F6, 0(R2)");
        check("S.D: tipo == STORE",          i.GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("S.D: exLatency  == 1",        i.GetExLatency()  == 1);
        check("S.D: memLatency == 1",        i.GetMemLatency() == 1);
        check("S.D: dest tipo='Z' (sem dest)", i.GetDestRegister().GetType() == 'Z');
        check("S.D: J tipo='F'",             i.GetJ().GetType() == 'F');
        check("S.D: J id=6",                 i.GetJ().GetId()   == 6);
        check("S.D: K tipo='R'",             i.GetK().GetType() == 'R');
        check("S.D: K id=2",                 i.GetK().GetId()   == 2);
    }

    secao("2.3 INT_BASIC (DADDIU, ADD)");
    {
        Instruction i(2, "DADDIU R1, R1, #8");
        check("DADDIU: tipo == INT_BASIC",   i.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("DADDIU: exLatency == 1",      i.GetExLatency() == 1);
        check("DADDIU: dest tipo='R'",       i.GetDestRegister().GetType() == 'R');
        check("DADDIU: dest id=1",           i.GetDestRegister().GetId()   == 1);
        check("DADDIU: J tipo='R'",          i.GetJ().GetType() == 'R');
        check("DADDIU: J id=1",              i.GetJ().GetId()   == 1);
        check("DADDIU: K tipo='Z' (imediato)", i.GetK().GetType() == 'Z');

        Instruction add(3, "ADD R3, R1, R2");
        check("ADD: dest id=3",  add.GetDestRegister().GetId() == 3);
        check("ADD: J id=1",     add.GetJ().GetId()          == 1);
        check("ADD: K id=2",     add.GetK().GetId()          == 2);
    }

    secao("2.4 INT_MUL e INT_DIV (MUL, DIV)");
    {
        Instruction mul(5, "MUL R3, R1, R2");
        check("MUL: tipo == INT_MUL",        mul.GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("MUL: exLatency == 4",         mul.GetExLatency() == 4);

        Instruction div(6, "DIV R3, R1, R2");
        check("DIV: tipo == INT_DIV",        div.GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("DIV: exLatency == 10",        div.GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    secao("3.1 BNEZ e BEQ — operandos padrão");
    {
        Instruction i(4, "BNEZ R3, foo");
        check("BNEZ: tipo == BRANCH",        i.GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("BNEZ: exLatency == 1",        i.GetExLatency() == 1);
        check("BNEZ: dest tipo='Z'",         i.GetDestRegister().GetType() == 'Z');
        check("BNEZ: J tipo='R'",            i.GetJ().GetType() == 'R');
        check("BNEZ: J id=3",                i.GetJ().GetId()   == 3);
        check("BNEZ: K tipo='Z' (label)",    i.GetK().GetType() == 'Z');

        Instruction beq(5, "BEQ R1, R2, label");
        check("BEQ: J tipo='R'", beq.GetJ().GetType() == 'R');
        check("BEQ: J id=1",     beq.GetJ().GetId()   == 1);
        check("BEQ: K tipo='R'", beq.GetK().GetType() == 'R');
        check("BEQ: K id=2",     beq.GetK().GetId()   == 2);
    }

    secao("3.2 Outros opcodes: JR, J, BGTZ");
    {
        Instruction jr(0, "JR R1");
        check("JR: J tipo='R'", jr.GetJ().GetType() == 'R');

        Instruction j(1, "J loop");
        check("J: J tipo='Z' (sem registrador)", j.GetJ().GetType() == 'Z');

        Instruction bgtz(2, "BGTZ R4, done");
        check("BGTZ: J id=4", bgtz.GetJ().GetId() == 4);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE");

    secao("4.1 FLOAT_BASIC (ADD.D)");
    {
        Instruction i(7, "ADD.D F6, F4, F6");
        check("ADD.D: tipo == FLOAT_BASIC", i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("ADD.D: exLatency == 9",      i.GetExLatency() == 9);
        check("ADD.D: dest tipo='F'",        i.GetDestRegister().GetType() == 'F');
        check("ADD.D: dest id=6",            i.GetDestRegister().GetId()   == 6);
        check("ADD.D: J tipo='F'",           i.GetJ().GetType() == 'F');
        check("ADD.D: J id=4",               i.GetJ().GetId()   == 4);
        check("ADD.D: K tipo='F'",           i.GetK().GetType() == 'F');
        check("ADD.D: K id=6",               i.GetK().GetId()   == 6);
    }

    secao("4.2 FLOAT_MUL (MUL.D)");
    {
        Instruction i(8, "MUL.D F4, F2, F0");
        check("MUL.D: tipo == FLOAT_MUL",    i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("MUL.D: exLatency == 14",      i.GetExLatency() == 14);
        check("MUL.D: dest tipo='F'",        i.GetDestRegister().GetType() == 'F');
        check("MUL.D: dest id=4",            i.GetDestRegister().GetId()   == 4);
        check("MUL.D: J tipo='F'",           i.GetJ().GetType() == 'F');
        check("MUL.D: J id=2",               i.GetJ().GetId()   == 2);
        check("MUL.D: K tipo='F'",           i.GetK().GetType() == 'F');
        check("MUL.D: K id=0",               i.GetK().GetId()   == 0);
    }

    secao("4.3 FLOAT_DIV (DIV.D)");
    {
        Instruction i(9, "DIV.D F4, F2, F0");
        check("DIV.D: tipo == FLOAT_DIV",    i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("DIV.D: exLatency == 40",      i.GetExLatency() == 40);
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
        Instruction i(11, "L.D F0, 0(R0)");
        check("antes: exLat == 1",  i.GetExLatency()  == 1);
        check("antes: memLat == 1", i.GetMemLatency() == 1);
        i.SetExLatency(5);
        i.SetMemLatency(3);
        check("depois: exLat == 5",  i.GetExLatency()  == 5);
        check("depois: memLat == 3", i.GetMemLatency() == 3);
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. NORMALIZAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. NORMALIZAÇÃO");

    secao("6.1 NormalizeInstruction — casos variados");
    {
        Instruction i1(0, "add r1, r2, r3");
        check("lowercase -> uppercase", i1.GetInstructionString() ==     "ADD    R1, R2, R3");

        Instruction i2(1, "   l.d     f2 ,    0(r1)   ");
        check("espacos extras + tabs", i2.GetInstructionString() ==      "L.D    F2, 0(R1)");

        Instruction i3(2, "ADD R1 R2 R3"); // sem vírgulas
        check("apenas espaços -> vírgulas", i3.GetInstructionString() == "ADD    R1, R2, R3");

        Instruction i4(3, "SW R1 4(R2)");
        check("STORE sem vírgula", i4.GetInstructionString() ==          "SW     R1, 4(R2)");
    }

    secao("6.2 BRANCH — normalização de labels");
    {
        Instruction i1(0, "BNEZ r3, LOOP");
        check("label vira minúsculo", i1.GetInstructionString() ==       "BNEZ   R3, loop");

        Instruction i2(1, "J END");
        check("J label vira minúsculo", i2.GetInstructionString() ==     "J      end");

        // Este é o caso que expõe o bug do SetAttributes:
        Instruction i3(2, "BEQZ R2, retry");
        check("label 'retry' (começa com R) não vira Register", i3.GetJ().GetType() == 'R');
        check("label 'retry' não afetou K",                     i3.GetK().GetType() == 'Z');
    }

    secao("6.3 Opcode minúsculo reconhecido (mul.d)");
    {
        Instruction i(0, "mul.d f4, f2, f0");
        check("opcode minúsculo é reconhecido", i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
