/* tb_ArchSimplified.cpp */
//Testbench isolado de ArchSimplified.cpp
#include "../Architectures/headers/ArchSimplified.h"
#include "tb_Helpers.h"
#include <iostream>
#include <vector>

using namespace processor;

// Ids físicos desta arquitetura (ver RegisterTable() em ArchSimplified.cpp):
// - 'R' (int):   id = n        (r0..r31)
// - 'F' (float): id = 32 + n   (f0..f31)
// - 'M' (hi/lo): hi = 64, lo = 65
// - "ra" é apelido de r31 (mesmo id físico, tipo 'R').
static constexpr int FREG(int n) { return 32 + n; }
static constexpr int RA{31};
static constexpr int HI{64};
static constexpr int LO{65};

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. PARSE — TOLERÂNCIA DE FORMATAÇÃO E CASE
    // ════════════════════════════════════════════════════════════════════

    print_title("1. PARSE — TOLERÂNCIA DE FORMATAÇÃO E CASE");

    section("1.1 Espaçamento variado — resultado idêntico");
    {
        InstructionSimplified a(0);
        a.Parse("add r1,r2,r3");
        InstructionSimplified b(0);
        b.Parse("add   r1 , r2 ,   r3");
        InstructionSimplified c(0);
        c.Parse("add\tr1,\tr2,\tr3");

        check("sem espaços extras: GetInstructionType() == INT_BASIC",
            a.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("espaços extras: mesmo tipo",
            b.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("tabs: mesmo tipo",
            c.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("string canônica idêntica independente da formatação de entrada",
            a.GetInstructionString() == b.GetInstructionString() &&
            b.GetInstructionString() == c.GetInstructionString());
    }

    section("1.2 Maiúsculo/minúsculo — opcode e registrador case-insensitive");
    {
        InstructionSimplified lower(0);
        lower.Parse("add r1, r2, r3");
        InstructionSimplified upper(0);
        upper.Parse("ADD R1, R2, R3");
        InstructionSimplified mixed(0);
        mixed.Parse("AdD r1, R2, r3");

        check("'ADD' reconhecido como INT_BASIC",
            upper.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("'AdD' reconhecido como INT_BASIC",
            mixed.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mesmo registrador de destino independente da caixa",
            has_reg(upper.GetDestRegisters(), 'R', 1) && has_reg(mixed.GetDestRegisters(), 'R', 1));
        check("string canônica sempre em minúsculo (opcode e registradores)",
            lower.GetInstructionString() == upper.GetInstructionString() &&
            upper.GetInstructionString() == mixed.GetInstructionString());
    }

    // ════════════════════════════════════════════════════════════════════
    // 2. LOADS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. LOADS");

    section("2.1 lw — destino inteiro, offset positivo e negativo (representa 8 opcodes iguais)");
    {
        InstructionSimplified pos(0);
        pos.Parse("lw r1, 10(r2)");
        check("lw: GetInstructionType() == LOAD",
            pos.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("lw: dest == {r1}",
            pos.GetDestRegisters().size() == 1 && has_reg(pos.GetDestRegisters(), 'R', 1));
        check("lw: ex_source == {r2}",
            pos.GetExSourceRegisters().size() == 1 && has_reg(pos.GetExSourceRegisters(), 'R', 2));
        check("lw: mem_source vazio",
            pos.GetMemSourceRegisters().empty());
        check("lw: EX lat == 1, MEM lat == 1",
            pos.GetExLatency() == 1 && pos.GetMemLatency() == 1);

        InstructionSimplified neg(0);
        neg.Parse("lw r1, -4(r2)");
        check("lw: offset negativo é válido (base+offset permite deslocamento negativo)",
            neg.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
    }

    section("2.2 l.d — destino float, base sempre inteira");
    {
        InstructionSimplified i(0);
        i.Parse("l.d f1, 8(r2)");
        check("l.d: GetInstructionType() == LOAD",
            i.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("l.d: dest == {f1}",
            i.GetDestRegisters().size() == 1 && has_reg(i.GetDestRegisters(), 'F', FREG(1)));
        check("l.d: ex_source == {r2} (base é sempre inteira, mesmo em load float)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("l.d: nenhum registrador float na fonte",
            no_type(i.GetExSourceRegisters(), 'F'));
    }

    section("2.3 load (genérico) — aceita destino inteiro OU float");
    {
        InstructionSimplified as_int(0);
        as_int.Parse("load r1, 4(r2)");
        check("load(int): dest == {r1}",
            has_reg(as_int.GetDestRegisters(), 'R', 1));

        InstructionSimplified as_float(0);
        as_float.Parse("load f1, 4(r2)");
        check("load(float): dest == {f1}",
            has_reg(as_float.GetDestRegisters(), 'F', FREG(1)));
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. STORES
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. STORES");

    section("3.1 sw — valor inteiro, sem dest_registers (representa 5 opcodes iguais)");
    {
        InstructionSimplified i(0);
        i.Parse("sw r1, 10(r2)");
        check("sw: GetInstructionType() == STORE",
            i.GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("sw: dest_registers vazio (store não escreve registrador)",
            i.GetDestRegisters().empty());
        check("sw: ex_source == {r2} (endereço, necessário em EX)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("sw: mem_source == {r1} (valor armazenado, só necessário em MEM)",
            has_reg(i.GetMemSourceRegisters(), 'R', 1));
        check("sw: EX lat == 1, MEM lat == 1",
            i.GetExLatency() == 1 && i.GetMemLatency() == 1);
    }

    section("3.2 s.d — valor float");
    {
        InstructionSimplified i(0);
        i.Parse("s.d f1, 8(r2)");
        check("s.d: mem_source == {f1}",
            has_reg(i.GetMemSourceRegisters(), 'F', FREG(1)));
        check("s.d: ex_source == {r2} (base sempre inteira)",
            has_reg(i.GetExSourceRegisters(), 'R', 2));
    }

    section("3.3 store (genérico) — aceita valor inteiro OU float");
    {
        InstructionSimplified as_int(0);
        as_int.Parse("store r1, 4(r2)");
        check("store(int): mem_source == {r1}",
            has_reg(as_int.GetMemSourceRegisters(), 'R', 1));

        InstructionSimplified as_float(0);
        as_float.Parse("store f1, 4(r2)");
        check("store(float): mem_source == {f1}",
            has_reg(as_float.GetMemSourceRegisters(), 'F', FREG(1)));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. BRANCHES
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. BRANCHES");

    section("4.1 beq — 2 registradores + label, sem escrita implícita (representa beq/bne)");
    {
        InstructionSimplified i(0);
        i.Parse("beq r1, r2, LOOP");
        check("beq: GetInstructionType() == BRANCH",
            i.GetInstructionType() == INSTRUCTION_TYPE::BRANCH);
        check("beq: ex_source == {r1, r2}",
            i.GetExSourceRegisters().size() == 2 &&
            has_reg(i.GetExSourceRegisters(), 'R', 1) && has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("beq: dest vazio",
            i.GetDestRegisters().empty());
        check("beq: EX lat == 1",
            i.GetExLatency() == 1);
    }

    section("4.2 bnez — 1 registrador + label, comparação implícita com zero (representa 6 opcodes iguais)");
    {
        InstructionSimplified i(0);
        i.Parse("bnez r1, LOOP");
        check("bnez: ex_source == {r1}",
            i.GetExSourceRegisters().size() == 1 && has_reg(i.GetExSourceRegisters(), 'R', 1));
        check("bnez: dest vazio",
            i.GetDestRegisters().empty());
    }

    section("4.3 bltzal — mesmo formato do bnez, mas escreve 'ra' implicitamente (representa bltzal/bgezal)");
    {
        InstructionSimplified i(0);
        i.Parse("bltzal r1, LOOP");
        check("bltzal: ex_source == {r1}",
            has_reg(i.GetExSourceRegisters(), 'R', 1));
        check("bltzal: dest == {ra} (r31, implícito)",
            i.GetDestRegisters().size() == 1 && has_reg(i.GetDestRegisters(), 'R', RA));
    }

    section("4.4 j — só label, nenhum registrador envolvido");
    {
        InstructionSimplified i(0);
        i.Parse("j LOOP");
        check("j: dest vazio",
            i.GetDestRegisters().empty());
        check("j: ex_source vazio",
            i.GetExSourceRegisters().empty());
    }

    section("4.5 jal — só label, escreve 'ra' implicitamente");
    {
        InstructionSimplified i(0);
        i.Parse("jal LOOP");
        check("jal: dest == {ra}",
            has_reg(i.GetDestRegisters(), 'R', RA));
        check("jal: ex_source vazio (não lê registrador algum)",
            i.GetExSourceRegisters().empty());
    }

    section("4.6 jr — só registrador, sem label");
    {
        InstructionSimplified i(0);
        i.Parse("jr r1");
        check("jr: ex_source == {r1}",
            has_reg(i.GetExSourceRegisters(), 'R', 1));
        check("jr: dest vazio",
            i.GetDestRegisters().empty());
    }

    section("4.7 jalr — duas sintaxes válidas: (rs) e (rd, rs)");
    {
        InstructionSimplified one_op(0);
        one_op.Parse("jalr r1");
        check("jalr(rs): ex_source == {r1}",
            has_reg(one_op.GetExSourceRegisters(), 'R', 1));
        check("jalr(rs): dest == {ra} (implícito)",
            has_reg(one_op.GetDestRegisters(), 'R', RA));

        InstructionSimplified two_op(0);
        two_op.Parse("jalr r2, r1");
        check("jalr(rd,rs): dest == {r2} (explícito)",
            has_reg(two_op.GetDestRegisters(), 'R', 2));
        check("jalr(rd,rs): ex_source == {r1}",
            has_reg(two_op.GetExSourceRegisters(), 'R', 1));
        check("jalr(rd,rs): NÃO escreve em ra (destino já foi dado explicitamente)",
            !has_reg(two_op.GetDestRegisters(), 'R', RA));
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. INT_BASIC
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. INT_BASIC");

    section("5.1 add — 3 registradores inteiros (representa 17 opcodes iguais)");
    {
        InstructionSimplified i(0);
        i.Parse("add r1, r2, r3");
        check("add: dest == {r1}",
            has_reg(i.GetDestRegisters(), 'R', 1));
        check("add: ex_source == {r2, r3}",
            i.GetExSourceRegisters().size() == 2 &&
            has_reg(i.GetExSourceRegisters(), 'R', 2) && has_reg(i.GetExSourceRegisters(), 'R', 3));
        check("add: EX lat == 1",
            i.GetExLatency() == 1);
    }

    section("5.2 addi — 2 registradores + imediato ASSINADO (representa 8 opcodes iguais)");
    {
        InstructionSimplified pos(0);
        pos.Parse("addi r1, r2, #5");
        check("addi(+5): dest == {r1}, ex_source == {r2}",
            has_reg(pos.GetDestRegisters(), 'R', 1) && has_reg(pos.GetExSourceRegisters(), 'R', 2));

        InstructionSimplified neg(0);
        neg.Parse("addi r1, r2, #-5");
        check("addi(-5): imediato negativo é válido em campo assinado",
            neg.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
    }

    section("5.3 andi / sll — imediato NÃO-ASSINADO (código idêntico; 2 opcodes checados por segurança extra)");
    {
        InstructionSimplified andi_i(0);
        andi_i.Parse("andi r1, r2, #5");
        check("andi: dest == {r1}, ex_source == {r2}",
            has_reg(andi_i.GetDestRegisters(), 'R', 1) && has_reg(andi_i.GetExSourceRegisters(), 'R', 2));

        InstructionSimplified sll_i(0);
        sll_i.Parse("sll r1, r2, #5");
        check("sll: dest == {r1}, ex_source == {r2}",
            has_reg(sll_i.GetDestRegisters(), 'R', 1) && has_reg(sll_i.GetExSourceRegisters(), 'R', 2));
    }

    section("5.4 lui — sem registrador rs");
    {
        InstructionSimplified i(0);
        i.Parse("lui r1, #5");
        check("lui: dest == {r1}",
            has_reg(i.GetDestRegisters(), 'R', 1));
        check("lui: ex_source vazio (não lê registrador algum)",
            i.GetExSourceRegisters().empty());
    }

    section("5.5 mfhi / mflo — fonte implícita em HI/LO (registradores DIFERENTES entre si)");
    {
        InstructionSimplified hi_i(0);
        hi_i.Parse("mfhi r1");
        check("mfhi: dest == {r1}",
            has_reg(hi_i.GetDestRegisters(), 'R', 1));
        check("mfhi: ex_source == {hi}",
            hi_i.GetExSourceRegisters().size() == 1 && has_reg(hi_i.GetExSourceRegisters(), 'M', HI));

        InstructionSimplified lo_i(0);
        lo_i.Parse("mflo r1");
        check("mflo: ex_source == {lo}",
            lo_i.GetExSourceRegisters().size() == 1 && has_reg(lo_i.GetExSourceRegisters(), 'M', LO));
        check("mfhi e mflo leem registradores diferentes (hi != lo)",
            !has_reg(lo_i.GetExSourceRegisters(), 'M', HI));
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. INT_MUL
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. INT_MUL");

    section("6.1 mult — resultado em HI/LO (representa mult/multu/dmult/dmultu)");
    {
        InstructionSimplified i(0);
        i.Parse("mult r1, r2");
        check("mult: GetInstructionType() == INT_MUL",
            i.GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mult: ex_source == {r1, r2}",
            i.GetExSourceRegisters().size() == 2 &&
            has_reg(i.GetExSourceRegisters(), 'R', 1) && has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("mult: dest == {hi, lo}",
            i.GetDestRegisters().size() == 2 &&
            has_reg(i.GetDestRegisters(), 'M', HI) && has_reg(i.GetDestRegisters(), 'M', LO));
        check("mult: EX lat == 4",
            i.GetExLatency() == 4);
    }

    section("6.2 mul — 3 registradores, resultado direto (SEM hi/lo)");
    {
        InstructionSimplified i(0);
        i.Parse("mul r1, r2, r3");
        check("mul: dest == {r1}",
            has_reg(i.GetDestRegisters(), 'R', 1));
        check("mul: NÃO usa hi/lo",
            no_type(i.GetDestRegisters(), 'M'));
        check("mul: ex_source == {r2, r3}",
            has_reg(i.GetExSourceRegisters(), 'R', 2) && has_reg(i.GetExSourceRegisters(), 'R', 3));
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. INT_DIV
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. INT_DIV");

    section("7.1 div — mesmo padrão do mult: rs, rt -> hi/lo (representa 4 opcodes iguais)");
    {
        InstructionSimplified i(0);
        i.Parse("div r1, r2");
        check("div: GetInstructionType() == INT_DIV",
            i.GetInstructionType() == INSTRUCTION_TYPE::INT_DIV);
        check("div: ex_source == {r1, r2}",
            has_reg(i.GetExSourceRegisters(), 'R', 1) && has_reg(i.GetExSourceRegisters(), 'R', 2));
        check("div: dest == {hi, lo}",
            has_reg(i.GetDestRegisters(), 'M', HI) && has_reg(i.GetDestRegisters(), 'M', LO));
        check("div: EX lat == 10",
            i.GetExLatency() == 10);
    }

    // ════════════════════════════════════════════════════════════════════
    // 8. FLOAT_BASIC
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("8. FLOAT_BASIC");

    section("8.1 add.d — 3 registradores float (representa add.d/add.s/sub.d/sub.s)");
    {
        InstructionSimplified i(0);
        i.Parse("add.d f1, f2, f3");
        check("add.d: GetInstructionType() == FLOAT_BASIC",
            i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
        check("add.d: dest == {f1}",
            has_reg(i.GetDestRegisters(), 'F', FREG(1)));
        check("add.d: ex_source == {f2, f3}",
            has_reg(i.GetExSourceRegisters(), 'F', FREG(2)) && has_reg(i.GetExSourceRegisters(), 'F', FREG(3)));
        check("add.d: EX lat == 9",
            i.GetExLatency() == 9);
    }

    section("8.2 sqrt.s — unário/conversão (representa 9 opcodes iguais: abs.s, neg.s, cvt.*)");
    {
        InstructionSimplified i(0);
        i.Parse("sqrt.s f1, f2");
        check("sqrt.s: dest == {f1}",
            has_reg(i.GetDestRegisters(), 'F', FREG(1)));
        check("sqrt.s: ex_source == {f2}",
            i.GetExSourceRegisters().size() == 1 && has_reg(i.GetExSourceRegisters(), 'F', FREG(2)));
    }

    // ════════════════════════════════════════════════════════════════════
    // 9. FLOAT_MUL
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("9. FLOAT_MUL");

    section("9.1 mul.d — 3 registradores float (representa mul.d/mul.s)");
    {
        InstructionSimplified i(0);
        i.Parse("mul.d f1, f2, f3");
        check("mul.d: GetInstructionType() == FLOAT_MUL",
            i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_MUL);
        check("mul.d: dest == {f1}, ex_source == {f2, f3}",
            has_reg(i.GetDestRegisters(), 'F', FREG(1)) &&
            has_reg(i.GetExSourceRegisters(), 'F', FREG(2)) && has_reg(i.GetExSourceRegisters(), 'F', FREG(3)));
        check("mul.d: EX lat == 14",
            i.GetExLatency() == 14);
    }

    // ════════════════════════════════════════════════════════════════════
    // 10. FLOAT_DIV
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("10. FLOAT_DIV");

    section("10.1 div.d — 3 registradores float (representa div.d/div.s)");
    {
        InstructionSimplified i(0);
        i.Parse("div.d f1, f2, f3");
        check("div.d: GetInstructionType() == FLOAT_DIV",
            i.GetInstructionType() == INSTRUCTION_TYPE::FLOAT_DIV);
        check("div.d: dest == {f1}, ex_source == {f2, f3}",
            has_reg(i.GetDestRegisters(), 'F', FREG(1)) &&
            has_reg(i.GetExSourceRegisters(), 'F', FREG(2)) && has_reg(i.GetExSourceRegisters(), 'F', FREG(3)));
        check("div.d: EX lat == 40 (a mais cara da arquitetura)",
            i.GetExLatency() == 40);
    }

    // ════════════════════════════════════════════════════════════════════
    // 11. NORMALIZAÇÃO / INSTRUCTION_STRING
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("11. NORMALIZAÇÃO / INSTRUCTION_STRING");

    section("11.1 Padding do opcode — coluna de 7 chars (biggest_instruction)");
    {
        InstructionSimplified curto(0);
        curto.Parse("add r1, r2, r3");
        check("opcode curto ('add', 3 chars): 4 espaços de padding antes do operando",
            curto.GetInstructionString().substr(0, 7) == "add    ");

        InstructionSimplified limite(0);
        limite.Parse("cvt.s.w f1, f2");
        // "cvt.s.w" já tem exatamente 7 chars -> o "for" de padding não roda nenhuma vez.
        // Resultado: nenhum separador é inserido antes do operando ("cvt.s.wf1, f2").
        // Quirk cosmético conhecido (não afeta parsing/validação) — sinalizado à parte.
        check("opcode no limite (7 chars): sem separador antes do operando (quirk conhecido)",
            limite.GetInstructionString().substr(0, 7) == "cvt.s.w" &&
            limite.GetInstructionString()[7] != ' ');
    }

    section("11.2 Label preserva a caixa original; registradores não");
    {
        InstructionSimplified i(0);
        i.Parse("beq R1, R2, MinhaLabel");
        check("registradores normalizados para minúsculo na string canônica",
            i.GetInstructionString().find("r1") != std::string::npos);
        check("label preserva exatamente a caixa escrita pelo usuário",
            i.GetInstructionString().find("MinhaLabel") != std::string::npos);
    }

    section("11.3 Formato específico de load/store — 'reg, off(base)'");
    {
        InstructionSimplified i(0);
        i.Parse("lw R1, 10(R2)");
        check("string reconstruída no formato 'r1, 10(r2)'",
            i.GetInstructionString().find("r1, 10(r2)") != std::string::npos);
    }

    // ════════════════════════════════════════════════════════════════════
    // 12. INTEGRAÇÃO
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("12. INTEGRAÇÃO");

    section("12.1 Múltiplas instruções — objetos independentes, sem vazamento de estado");
    {
        InstructionSimplified first(0);
        first.Parse("add r1, r2, r3");
        InstructionSimplified second(1);
        second.Parse("sw r4, 0(r5)");

        check("posição de cada instrução preservada",
            first.GetPosition() == 0 && second.GetPosition() == 1);
        check("dest_registers de 'first' não vaza para 'second'",
            !has_reg(second.GetDestRegisters(), 'R', 1));
        check("mem_source de 'second' não aparece em 'first'",
            first.GetMemSourceRegisters().empty());
    }

    section("12.2 Sequência com várias categorias — nenhuma quebra as outras");
    {
        std::vector<std::string> trace{
            "add r1, r2, r3", "lw r4, 0(r5)", "beq r1, r4, END",
            "mult r6, r7", "add.d f1, f2, f3"
        };
        std::vector<InstructionSimplified> parsed;
        for (size_t idx{}; idx < trace.size(); idx++) {
            InstructionSimplified inst(static_cast<int>(idx));
            inst.Parse(trace[idx]);
            parsed.push_back(inst);
        }

        check("5 instruções de categorias diferentes decodificadas sem abortar",
            parsed.size() == 5);
        check("tipos corretos, na ordem em que foram declaradas",
            parsed[0].GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC   &&
            parsed[1].GetInstructionType() == INSTRUCTION_TYPE::LOAD        &&
            parsed[2].GetInstructionType() == INSTRUCTION_TYPE::BRANCH      &&
            parsed[3].GetInstructionType() == INSTRUCTION_TYPE::INT_MUL     &&
            parsed[4].GetInstructionType() == INSTRUCTION_TYPE::FLOAT_BASIC);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";

    /*
    // ════════════════════════════════════════════════════════════════════
    // 13. CASOS DE FALHA (abortam a execução)
    // ════════════════════════════════════════════════════════════════════
    // Cada Parse() abaixo deveria terminar o processo via std::abort().
    // Como abort() mata o processo inteiro, testar vários no mesmo main()
    // impediria os casos seguintes (e todas as seções acima) de rodar.
    // Descomentar e rodar UM de cada vez para validar manualmente.

    section("[ABORT] Opcode desconhecido");
    {
        InstructionSimplified i(0);
        i.Parse("foo r1, r2, r3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] LOAD — falta o registrador base (quantidade errada de operandos)");
    {
        InstructionSimplified i(0);
        i.Parse("lw r1, 10");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Tipo de registrador errado (float onde se espera inteiro)");
    {
        InstructionSimplified i(0);
        i.Parse("add f1, r2, r3");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Imediato de operação sem '#' (obrigatório na sintaxe do livro)");
    {
        InstructionSimplified i(0);
        i.Parse("addi r1, r2, 5");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Imediato negativo em campo NÃO-ASSINADO (IMMU)");
    {
        InstructionSimplified i(0);
        i.Parse("andi r1, r2, #-5");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Offset de load/store com '#' (offset deve ser puro, sem '#')");
    {
        InstructionSimplified i(0);
        i.Parse("lw r1, #10(r2)");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] Label inválido — começa com dígito");
    {
        InstructionSimplified i(0);
        i.Parse("j 1LOOP");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }

    section("[ABORT] jalr com aridade fora de {2,3} (0 operandos)");
    {
        InstructionSimplified i(0);
        i.Parse("jalr");
        std::cout << "[FALHOU] Deveria ter abortado antes de chegar aqui!\n";
    }
    */

    return failed ? 1 : 0;
}
