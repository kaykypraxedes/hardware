/* tb_InstructionX86Intel.cpp */
// Testbench isolado do módulo Instruction - Arquitetura x86 Intel
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::X86_INTEL;

// Helper do testbench: verifica se algum slot (type, id, mask) está no vetor
// (ordem das variantes mascaradas não é determinística: a tabela é um unordered_map).
static bool contains(const std::vector<Register>& regs, const char type, const int id, const int mask = 255) {
    for (const Register& r : regs)
        if (r.GetType() == type && r.GetId() == id && r.GetMask() == mask) return true;
    return false;
}

// Helper do testbench: monta uma instrução x86 Intel em 'position' via InstructionFactory (MESMO caminho que a Thread usa).
// - As linhas "dummy" anteriores são necessárias porque a Factory atribui a posição pelo índice da linha no arquivo de trace.
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> lines;
    for (int p = 0; p < position; p++)
        lines.push_back("add eax, eax"); // dummy: apenas ocupa a posição
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

    section("1.1 Instruction() — construtor padrão (via InstructionX86Intel, que é concreta)");
    {
        InstructionX86Intel i;
        check("GetPosition() == -1",             i.GetPosition() == -1);
        check("GetInstructionType() == INVALID", i.GetInstructionType() == INSTRUCTION_TYPE::INVALID);
        check("GetExLatency() == 0",              i.GetExLatency() == 0);
        check("GetMemLatency() == 0",             i.GetMemLatency() == 0);
    }

    section("1.2 InstructionFactory — posição pelo índice da linha e string");
    {
        auto i = make_inst(7, "add eax, ebx");
        check("GetPosition() == 7",                             i->GetPosition() == 7);
        check("GetInstructionString() == 'add      eax, ebx'",  i->GetInstructionString() == "add      eax, ebx");
    }

    section("1.3 InstructionFactory — arquitetura de trace (X86_INTEL)");
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

    section("[ABORT] String vazia deve abortar");
    {
        InstructionX86Intel i(5);
        i.Parse("");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Instrução desconhecida deve abortar");
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

    section("2.1 LOAD (via MOV e via opcode dedicado)");
    {
        auto i = make_inst(0, "mov eax, [ebx+4]");
        check("mov load: tipo == LOAD",             i->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("mov load: exLatency  == 1",          i->GetExLatency()  == 1);
        check("mov load: memLatency == 1",          i->GetMemLatency() == 1);
        check("mov load: dest[0] tipo='R'",         i->GetDestRegisters()[0].GetType() == 'R');
        check("mov load: dest[0] id=0 (eax)",       i->GetDestRegisters()[0].GetId()   == 0);
        check("mov load: dest inclui rax/ax/al/ah (aliases do eax)",
            contains(i->GetDestRegisters(), 'L', 0, 0xFF) && contains(i->GetDestRegisters(), 'W', 0, 0x03) &&
            contains(i->GetDestRegisters(), 'B', 0, 0x01) && contains(i->GetDestRegisters(), 'B', 0, 0x02));
        check("mov load: source[0] tipo='R'",       i->GetSourceRegisters()[0].GetType() == 'R');
        check("mov load: source[0] id=1 (ebx base)",i->GetSourceRegisters()[0].GetId()   == 1);
        check("mov load: base ebx bloqueia rbx/bx/bl/bh",
            contains(i->GetSourceRegisters(), 'L', 1) && contains(i->GetSourceRegisters(), 'B', 1, 0x01));
        check("mov load: offset '+4' não vira fonte extra", i->GetSourceRegisters().size() == 5);
        check("instruction_string preserva colchetes",
            i->GetInstructionString() == "mov      eax, [ebx+4]");

        auto v = make_inst(1, "movss xmm0, [eax]");
        check("movss: tipo == LOAD",     v->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("movss: dest tipo='V'",    v->GetDestRegisters()[0].GetType() == 'V');
        check("movss: dest id=64",       v->GetDestRegisters()[0].GetId()   == 64);
        check("movss: source id=0 (eax)",v->GetSourceRegisters()[0].GetId() == 0);
    }

    section("2.2 STORE (via MOV)");
    {
        auto i = make_inst(2, "mov [ebx+4], eax");
        check("mov store: tipo == STORE",                 i->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("mov store: exLatency  == 1",                i->GetExLatency()  == 1);
        check("mov store: memLatency == 1",                i->GetMemLatency() == 1);
        check("mov store: sem destino",                    i->GetDestRegisters().empty());
        check("mov store: source[0] tipo='R' (dado)",      i->GetSourceRegisters()[0].GetType() == 'R');
        check("mov store: source[0] id=0 (eax, dado)",     i->GetSourceRegisters()[0].GetId()   == 0);
        check("mov store: 10 fontes (eax+aliases, ebx+aliases)", i->GetSourceRegisters().size() == 10);
        check("mov store: endereço ebx está entre as fontes",
            contains(i->GetSourceRegisters(), 'R', 1, 0x0F));
        check("instruction_string preserva colchetes",
            i->GetInstructionString() == "mov      [ebx+4], eax");
    }

    section("2.3 INT_BASIC — EFLAGS entra como destino e origem inicial (quirk do modelo)");
    {
        auto i = make_inst(3, "add eax, ebx");
        check("add: tipo == INT_BASIC",         i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("add: 6 destinos (eax+aliases + EFLAGS)", i->GetDestRegisters().size() == 6);
        check("add: dest[0] id=0 (eax)",        i->GetDestRegisters()[0].GetId()   == 0);
        check("add: EFLAGS está entre os destinos", contains(i->GetDestRegisters(), 'G', 80));
        check("add: destinos sem duplicatas",   i->GetDestRegisters().size() == 6);
        check("add: source[0] id=0 (eax reusado como fonte)", i->GetSourceRegisters()[0].GetId() == 0);
        check("add: ebx (id 1) presente nas fontes", contains(i->GetSourceRegisters(), 'R', 1, 0x0F));

        auto imm = make_inst(4, "add eax, 5");
        check("add com imediato: 5 fontes (família eax, imediato não resolve)",
            imm->GetSourceRegisters().size() == 5);
    }

    section("2.4 INT_MUL e INT_DIV (imul, idiv)");
    {
        auto mul = make_inst(5, "imul eax, ebx");
        check("imul: tipo == INT_MUL", mul->GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("imul: exLatency == 4",  mul->GetExLatency() == 4);

        auto div = make_inst(6, "idiv eax");
        check("idiv: tipo == INT_DIV",        div->GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("idiv: exLatency == 10",        div->GetExLatency() == 10);
        check("idiv (1 operando): fontes = famílias eax (0) + edx (2)",
            has_reg(div->GetSourceRegisters(), 'R', 0) && has_reg(div->GetSourceRegisters(), 'R', 2) &&
            only_ids(div->GetSourceRegisters(), {0, 2}));
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. IDENTIFICAÇÃO DE TIPO — BRANCH
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. IDENTIFICAÇÃO DE TIPO — BRANCH");

    section("3.1 BRANCH sempre lê EFLAGS — mesmo desvios incondicionais (quirk do modelo)");
    {
        auto je = make_inst(0, "je end_loop");
        check("je: tipo == BRANCH",         je->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("je: source[0] tipo='G'",     je->GetSourceRegisters()[0].GetType() == 'G');
        check("je: source[0] id=80",        je->GetSourceRegisters()[0].GetId()   == 80);
        check("je: só 1 fonte (EFLAGS)",    je->GetSourceRegisters().size() == 1);

        // jmp é incondicional e não lê EFLAGS — só os desvios condicionais (JCC).
        auto jmp = make_inst(1, "jmp Loop_1");
        check("jmp (incondicional): 0 fontes (não lê EFLAGS)",
            jmp->GetSourceRegisters().empty());
        check("label 'Loop_1' preserva o case original",
            jmp->GetInstructionString() == "jmp      Loop_1");
    }

    section("3.2 call e ret — rsp implícito, sem EFLAGS");
    {
        auto call = make_inst(2, "call foo");
        check("call: não lê EFLAGS", no_type(call->GetSourceRegisters(), 'G'));
        check("call: rsp (família id 6) como fonte e destino implícitos",
            has_reg(call->GetSourceRegisters(), 'L', 6) && has_reg(call->GetDestRegisters(), 'L', 6));

        auto ret = make_inst(3, "ret");
        check("ret: tipo == BRANCH",     ret->GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("ret: não lê EFLAGS mesmo sem nenhum operando na string",
            no_type(ret->GetSourceRegisters(), 'G'));
        check("ret: rsp (família id 6) como fonte e destino implícitos",
            has_reg(ret->GetSourceRegisters(), 'L', 6) && has_reg(ret->GetDestRegisters(), 'L', 6));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE (SSE)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. IDENTIFICAÇÃO DE TIPO — PONTO FLUTUANTE (SSE)");

    section("4.1 FLOAT_BASIC (addss) — mesmo quirk do EFLAGS como destino");
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

    section("4.2 FLOAT_MUL (mulss)");
    {
        auto i = make_inst(5, "mulss xmm0, xmm1");
        check("mulss: tipo == FLOAT_MUL", i->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("mulss: exLatency == 14",   i->GetExLatency() == 14);
    }

    section("4.3 FLOAT_DIV (divss)");
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

    section("6.1 NormalizeInstruction — casos variados");
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

    section("6.2 BRANCH — operandos NUNCA são alterados (nem registrador seria lowercased)");
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

    section("7.1 Polivalência do MOV — LOAD, STORE e INT_BASIC (reg-reg e reg-imediato)");
    {
        auto load = make_inst(0, "mov eax, [ebx+4]");
        check("mov reg, [mem]  -> LOAD",  load->GetInstructionType() == INSTRUCTION_TYPE::LOAD);

        auto store = make_inst(1, "mov [ebx+4], eax");
        check("mov [mem], reg  -> STORE", store->GetInstructionType() == INSTRUCTION_TYPE::STORE);

        auto regreg = make_inst(2, "mov eax, ebx");
        check("mov reg, reg    -> INT_BASIC", regreg->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov reg,reg: dest[0] id=0 (eax)", regreg->GetDestRegisters()[0].GetId() == 0);
        check("mov reg,reg: source[0] id=1 (ebx)", regreg->GetSourceRegisters()[0].GetId() == 1);
        // MOV, na vida real, NÃO altera EFLAGS — o quirk de cópia foi corrigido.
        check("mov reg,reg: 5 destinos (eax+aliases, sem EFLAGS)",
            regreg->GetDestRegisters().size() == 5 && !contains(regreg->GetDestRegisters(), 'G', 80));

        auto regimm = make_inst(3, "mov eax, 10");
        check("mov reg, imediato -> INT_BASIC", regimm->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov reg,imediato: 5 destinos (eax+aliases)",
            regimm->GetDestRegisters().size() == 5);
        check("mov reg,imediato: nenhuma fonte (imediato não resolve)", regimm->GetSourceRegisters().empty());
    }

    section("7.2 Endereçamento SIB — base e índice viram fontes");
    {
        // [ebx+ecx*4+8]: o parser captura todos os registradores do endereço.
        auto i = make_inst(0, "mov eax, [ebx+ecx*4+8]");
        check("SIB: base ebx entre as fontes", has_reg(i->GetSourceRegisters(), 'R', 1));
        check("SIB: índice ecx também vira fonte", has_reg(i->GetSourceRegisters(), 'R', 2));
        check("SIB: fontes só as famílias ebx/ecx", only_ids(i->GetSourceRegisters(), {1, 2}));
        check("SIB: string preserva o endereço completo",
            i->GetInstructionString() == "mov      eax, [ebx+ecx*4+8]");
    }

    section("7.3 LEA é INT_BASIC (calcula endereço sem acessar memória)");
    {
        auto i = make_inst(1, "lea eax, [ebx+4]");
        check("lea: tipo == INT_BASIC",    i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("lea: memLatency == 0",      i->GetMemLatency() == 0); // Sem acesso à memória.
        check("lea: dest id=0 (eax)",      i->GetDestRegisters()[0].GetId() == 0);
        check("lea: source id=1 (ebx)",    i->GetSourceRegisters()[0].GetId() == 1);
    }

    section("7.4 Aliasing entre larguras — mesmo id físico em L/R/W/B");
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
            contains(ext->GetSourceRegisters(), 'R', 9, 0x0F));
    }

    section("7.5 AL/AH — variantes distintas por máscara (byte alto/baixo)");
    {
        // al = (id 0, mask 0x01), ah = (id 0, mask 0x02): o modelo diferencia o
        // byte baixo/alto — AL e AH NÃO se bloqueiam; ambos bloqueiam ax/eax/rax.
        auto i = make_inst(7, "mov al, ah");
        check("al: classe 'B' id 0 mask 0x01",
            i->GetDestRegisters()[0].GetType() == 'B' && i->GetDestRegisters()[0].GetId() == 0 &&
            i->GetDestRegisters()[0].GetMask() == 0x01);
        check("ah: classe 'B' id 0 mask 0x02",
            i->GetSourceRegisters()[0].GetType() == 'B' && i->GetSourceRegisters()[0].GetId() == 0 &&
            i->GetSourceRegisters()[0].GetMask() == 0x02);
        check("dests = {al, ax, eax, rax}: 4, sem ah",
            i->GetDestRegisters().size() == 4 && !contains(i->GetDestRegisters(), 'B', 0, 0x02));
        check("sources = {ah, ax, eax, rax}: 4, sem al",
            i->GetSourceRegisters().size() == 4 && !contains(i->GetSourceRegisters(), 'B', 0, 0x01));
        check("al e ah bloqueiam ax/eax/rax em comum",
            contains(i->GetDestRegisters(), 'W', 0, 0x03) && contains(i->GetSourceRegisters(), 'W', 0, 0x03) &&
            contains(i->GetDestRegisters(), 'L', 0, 0xFF) && contains(i->GetSourceRegisters(), 'L', 0, 0xFF));
    }

    section("7.6 movzx/movsx — operação legítima entre bancos/larguras diferentes");
    {
        auto i = make_inst(8, "movzx ecx, al");
        check("movzx: tipo == INT_BASIC",  i->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("movzx: dest[0] id=2 (ecx, 'R')", i->GetDestRegisters()[0].GetId() == 2 && i->GetDestRegisters()[0].GetType() == 'R');
        check("movzx: 5 destinos (ecx+aliases, sem EFLAGS)",
            only_ids(i->GetDestRegisters(), {2}) && no_type(i->GetDestRegisters(), 'G'));
        check("movzx: al presente nas fontes ('B', id 0, mask 0x01)",
            contains(i->GetSourceRegisters(), 'B', 0, 0x01));
        check("movzx: não mexe em EFLAGS (zerar estender não afeta flags)",
            no_type(i->GetSourceRegisters(), 'G'));
    }

    section("7.7 Família MOVS (SSE) — colchetes decidem STORE/LOAD, cópia vira FLOAT_BASIC");
    {
        auto store = make_inst(9, "movsd [rbx], xmm6");
        check("movsd [mem], reg -> STORE", store->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("movsd store: sem destino",  store->GetDestRegisters().empty());
        check("movsd store: source[0]='V' id=70 (xmm6, dado)",
            store->GetSourceRegisters()[0].GetType() == 'V' && store->GetSourceRegisters()[0].GetId() == 70);
        check("movsd store: source[1]='L' id=1 (rbx, base)",
            store->GetSourceRegisters()[1].GetType() == 'L' && store->GetSourceRegisters()[1].GetId() == 1);
        check("movsd store: 6 fontes (xmm6 + rbx e aliases)",
            store->GetSourceRegisters().size() == 6);

        auto copy = make_inst(10, "movsd xmm6, xmm2");
        check("movsd reg, reg -> FLOAT_BASIC (cópia)", copy->GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("movsd copy: dest[0]='V' id=70 (xmm6)",
            copy->GetDestRegisters()[0].GetType() == 'V' && copy->GetDestRegisters()[0].GetId() == 70);
        check("movsd copy: só 1 destino (sem EFLAGS)", copy->GetDestRegisters().size() == 1);
        check("movsd copy: source[0]='V' id=66 (xmm2)",
            copy->GetSourceRegisters()[0].GetType() == 'V' && copy->GetSourceRegisters()[0].GetId() == 66);
    }

    section("7.8 CDB — um slot por variante (al ≠ ah) e bancos contíguos");
    {
        CDB cdb = InstructionX86Intel().MakeCDB();
        check("85 registradores físicos (16*5 + 4 + 16 + 1)",
            cdb.registers.size() == 85);
        const Register al_probe('B', 0, 0x01);
        const Register ah_probe('B', 0, 0x02);
        const Register& al_slot = GetReg(cdb, al_probe);
        const Register& ah_slot = GetReg(cdb, ah_probe);
        check("al e ah são slots distintos no CDB", &al_slot != &ah_slot);
        check("máscaras distintas nos slots", al_slot.GetMask() == 0x01 && ah_slot.GetMask() == 0x02);
        check("7 bancos de impressão", cdb.print_banks.size() == 7);
    }

    section("7.9 Label como operando de memória — não aborta, não vira fonte");
    {
        // 'mov eax, [var]' é sintaxe x86 válida: antes abortava no LookupRegister
        // ("Registrador inválido: 'var'"). Agora o label não vira fonte.
        auto load = make_inst(0, "mov eax, [var]");
        check("mov eax, [var] -> LOAD", load->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("mov eax, [var]: dest eax", has_reg(load->GetDestRegisters(), 'R', 0));
        check("mov eax, [var]: 0 fontes (label não resolve)", load->GetSourceRegisters().empty());
        check("mov eax, [var]: string preserva o label",
            load->GetInstructionString() == "mov      eax, [var]");

        auto store = make_inst(1, "mov [var], eax");
        check("mov [var], eax -> STORE", store->GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("mov [var], eax: sem destinos", store->GetDestRegisters().empty());
        check("mov [var], eax: só eax vira fonte",
            only_ids(store->GetSourceRegisters(), {0}));

        // Label direto sem colchetes ("mov eax, var") — também válido em x86.
        auto direct = make_inst(2, "mov eax, var");
        check("mov eax, var -> INT_BASIC", direct->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov eax, var: dest eax", has_reg(direct->GetDestRegisters(), 'R', 0));
        check("mov eax, var: 0 fontes", direct->GetSourceRegisters().empty());

        // RMW com label: só o destino antigo (eax) vira fonte, além de EFLAGS.
        auto add = make_inst(3, "add eax, [var]");
        check("add eax, [var]: fontes só eax (RMW) + EFLAGS",
            only_ids(add->GetSourceRegisters(), {0, 80}));

        auto lea = make_inst(4, "lea eax, [var]");
        check("lea eax, [var]: dest eax", has_reg(lea->GetDestRegisters(), 'R', 0));
        check("lea eax, [var]: 0 fontes", lea->GetSourceRegisters().empty());

        auto jmp = make_inst(5, "jmp [var]");
        check("jmp [var]: BRANCH sem fontes (nem EFLAGS)", jmp->GetSourceRegisters().empty());

        auto push = make_inst(6, "push [var]");
        check("push [var]: só rsp (id 6) em fontes e destinos",
            only_ids(push->GetSourceRegisters(), {6}) && only_ids(push->GetDestRegisters(), {6}));

        // Regressão: registrador real no endereço continua virando fonte.
        auto reg = make_inst(7, "mov eax, [ebx+4]");
        check("mov eax, [ebx+4]: base ebx ainda vira fonte",
            has_reg(reg->GetSourceRegisters(), 'R', 1));
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
