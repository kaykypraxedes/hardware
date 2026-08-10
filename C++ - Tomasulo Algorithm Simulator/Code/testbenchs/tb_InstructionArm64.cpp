/* tb_InstructionArm64.cpp */
// Testbench isolado do módulo Instruction - Arquitetura ARM64
#include "../headers/Instruction.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include <memory>
#include <vector>

using namespace processor;

static const ARCHITECTURE ARCH = ARCHITECTURE::ARM_64;

// Helper do testbench: monta uma instrução ARM64
static std::shared_ptr<Instruction> make_inst(const int position, const std::string& line) {
    std::vector<std::string> lines;
    for (int p = 0; p < position; p++) lines.push_back("nop");
    lines.push_back(line);
    std::vector<std::unique_ptr<Instruction>> parsed = InstructionFactory::ParseTrace(lines, ARCH);
    return std::shared_ptr<Instruction>(std::move(parsed[position]));
}

int main() {
    // ════════════════════════════════════════════════════════════════════
    // 1. CONSTRUÇÃO, PARSE E NORMALIZAÇÃO BÁSICA
    // ════════════════════════════════════════════════════════════════════
    print_title("1. CONSTRUÇÃO E NORMALIZAÇÃO BÁSICA");

    section("1.1 InstructionFactory e Normalização (Maiúsculas, Espaços, Colchetes)");
    {
        auto i = make_inst(0, "   LDR   X0 ,  [ X1 , #8 ]  ");
        check("Posição atribuída corretamente", i->GetPosition() == 0);
        check("Normalização: colchetes preservados, uppercase reduzido e vírgulas corrigidas",
              i->GetInstructionString() == "ldr   x0, [x1, #8]");

        auto b = make_inst(1, "b Target");
        check("Normalização de Label: preserva o case do Target",
              b->GetInstructionString() == "b     Target");
    }

    // ════════════════════════════════════════════════════════════════════
    // 2. ACESSO À MEMÓRIA (LOAD / STORE)
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n";
    print_title("2. ACESSO À MEMÓRIA (LOAD / STORE)");

    section("2.1 Operações Básicas e Aliasing de Largura (ldr, str)");
    {
        auto ldr = make_inst(0, "ldr w0, [x1, #8]");
        check("ldr: tipo == LOAD", ldr->GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("ldr (32-bit): dest w0 é classe 'R'", ldr->GetDestRegisters()[0].GetType() == 'R');
        check("ldr: fonte é apenas a base x1", only_ids(ldr->GetSourceRegisters(), {1}));

        auto str = make_inst(1, "str x0, [x1]");
        check("str: tipo == STORE (sem destino)", str->GetDestRegisters().empty());
        check("str: fontes x0 (dado) e x1 (base)", only_ids(str->GetSourceRegisters(), {0, 1}));
    }

    section("2.2 Pós-Indexação e Zero Register");
    {
        auto ldr_pos = make_inst(0, "ldr x0, [x1], #16"); // 4 tokens
        check("ldr pós-indexado: destino x0", only_ids(ldr_pos->GetDestRegisters(), {0}));
        check("ldr pós-indexado: fonte é apenas x1 (imediato #16 ignorado)", only_ids(ldr_pos->GetSourceRegisters(), {1}));

        auto str_wzr = make_inst(1, "str wzr, [x0]");
        check("str com wzr: wzr não vira fonte, apenas x0", only_ids(str_wzr->GetSourceRegisters(), {0}));
    }

    section("2.3 Pares de Registradores (ldp, stp)");
    {
        auto ldp = make_inst(0, "ldp x0, x1, [x2]");
        check("ldp: destinos x0 e x1", only_ids(ldp->GetDestRegisters(), {0, 1}));
        check("ldp: fonte x2 (base)", only_ids(ldp->GetSourceRegisters(), {2}));

        auto stp = make_inst(1, "stp x29, x30, [sp, #-16]!"); // 5 tokens (writeback '!')
        check("stp com sp: sem destino", stp->GetDestRegisters().empty());
        check("stp com sp: fontes x29, x30 e sp (id 31)", only_ids(stp->GetSourceRegisters(), {29, 30, 31}));

        auto ldp_wb = make_inst(2, "ldp x0, x1, [sp, #16]!"); // 5 tokens (writeback '!')
        check("ldp com writeback: destinos x0 e x1", only_ids(ldp_wb->GetDestRegisters(), {0, 1}));
        check("ldp com writeback: fonte só sp (id 31)", only_ids(ldp_wb->GetSourceRegisters(), {31}));
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. ARITMÉTICA E LÓGICA INTEIRA
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n";
    print_title("3. ARITMÉTICA E LÓGICA INTEIRA");

    section("3.1 Operações de 3 a 5 tokens e sufixo 's' (add, adds, cmp)");
    {
        auto add = make_inst(0, "add x1, x2, x3");
        check("add: dest x1 (sem CPSR)", only_ids(add->GetDestRegisters(), {1}) && no_type(add->GetDestRegisters(), 'G'));
        check("add: fontes x2 e x3", only_ids(add->GetSourceRegisters(), {2, 3}));

        auto adds = make_inst(1, "adds w0, w1, #5");
        check("adds (com imediato): dest w0 e CPSR ('G')", has_reg(adds->GetDestRegisters(), 'G', 80) && has_reg(adds->GetDestRegisters(), 'R', 0));
        check("adds: única fonte w1", only_ids(adds->GetSourceRegisters(), {1}));

        auto cmp = make_inst(2, "cmp x1, x2, lsl #3"); // 5 tokens
        check("cmp: destino único é CPSR", only_ids(cmp->GetDestRegisters(), {80}));
        check("cmp: fontes x1 e x2 (ignora lsl e #3)", only_ids(cmp->GetSourceRegisters(), {1, 2}));
    }

    section("3.2 Multiplicação, Divisão e Movimentação");
    {
        auto mul = make_inst(0, "mul x3, x1, x2");
        check("mul: exLatency == 4", mul->GetExLatency() == 4);

        auto mov = make_inst(1, "mov x0, xzr");
        check("mov com xzr: 0 fontes", mov->GetSourceRegisters().empty());
        check("mov com xzr: dest x0", only_ids(mov->GetDestRegisters(), {0}));

        auto nop = make_inst(2, "nop");
        check("nop: tipo == INT_BASIC", nop->GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("nop: sem destinos e sem fontes",
            nop->GetDestRegisters().empty() && nop->GetSourceRegisters().empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. DESVIOS (BRANCHES)
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n";
    print_title("4. DESVIOS (BRANCHES)");

    section("4.1 Incondicionais vs Condicionais");
    {
        auto b = make_inst(0, "b LOOP");
        check("b (incondicional): 0 fontes (não lê CPSR)", b->GetSourceRegisters().empty());

        auto beq = make_inst(1, "b.eq LOOP");
        check("b.eq (condicional): 1 fonte (lê CPSR)", only_ids(beq->GetSourceRegisters(), {80}));
    }

    section("4.2 Desvios com Links e Registradores (bl, blr, cbz, ret)");
    {
        auto bl = make_inst(0, "bl FUNC");
        check("bl: escreve x30 (link register)", only_ids(bl->GetDestRegisters(), {30}));

        auto blr = make_inst(1, "blr x5");
        check("blr: lê x5, escreve x30", only_ids(blr->GetSourceRegisters(), {5}) && only_ids(blr->GetDestRegisters(), {30}));

        auto cbz = make_inst(2, "cbz w0, Target");
        check("cbz: lê w0", only_ids(cbz->GetSourceRegisters(), {0}));

        auto ret = make_inst(3, "ret");
        check("ret (implícito): lê x30", only_ids(ret->GetSourceRegisters(), {30}));
    }

    section("4.3 Test Bit and Branch (tbz, tbnz)");
        {
            auto tbz = make_inst(0, "tbz x0, #3, LOOP"); // 4 tokens
            check("tbz: le apenas x0", only_ids(tbz->GetSourceRegisters(), {0}));
            check("tbz: sem destinos", tbz->GetDestRegisters().empty());
        }

    // ════════════════════════════════════════════════════════════════════
    // 5. PONTO FLUTUANTE
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n";
    print_title("5. PONTO FLUTUANTE");

    section("5.1 Operações Float (Ignoram regra do CPSR para sufixo 's')");
    {
        auto fcvtzs = make_inst(0, "fcvtzs w0, d1");
        check("fcvtzs: dest w0 (sem CPSR, mesmo terminando em 's')", only_ids(fcvtzs->GetDestRegisters(), {0}) && no_type(fcvtzs->GetDestRegisters(), 'G'));

        auto fcmp = make_inst(1, "fcmp d0, d1");
        check("fcmp: destino é o CPSR", only_ids(fcmp->GetDestRegisters(), {80}));
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. TESTES DE VALIDAÇÃO ESTRITA (ABORTS DA ARQUITETURA)
    // ════════════════════════════════════════════════════════════════════

    /*

    print_title("6. CASOS DE ABORT (Falhas de Sintaxe Esperadas)");
    std::cout << "Aviso: O programa sera interrompido no primeiro teste ativo abaixo.\n";

    section("[ABORT] Destinos inválidos (Imediato / Endereço no lugar do Destino)");
    {
        make_inst(0, "add #5, x1, x2");
        make_inst(1, "ldr [x0], [x1]");
        make_inst(2, "mov label, x1");
    }

    section("[ABORT] Branches corrompidos (Registrador / Imediato no lugar do Label)");
    {
        make_inst(0, "b x1");       // 'b' espera Label, recebeu registrador real
        make_inst(1, "bl #10");     // Recebeu imediato
        make_inst(2, "cbz x0, x1"); // 'cbz' espera Label no token 2, recebeu registrador
    }

    section("[ABORT] Branches com fontes inválidas (Imediato no lugar do Registrador)");
    {
        make_inst(0, "cbz #5, Target"); // cbz precisa ler um registrador
        make_inst(1, "blr #10");
    }

    section("[ABORT] Memória com base inválida (Imediato puro solto)");
    {
        make_inst(0, "ldr x0, #16"); // Falta a base de leitura [x1]
        make_inst(1, "str x0, #16");
    }

    section("[ABORT] Aritmética com sintaxe de memória");
    {
        make_inst(0, "add x1, x2, [x3]"); // Colchetes não são suportados em instruções lógicas/aritméticas
    }

    section("[ABORT] Erros de sintaxe de pontuacao em memoria");
    {
        make_inst(0, "ldr x0, [x1");  // Colchete não fechado (abort em PushAddressSources)
        make_inst(1, "ldr x0, []");   // Endereço vazio (abort em PushAddressSources)
    }

    section("[ABORT] Falha geral de resolucao (Sintaxe nao mapeada pelo SetAttributes)");
    {
        make_inst(0, "add");    // 'add' com 1 token não entra em nenhuma regra de size()
        make_inst(1, "ldr x0"); // 'ldr' com 2 tokens também não bate com a sintaxe esperada
    }

    section("[ABORT] Branch tbz com destino corrompido");
    {
        make_inst(0, "tbz x0, #3, x1"); // 'tbz' espera Label no último token, recebeu registrador real
    }
    */

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
