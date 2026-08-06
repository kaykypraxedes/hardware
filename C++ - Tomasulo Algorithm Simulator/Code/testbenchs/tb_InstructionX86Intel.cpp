/* tb_InstructionX86Intel.cpp */
// Testbench isolado do módulo Instruction - Arquitetura x86 Intel
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::X86_INTEL;

// Helper do testbench: monta uma instrução x86 Intel em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
// - As linhas "dummy" anteriores são necessárias porque a Factory atribui a posição pelo índice da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> linhas;
    for (int p = 0; p < position; p++)
        linhas.push_back("add eax, eax"); // dummy: apenas ocupa a posição
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

    secao("1.1 Instruction() — construtor padrão (via InstructionX86Intel, que é concreta)");
    {
        InstructionX86Intel i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    secao("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add eax, ebx");
        check("GetPosition() == 7",                             i->GetPosition() == 7);
        check("GetInstructionString() == 'add      eax, ebx'",  i->GetInstructionString() == "add      eax, ebx");
    }

    secao("1.3 InstructionFactory — arquitetura de trace (X86_INTEL)");
    {
        std::vector<std::string> trace = {"add eax, ebx", "mov eax, [ebx]"};
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
        InstructionX86Intel i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    secao("[ABORT] Instrução desconhecida deve abortar");
    {
        InstructionX86Intel i(10);
        i.Parse("xpto eax, ebx");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    // ════════════════════════════════════════════════════════════════════
    // 2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. IDENTIFICAÇÃO DE TIPO — MEMÓRIA E INTEIROS");

    secao("2.1 LOAD (via MOV e via opcode dedicado)");
    {
        auto i = make_inst(0, "mov eax, [ebx+4]");
        check("mov load: tipo == LOAD",             i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("mov load: exLatency  == 1",          i->GetExLatency()  == 1);
        check("mov load: memLatency == 1",          i->GetMemLatency() == 1);
        check("mov load: dest[0] tipo='R'",         i->GetDestRegisters()[0].GetType() == 'R');
        check("mov load: dest[0] id=0 (eax)",       i->GetDestRegisters()[0].GetId()   == 0);
        check("mov load: source[0] tipo='R'",       i->GetSourceRegisters()[0].GetType() == 'R');
        check("mov load: source[0] id=1 (ebx base)",i->GetSourceRegisters()[0].GetId()   == 1);
        check("mov load: offset '+4' não vira fonte extra", i->GetSourceRegisters().size() == 1);
        check("instruction_string preserva colchetes",
            i->GetInstructionString() == "mov      eax, [ebx+4]");

        auto v = make_inst(1, "movss xmm0, [eax]");
        check("movss: tipo == LOAD",     v->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("movss: dest tipo='V'",    v->GetDestRegisters()[0].GetType() == 'V');
        check("movss: dest id=64",       v->GetDestRegisters()[0].GetId()   == 64);
        check("movss: source id=0 (eax)",v->GetSourceRegisters()[0].GetId() == 0);
    }

    secao("2.2 STORE (via MOV)");
    {
        auto i = make_inst(2, "mov [ebx+4], eax");
        check("mov store: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("mov store: exLatency  == 1",                i->GetExLatency()  == 1);
        check("mov store: memLatency == 1",                i->GetMemLatency() == 1);
        check("mov store: sem destino",                    i->GetDestRegisters().empty());
        check("mov store: source[0] tipo='R' (dado)",      i->GetSourceRegisters()[0].GetType() == 'R');
        check("mov store: source[0] id=0 (eax, dado)",     i->GetSourceRegisters()[0].GetId()   == 0);
        check("mov store: source[1] tipo='R' (endereço)",  i->GetSourceRegisters()[1].GetType() == 'R');
        check("mov store: source[1] id=1 (ebx, base)",     i->GetSourceRegisters()[1].GetId()   == 1);
        check("instruction_string preserva colchetes",
            i->GetInstructionString() == "mov      [ebx+4], eax");
    }

    secao("2.3 INT_BASIC — EFLAGS entra como destino e origem inicial (quirk do modelo)");
    {
        auto i = make_inst(3, "add eax, ebx");
        check("add: tipo == INT_BASIC",         i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("add: 2 destinos (eax + EFLAGS)", i->GetDestRegisters().size() == 2);
        check("add: dest[0] id=0 (eax)",        i->GetDestRegisters()[0].GetId()   == 0);
        check("add: dest[1] tipo='G' (EFLAGS)", i->GetDestRegisters()[1].GetType() == 'G');
        check("add: dest[1] id=80",             i->GetDestRegisters()[1].GetId()   == 80);
        check("add: source[0] id=0 (eax reusado como fonte)", i->GetSourceRegisters()[0].GetId() == 0);
        check("add: source[1] id=1 (ebx)",      i->GetSourceRegisters()[1].GetId() == 1);

        auto imm = make_inst(4, "add eax, 5");
        check("add com imediato: só 1 fonte (imediato não resolve)", imm->GetSourceRegisters().size() == 1);
    }

    secao("2.4 INT_MUL e INT_DIV (imul, idiv)");
    {
        auto mul = make_inst(5, "imul eax, ebx");
        check("imul: tipo == INT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("imul: exLatency == 4",  mul->GetExLatency() == 4);

        auto div = make_inst(6, "idiv eax");
        check("idiv: tipo == INT_DIV",        div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("idiv: exLatency == 10",        div->GetExLatency() == 10);
        check("idiv (1 operando): 1 fonte",   div->GetSourceRegisters().size() == 1);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    secao("3.1 BRANCH sempre lê EFLAGS — mesmo desvios incondicionais (quirk do modelo)");
    {
        auto je = make_inst(0, "je end_loop");
        check("je: tipo == BRANCH",         je->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("je: source[0] tipo='G'",     je->GetSourceRegisters()[0].GetType() == 'G');
        check("je: source[0] id=80",        je->GetSourceRegisters()[0].GetId()   == 80);
        check("je: só 1 fonte (EFLAGS)",    je->GetSourceRegisters().size() == 1);

        // jmp é incondicional e, na vida real, não lê EFLAGS — mas o código
        // trata todo BRANCH da mesma forma, então o EFLAGS aparece aqui também.
        auto jmp = make_inst(1, "jmp Loop_1");
        check("jmp (incondicional) também recebe EFLAGS como fonte",
            jmp->GetSourceRegisters().size() == 1 && jmp->GetSourceRegisters()[0].GetId() == 80);
        check("label 'Loop_1' preserva o case original",
            jmp->GetInstructionString() == "jmp      Loop_1");
    }

    secao("3.2 call e ret — sem operandos de registrador, ainda assim EFLAGS aparece");
    {
        auto call = make_inst(2, "call foo");
        check("call: 1 fonte (EFLAGS)", call->GetSourceRegisters().size() == 1);

        auto ret = make_inst(3, "ret");
        check("ret: tipo == BRANCH",     ret->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("ret: 1 fonte (EFLAGS) mesmo sem nenhum operando na string",
            ret->GetSourceRegisters().size() == 1 && ret->GetSourceRegisters()[0].GetId() == 80);
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE (SSE)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE (SSE)");

    secao("4.1 FLOAT_BASIC (addss) — mesmo quirk do EFLAGS como destino");
    {
        auto i = make_inst(4, "addss xmm0, xmm1");
        check("addss: tipo == FLOAT_BASIC",  i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("addss: exLatency == 9",       i->GetExLatency() == 9);
        check("addss: dest[0] tipo='V'",     i->GetDestRegisters()[0].GetType() == 'V');
        check("addss: dest[0] id=64 (xmm0)", i->GetDestRegisters()[0].GetId()   == 64);
        check("addss: dest[1] tipo='G' (EFLAGS, mesmo em SSE)", i->GetDestRegisters()[1].GetType() == 'G');
        check("addss: source[0] id=64 (xmm0 reusado)", i->GetSourceRegisters()[0].GetId() == 64);
        check("addss: source[1] id=65 (xmm1)",         i->GetSourceRegisters()[1].GetId() == 65);
    }

    secao("4.2 FLOAT_MUL (mulss)");
    {
        auto i = make_inst(5, "mulss xmm0, xmm1");
        check("mulss: tipo == FLOAT_MUL", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("mulss: exLatency == 14",   i->GetExLatency() == 14);
    }

    secao("4.3 FLOAT_DIV (divss)");
    {
        auto i = make_inst(6, "divss xmm0, xmm1");
        check("divss: tipo == FLOAT_DIV", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("divss: exLatency == 40",   i->GetExLatency() == 40);
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
        auto i = make_inst(11, "mov eax, [ebx]");
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
        auto i1 = make_inst(0, "ADD EAX, EBX"); // Maiúsculo.
        check("uppercase -> lowercase", i1->GetInstructionString() ==      "add      eax, ebx");

        auto i2 = make_inst(1, "   MOV\tEAX ,  [EBX+4]   ");
        check("espacos/tabs extras + colchetes preservados",
            i2->GetInstructionString() ==                                 "mov      eax, [ebx+4]");

        auto i3 = make_inst(2, "ADD EAX EBX"); // Sem vírgula
        check("sem vírgula -> normalizado com vírgula",
            i3->GetInstructionString() ==                                 "add      eax, ebx");
    }

    secao("6.2 BRANCH — operandos NUNCA são alterados (nem registrador seria lowercased)");
    {
        auto i1 = make_inst(0, "JMP Loop_1");
        check("opcode lowercase, label preserva o case",
            i1->GetInstructionString() ==                                 "jmp      Loop_1");

        auto i2 = make_inst(1, "JE end_loop");
        check("je + label minúsculo preservado",
            i2->GetInstructionString() ==                                 "je       end_loop");
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. CASOS ESPECÍFICOS DA ARQUITETURA (x86 Intel)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. CASOS ESPECÍFICOS — x86 INTEL");

    secao("7.1 Polivalência do MOV — LOAD, STORE e INT_BASIC (reg-reg e reg-imediato)");
    {
        auto load = make_inst(0, "mov eax, [ebx+4]");
        check("mov reg, [mem]  -> LOAD",  load->GetInstructionType() == INSTRUCTION_TYPE::LOAD);

        auto store = make_inst(1, "mov [ebx+4], eax");
        check("mov [mem], reg  -> STORE", store->GetInstructionType() == INSTRUCTION_TYPE::STORE);

        auto regreg = make_inst(2, "mov eax, ebx");
        check("mov reg, reg    -> INT_BASIC", regreg->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov reg,reg: dest id=0 (eax)", regreg->GetDestRegisters()[0].GetId() == 0);
        check("mov reg,reg: source[1] id=1 (ebx)", regreg->GetSourceRegisters()[1].GetId() == 1);
        // MOV, na vida real, NÃO altera EFLAGS — mas por cair no mesmo caminho
        // genérico de ALU, o código atual adiciona EFLAGS como destino aqui também.
        check("mov reg,reg: EFLAGS aparece como destino (quirk conhecido)",
            regreg->GetDestRegisters()[1].GetType() == 'G');

        auto regimm = make_inst(3, "mov eax, 10");
        check("mov reg, imediato -> INT_BASIC", regimm->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov reg,imediato: só 1 fonte (imediato não resolve)", regimm->GetSourceRegisters().size() == 1);
    }

    secao("7.2 Endereçamento SIB — índice do endereço é descartado, só a base sobrevive");
    {
        // [ebx+ecx*4+8]: a implementação corta a string no primeiro '+'/'*' e
        // fica só com a base ('ebx'); o registrador de índice (ecx) é perdido.
        auto i = make_inst(0, "mov eax, [ebx+ecx*4+8]");
        check("SIB: 1 única fonte (só a base)",   i->GetSourceRegisters().size() == 1);
        check("SIB: source[0] id=1 (ebx, base)",  i->GetSourceRegisters()[0].GetId() == 1);
        check("SIB: string preserva o endereço completo",
            i->GetInstructionString() == "mov      eax, [ebx+ecx*4+8]");
    }

    secao("7.3 LEA é classificado como LOAD (embora não acesse memória de fato)");
    {
        auto i = make_inst(1, "lea eax, [ebx+4]");
        check("lea: tipo == LOAD",         i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lea: memLatency == 1",      i->GetMemLatency() == 1); // Mesma latência de um load real.
        check("lea: dest id=0 (eax)",      i->GetDestRegisters()[0].GetId() == 0);
        check("lea: source id=1 (ebx)",    i->GetSourceRegisters()[0].GetId() == 1);
    }

    secao("7.4 Aliasing entre larguras — mesmo id físico em L/R/W/B");
    {
        auto l = make_inst(2, "mov rax, rbx");
        check("rax/rbx: classe 'L', ids 0/1",
            l->GetDestRegisters()[0].GetType() == 'L' && l->GetDestRegisters()[0].GetId() == 0);

        auto r = make_inst(3, "mov eax, ebx");
        check("eax/ebx: classe 'R', id 0",
            r->GetDestRegisters()[0].GetType() == 'R' && r->GetDestRegisters()[0].GetId() == 0);

        auto w = make_inst(4, "mov ax, bx");
        check("ax/bx: classe 'W', id 0",
            w->GetDestRegisters()[0].GetType() == 'W' && w->GetDestRegisters()[0].GetId() == 0);

        auto b = make_inst(5, "mov al, bl");
        check("al/bl: classe 'B', id 0",
            b->GetDestRegisters()[0].GetType() == 'B' && b->GetDestRegisters()[0].GetId() == 0);

        auto ext = make_inst(6, "add r8d, r9d");
        check("r8d/r9d (regs estendidos): classe 'R', ids 8/9",
            ext->GetDestRegisters()[0].GetType() == 'R' && ext->GetDestRegisters()[0].GetId() == 8 &&
            ext->GetSourceRegisters()[1].GetId() == 9);
    }

    secao("7.5 AL/AH colidem no mesmo id — o modelo não distingue byte alto/baixo");
    {
        // Limitação conhecida: tanto AL (byte baixo) quanto AH (byte alto) do
        // mesmo grupo mapeiam para Register('B', 0) — o modelo não consegue
        // diferenciar os dois, apesar de serem fisicamente registradores distintos.
        auto i = make_inst(7, "mov al, ah");
        check("al: classe 'B' id 0",  i->GetDestRegisters()[0].GetType() == 'B' && i->GetDestRegisters()[0].GetId() == 0);
        check("ah também id 0 (colisão)",
            i->GetSourceRegisters()[1].GetType() == 'B' && i->GetSourceRegisters()[1].GetId() == 0);
    }

    secao("7.6 movzx/movsx — operação legítima entre bancos/larguras diferentes");
    {
        auto i = make_inst(8, "movzx ecx, al");
        check("movzx: tipo == INT_BASIC",  i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("movzx: dest id=2 (ecx, 'R')", i->GetDestRegisters()[0].GetId() == 2 && i->GetDestRegisters()[0].GetType() == 'R');
        check("movzx: source[1] id=0 (al, 'B') — mesmo id numérico de eax, banco diferente",
            i->GetSourceRegisters()[1].GetId() == 0 && i->GetSourceRegisters()[1].GetType() == 'B');
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passou << " OK, " << falhou << " FALHOU\n";
    return falhou ? 1 : 0;
}
