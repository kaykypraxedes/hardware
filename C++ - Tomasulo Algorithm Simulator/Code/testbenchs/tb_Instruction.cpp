// ──────────────────────────────────────────────────────────────────────────
//  tb_Instruction.cpp  —  Testbench isolado de Instruction.cpp
//  Compile: g++ -o tb_Instrucao tb_Instrucao.cpp ../Componentes.cpp ../Instrucao.cpp
// ──────────────────────────────────────────────────────────────────────────
#include "../headers/Instruction.h"
#include "../headers/Components.h"
#include "tb_helpers.h"

using namespace processor;

int main() {

    secao("Instruction() — construtor padrão");
    {
        Instruction i;
        check("GetPC() == -1",                         i.GetPC() == -1);
        check("GetInstructionType() == NONEXISTENT",   i.GetInstructionType() == INSTRUCTION_TYPE::NONEXISTENT);
        check("GetExLatency() == 0",                   i.GetExLatency() == 0);
        check("GetMemLatency() == 0",                  i.GetMemLatency() == 0);
    }

    secao("GetPC() e GetInstructionString()");
    {
        Instruction i(7, "ADD R1, R2, R3");
        check("GetPC() == 7",                          i.GetPC() == 7);
        check("GetInstructionString() == 'ADD R1, R2, R3'",
              i.GetInstructionString() == "ADD R1, R2, R3");
    }

    secao("Identificação de tipo e latências — LOAD");
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

    secao("Identificação de tipo e latências — STORE");
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

    secao("Identificação de tipo e latências — INT_BASIC");
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

    secao("Identificação de tipo e latências — BRANCH");
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

    secao("Identificação de tipo e latências — INT_MUL");
    {
        Instruction i(5, "MUL R3, R1, R2");
        check("MUL: tipo == INT_MUL",        i.GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("MUL: exLatency == 4",         i.GetExLatency() == 4);
    }

    secao("Identificação de tipo e latências — INT_DIV");
    {
        Instruction i(6, "DIV R3, R1, R2");
        check("DIV: tipo == INT_DIV",        i.GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("DIV: exLatency == 10",        i.GetExLatency() == 10);
    }

    secao("Identificação de tipo e latências — FLOAT_BASIC");
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

    secao("Identificação de tipo e latências — FLOAT_MUL");
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

    secao("Identificação de tipo e latências — FLOAT_DIV");
    {
        Instruction i(9, "DIV.D F4, F2, F0");
        check("DIV.D: tipo == FLOAT_DIV",    i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("DIV.D: exLatency == 40",      i.GetExLatency() == 40);
    }

    secao("Instrução desconhecida -> NONEXISTENT");
    {
        Instruction i(10, "XPTO R1, R2, R3");
        check("XPTO: tipo == NONEXISTENT",  i.GetInstructionType() == INSTRUCTION_TYPE::NONEXISTENT);
        check("XPTO: exLatency == 0",        i.GetExLatency() == 0);
    }

    secao("SetExLatency / SetMemLatency");
    {
        Instruction i(11, "L.D F0, 0(R0)");
        check("antes: exLat == 1",  i.GetExLatency()  == 1);
        check("antes: memLat == 1", i.GetMemLatency() == 1);
        i.SetExLatency(5);
        i.SetMemLatency(3);
        check("depois: exLat == 5",  i.GetExLatency()  == 5);
        check("depois: memLat == 3", i.GetMemLatency() == 3);
    }

    secao("base_ex_latencies (vetor estático)");
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

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
