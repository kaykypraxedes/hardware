/* tb_X86Intel.cpp */
// Testbench completo e isolado de X86Intel.cpp
#include "../Architectures/headers/X86Intel.h"
#include "tb_Helpers.h"
#include <iostream>
#include <limits>       // std::numeric_limits.
#include <set>          // std::set.
#include <sstream>      // std::ostringstream.
#include <tuple>        // std::tuple.
#include <vector>

using namespace processor;

// Função customizada para o testbench verificar type, id E máscara simultaneamente.
static bool has_mask(const std::vector<Register>& regs, char type, int id, int mask) {
    for (const auto& r : regs) {
        if (r.GetType() == type && r.GetId() == id && r.GetMask() == mask) return true;
    }
    return false;
}

static bool has_any_flag(const std::vector<Register>& regs) {
    for (const auto& reg : regs)
        if (reg.GetType() == 'G' && reg.GetId() >= 80 && reg.GetId() <= 85) return true;
    return false;
}

static bool has_all_flags(const std::vector<Register>& regs) {
    for (int id{80}; id <= 85; id++)
        if (!has_mask(regs, 'G', id, 0xFF)) return false;
    return true;
}

static bool has_unique_registers(const std::vector<Register>& regs) {
    std::set<std::tuple<char, int, int>> identities;
    for (const auto& reg : regs)
        identities.emplace(reg.GetType(), reg.GetId(), reg.GetMask());
    return identities.size() == regs.size();
}

// Confirma que as fontes pertencem exatamente às famílias físicas esperadas.
static bool HasOnlyRegisterFamilies(
    const std::vector<Register>& regs,
    const std::set<int>&         expected_ids
) {
    std::set<int> found_ids;
    for (const Register& reg : regs) {
        if (expected_ids.find(reg.GetId()) == expected_ids.end()) return false;
        found_ids.insert(reg.GetId());
    }
    return found_ids == expected_ids;
}

// Compara todos os aliases de GPR64 e os slots independentes XMM/flags esperados.
static bool HasExactRegisterFamilies(
    const std::vector<Register>& regs,
    const std::set<int>&         expected_gpr_ids,
    const std::set<int>&         expected_xmm_ids = {},
    const std::set<int>&         expected_flag_ids = {}
) {
    std::set<std::tuple<char, int, int>> expected;

    // Uma dependência GPR64 bloqueia todos os aliases sobrepostos da família.
    for (const int id : expected_gpr_ids) {
        expected.emplace('L', id, 0xFF);
        expected.emplace('R', id, 0x0F);
        expected.emplace('W', id, 0x03);
        expected.emplace('B', id, 0x01);
        if (id < 4) expected.emplace('B', id, 0x02);
    }
    for (const int id : expected_xmm_ids) expected.emplace('F', id, 0xFF);
    for (const int id : expected_flag_ids) expected.emplace('G', id, 0xFF);

    std::set<std::tuple<char, int, int>> found;
    for (const Register& reg : regs)
        found.emplace(reg.GetType(), reg.GetId(), reg.GetMask());
    return found == expected && found.size() == regs.size();
}

// Produz spelling hexadecimal portável para os limites do tipo unsigned long.
static std::string ToHexLiteral(
    unsigned long magnitude
) {
    std::ostringstream stream;
    stream << "0x" << std::hex << magnitude;
    return stream.str();
}

// Ids físicos desta arquitetura (ver RegisterTable() em X86Intel.cpp):
static constexpr int REG_A{0};
static constexpr int REG_B{1};
static constexpr int REG_C{2};
static constexpr int REG_D{3};
static constexpr int REG_SP{6};
static constexpr int REG_BP{7};
static constexpr int X86_R14_ID{14};
static constexpr int X86_R15_ID{15};
static constexpr int XMM0{32};
static constexpr int XMM1{33};
static constexpr int CF{80};

int main() {

    // ════════════════════════════════════════════════════════════════════
    // 1. PARSE E NORMALIZAÇÃO DE STRINGS
    // ════════════════════════════════════════════════════════════════════

    print_title("1. PARSE E NORMALIZAÇÃO DE STRINGS");

    section("1.1 Espaçamento variado e case-insensitive");
    {
        InstructionX86Intel a(0);
        a.Parse("ADD EAX, EBX");
        InstructionX86Intel b(0);
        b.Parse("add   eax , ebx");
        InstructionX86Intel c(0);
        c.Parse("AdD\teax,\tEBX");

        check("opcode e registradores são normalizados para minúsculo",
            a.GetInstructionString() == b.GetInstructionString() &&
            b.GetInstructionString() == c.GetInstructionString());
    }

    section("1.2 Normalização (instruction_string)");
    {
        InstructionX86Intel a(0);
        a.Parse("cvttss2si eax, xmm0");
        check("Opcodes grandes definem o padding (cvttss2si tem 9 chars)",
            a.GetInstructionString().find("cvttss2si eax, xmm0") != std::string::npos);

        InstructionX86Intel b(0);
        b.Parse("mov rax, [rbx + 4]");
        check("Colchetes e operandos são reconstruídos fielmente",
            b.GetInstructionString().find("mov       rax, [rbx + 4]") != std::string::npos);
    }

    section("1.3 Extração complexa de memória");
    {
        InstructionX86Intel a(0);
        a.Parse("mov eax, [rax + rbx*4 + 8]");
        check("extrai rax e rbx de dentro dos colchetes, ignorando 4 e 8",
            has_mask(a.GetExSourceRegisters(), 'L', REG_A, 0xFF) &&
            has_mask(a.GetExSourceRegisters(), 'L', REG_B, 0xFF));
    }

    section("1.4 Reconstrução de string com imediatos e memória complexa");
        {
            InstructionX86Intel i1(0);
            i1.Parse("mov rcx, [rax+rbx*4+8]");

            check("Preserva a formatação interna dos colchetes (imediatos e multiplicadores)",
                i1.GetInstructionString().find("mov       rcx, [rax + rbx * 4 + 8]") != std::string::npos);

            InstructionX86Intel i2(0);
            i2.Parse("add eax, -15");

            check("Preserva imediatos puros (negativos e constantes) na remontagem",
                i2.GetInstructionString().find("add       eax, -15") != std::string::npos);
        }

    // ════════════════════════════════════════════════════════════════════
    // 2. TESTES DE MASCARAMENTO E SOBREPOSIÇÃO FÍSICA
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("2. TESTES DE MASCARAMENTO E SOBREPOSIÇÃO FÍSICA");

    section("2.1 Isolamento de registradores parciais (al vs ah)");
    {
        InstructionX86Intel i1(0);
        i1.Parse("add al, 5");
        check("add al (0x01) bloqueia ele mesmo, ax (0x03), eax (0x0F) e rax (0xFF)",
            has_mask(i1.GetDestRegisters(), 'B', REG_A, 0x01) &&
            has_mask(i1.GetDestRegisters(), 'W', REG_A, 0x03) &&
            has_mask(i1.GetDestRegisters(), 'R', REG_A, 0x0F) &&
            has_mask(i1.GetDestRegisters(), 'L', REG_A, 0xFF));

        check("add al (0x01) NÃO deve bloquear o ah (0x02) livre",
            !has_mask(i1.GetDestRegisters(), 'B', REG_A, 0x02));

        InstructionX86Intel i2(0);
        i2.Parse("add ah, 5");
        check("add ah (0x02) NÃO deve bloquear o al (0x01)",
            !has_mask(i2.GetDestRegisters(), 'B', REG_A, 0x01));
    }

    section("2.2 Escrita cheia bloqueia todos os parciais (eax)");
    {
        InstructionX86Intel i(0);
        i.Parse("mov eax, 5");
        check("mov eax (0x0F) bloqueia rax, eax, ax, ah e al mutuamente",
            has_mask(i.GetDestRegisters(), 'L', REG_A, 0xFF) &&
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) &&
            has_mask(i.GetDestRegisters(), 'W', REG_A, 0x03) &&
            has_mask(i.GetDestRegisters(), 'B', REG_A, 0x01) &&
            has_mask(i.GetDestRegisters(), 'B', REG_A, 0x02));
    }

    section("2.3 Auditoria tabelada de todos os aliases GPR");
    {
        struct ALIAS_CASE {
            std::string name;
            char        type;
            int         id;
            int         mask;
        };

        const char* l64[]  = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp"};
        const char* r32[]  = {"eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp"};
        const char* w16[]  = {"ax",  "bx",  "cx",  "dx",  "si",  "di",  "sp",  "bp"};
        const char* low8[] = {"al",  "bl",  "cl",  "dl",  "sil", "dil", "spl", "bpl"};
        const char* high8[] = {"ah", "bh", "ch", "dh"};

        bool base_aliases_ok{true};
        for (int id{}; id < 8; id++) {
            const ALIAS_CASE aliases[] = {
                {l64[id],  'L', id, 0xFF},
                {r32[id],  'R', id, 0x0F},
                {w16[id],  'W', id, 0x03},
                {low8[id], 'B', id, 0x01}
            };
            for (const auto& alias : aliases) {
                InstructionX86Intel instruction(0);
                instruction.Parse(std::string("mov ") + alias.name + ", 1");
                base_aliases_ok = base_aliases_ok && has_mask(
                    instruction.GetDestRegisters(), alias.type, alias.id, alias.mask
                );
            }
        }
        for (int id{}; id < 4; id++) {
            InstructionX86Intel instruction(0);
            instruction.Parse(std::string("mov ") + high8[id] + ", 1");
            base_aliases_ok = base_aliases_ok &&
                has_mask(instruction.GetDestRegisters(), 'B', id, 0x02);
        }
        check("A/B/C/D/SI/DI/SP/BP possuem aliases e máscaras corretos", base_aliases_ok);

        bool extended_aliases_ok{true};
        for (int id{8}; id < 16; id++) {
            const std::string stem{"r" + std::to_string(id)};
            const ALIAS_CASE aliases[] = {
                {stem,       'L', id, 0xFF},
                {stem + "d", 'R', id, 0x0F},
                {stem + "w", 'W', id, 0x03},
                {stem + "b", 'B', id, 0x01}
            };
            for (const auto& alias : aliases) {
                InstructionX86Intel instruction(0);
                instruction.Parse(std::string("mov ") + alias.name + ", 1");
                extended_aliases_ok = extended_aliases_ok && has_mask(
                    instruction.GetDestRegisters(), alias.type, alias.id, alias.mask
                );
            }
        }
        check("R8-R15 possuem aliases L/R/W/B e ids compartilhados", extended_aliases_ok);
    }

    section("2.4 Matriz low8/high8 e ausência de duplicatas");
    {
        const char* low8[]  = {"al", "bl", "cl", "dl"};
        const char* high8[] = {"ah", "bh", "ch", "dh"};
        bool byte_matrix_ok{true};
        for (int id{}; id < 4; id++) {
            InstructionX86Intel low_instruction(0);
            low_instruction.Parse(std::string("add ") + low8[id] + ", 1");
            InstructionX86Intel high_instruction(0);
            high_instruction.Parse(std::string("add ") + high8[id] + ", 1");

            byte_matrix_ok = byte_matrix_ok &&
                !has_mask(low_instruction.GetDestRegisters(), 'B', id, 0x02) &&
                !has_mask(high_instruction.GetDestRegisters(), 'B', id, 0x01) &&
                has_unique_registers(low_instruction.GetDestRegisters()) &&
                has_unique_registers(high_instruction.GetDestRegisters());
        }
        check("low8/high8 de A/B/C/D permanecem independentes", byte_matrix_ok);
    }

    // ════════════════════════════════════════════════════════════════════
    // 3. O POLIMORFISMO DA FAMÍLIA MOV
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("3. O POLIMORFISMO DA FAMÍLIA MOV");

    section("3.1 MOV como ALU (Reg-Reg / Reg-Imm)");
    {
        InstructionX86Intel rr(0);
        rr.Parse("mov eax, ebx");
        check("mov reg, reg: GetInstructionType() == INT_BASIC",
            rr.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("mov reg, reg: dest == {eax} (não escreve flags)",
            has_mask(rr.GetDestRegisters(), 'R', REG_A, 0x0F) && !has_any_flag(rr.GetDestRegisters()));
        check("mov reg, reg: ex_source == {ebx} (NÃO LÊ o eax antes de gravar)",
            has_mask(rr.GetExSourceRegisters(), 'R', REG_B, 0x0F) && !has_mask(rr.GetExSourceRegisters(), 'R', REG_A, 0x0F));
    }

    section("3.2 MOV como LOAD (Reg-Mem)");
    {
        InstructionX86Intel ld(0);
        ld.Parse("mov eax, [rbx]");
        check("mov reg, [mem]: GetInstructionType() == LOAD",
            ld.GetInstructionType() == INSTRUCTION_TYPE::LOAD);
        check("mov reg, [mem]: dest == {eax}, ex_source == {ebx}",
            has_mask(ld.GetDestRegisters(), 'R', REG_A, 0x0F) && has_mask(ld.GetExSourceRegisters(), 'R', REG_B, 0x0F));
    }

    section("3.3 MOV como STORE (Mem-Reg e Mem-Imm)");
    {
        InstructionX86Intel st(0);
        st.Parse("mov [rax], ebx");
        check("mov [mem], reg: GetInstructionType() == STORE",
            st.GetInstructionType() == INSTRUCTION_TYPE::STORE);
        check("mov [mem], reg: mem_source == {ebx}, ex_source == {eax}",
            has_mask(st.GetMemSourceRegisters(), 'R', REG_B, 0x0F) && has_mask(st.GetExSourceRegisters(), 'R', REG_A, 0x0F));

        InstructionX86Intel sti(0);
        sti.Parse("mov [rax], 10");
        check("mov [mem], imm: imediato não gera dependência em mem_source",
            sti.GetMemSourceRegisters().empty());
    }

    section("3.4 MOVSX / MOVZX (Pure Write & Size Change)");
    {
        InstructionX86Intel ms(0);
        ms.Parse("movsx eax, bl");
        check("movsx (reg, reg): INT_BASIC, dest == {eax}, ex_source == {bl}",
            ms.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
            has_mask(ms.GetDestRegisters(), 'R', REG_A, 0x0F) && has_mask(ms.GetExSourceRegisters(), 'B', REG_B, 0x01));
        check("movsx (reg, reg): NÃO LÊ o destino antes (Pure Write)",
            !has_mask(ms.GetExSourceRegisters(), 'R', REG_A, 0x0F));
    }

    // ════════════════════════════════════════════════════════════════════
    // 4. INT_BASIC E DEPENDÊNCIAS DO EFLAGS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("4. INT_BASIC E DEPENDÊNCIAS DE FLAGS");

    section("4.1 ADD padrão (2 operandos destrutivos)");
    {
        InstructionX86Intel i(0);
        i.Parse("add eax, ebx");
        check("add: dest inclui eax e as seis flags rastreadas",
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) && has_all_flags(i.GetDestRegisters()));
        check("add: ex_source == {eax, ebx} (lê destino e fonte)",
            has_mask(i.GetExSourceRegisters(), 'R', REG_A, 0x0F) && has_mask(i.GetExSourceRegisters(), 'R', REG_B, 0x0F));
    }

    section("4.2 Read-Modify-Write em Memória (add [mem], reg)");
    {
        InstructionX86Intel i(0);
        i.Parse("add [rax], ebx");
        check("add [mem]: ex_source precisa puxar a base da memória (eax) E o valor (ebx)",
            has_mask(i.GetExSourceRegisters(), 'R', REG_A, 0x0F) && has_mask(i.GetExSourceRegisters(), 'R', REG_B, 0x0F));
    }

    section("4.3 ADC (Soma com Carry) - Lê CF");
    {
        InstructionX86Intel i(0);
        i.Parse("adc eax, ebx");
        check("adc: ex_source inclui CF anterior",
            has_mask(i.GetExSourceRegisters(), 'G', CF, 0xFF));
    }

    section("4.4 NOT / INC - Exceções e unários");
    {
        InstructionX86Intel i(0);
        i.Parse("not eax");
        check("not: NÃO altera flags", !has_any_flag(i.GetDestRegisters()));

        InstructionX86Intel inc(0);
        inc.Parse("inc eax");
        check("inc: afeta flags rastreadas e o próprio EAX",
            has_mask(inc.GetDestRegisters(), 'R', REG_A, 0x0F) && has_all_flags(inc.GetDestRegisters()));
    }

    section("4.5 CMP e TEST - Apenas afetam Flags");
    {
        InstructionX86Intel i(0);
        i.Parse("cmp eax, ebx");
        check("cmp: dest contém somente flags (não sobrescreve eax)",
            has_all_flags(i.GetDestRegisters()) && !has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F));
    }

    // ════════════════════════════════════════════════════════════════════
    // 5. LEA VS MEMORY ACCESS
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("5. LEA (Load Effective Address)");

    section("5.1 LEA - Cálculo puro sem acessar RAM");
    {
        InstructionX86Intel i(0);
        i.Parse("lea eax, [rbx + rcx]");
        check("lea: GetInstructionType() == INT_BASIC (Não é LOAD!)",
            i.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC);
        check("lea: dest == {eax}, ex_source == {ebx, ecx} (sem flags)",
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) &&
            !has_any_flag(i.GetDestRegisters()) &&
            has_mask(i.GetExSourceRegisters(), 'R', REG_B, 0x0F) &&
            has_mask(i.GetExSourceRegisters(), 'R', REG_C, 0x0F));
    }

    // ════════════════════════════════════════════════════════════════════
    // 6. STACK OPERATIONS (PUSH / POP)
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("6. STACK OPERATIONS (RSP IMPLÍCITO)");

    section("6.1 PUSH - Escreve na pilha e decrementa RSP");
    {
        InstructionX86Intel i(0);
        i.Parse("push rax");
        check("push: dest == {rsp} (L, 6), mem_source == {rax} (L, 0)",
            has_mask(i.GetDestRegisters(), 'L', REG_SP, 0xFF) && has_mask(i.GetMemSourceRegisters(), 'L', REG_A, 0xFF));
    }

    section("6.2 POP - Lê da pilha e incrementa RSP");
    {
        InstructionX86Intel i(0);
        i.Parse("pop rax");
        check("pop: dest == {rsp, rax}",
            has_mask(i.GetDestRegisters(), 'L', REG_SP, 0xFF) && has_mask(i.GetDestRegisters(), 'L', REG_A, 0xFF));
    }

    // ════════════════════════════════════════════════════════════════════
    // 7. IMPLICIT MULTIPLY / DIVIDE
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("7. MULTIPLY / DIVIDE (REGISTRADORES IMPLÍCITOS)");

    section("7.1 MUL - 1 operando (32-bit: EAX:EDX)");
    {
        InstructionX86Intel i(0);
        i.Parse("mul ebx");
        check("mul: GetInstructionType() == INT_MUL", i.GetInstructionType() == INSTRUCTION_TYPE::INT_MUL);
        check("mul: dest inclui eax, edx e flags rastreadas",
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) &&
            has_mask(i.GetDestRegisters(), 'R', REG_D, 0x0F) &&
            has_all_flags(i.GetDestRegisters()));
    }

    section("7.2 DIV - 1 operando (Leitura implícita)");
    {
        InstructionX86Intel i(0);
        i.Parse("div ebx");
        check("div: ex_source == {eax, edx, ebx}",
            has_mask(i.GetExSourceRegisters(), 'R', REG_A, 0x0F) &&
            has_mask(i.GetExSourceRegisters(), 'R', REG_D, 0x0F) &&
            has_mask(i.GetExSourceRegisters(), 'R', REG_B, 0x0F));
    }

    section("7.3 IMUL - 2 operandos (Multiplicação Moderna)");
    {
        InstructionX86Intel i(0);
        i.Parse("imul eax, ebx");
        check("imul (2 op): dest inclui eax e flags (NÃO usa edx)",
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) &&
            has_all_flags(i.GetDestRegisters()) &&
            !has_mask(i.GetDestRegisters(), 'R', REG_D, 0x0F));
    }

    // ════════════════════════════════════════════════════════════════════
    // 8. BRANCHES E CALL/RET
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("8. BRANCHES E SUB-ROTINAS");

    section("8.1 JMP - Incondicional Direto e Indireto");
    {
        InstructionX86Intel j1(0);
        j1.Parse("jmp LABEL");
        check("jmp LABEL: não possui registradores dependentes", j1.GetExSourceRegisters().empty());

        InstructionX86Intel j2(0);
        j2.Parse("jmp eax");
        check("jmp reg (Indireto): ex_source == {eax}", has_mask(j2.GetExSourceRegisters(), 'R', REG_A, 0x0F));
    }

    section("8.2 JCC (Ex: je, jne) - Condicionais");
    {
        InstructionX86Intel i(0);
        i.Parse("je LABEL");
        check("je: ex_source inclui flags rastreadas", has_all_flags(i.GetExSourceRegisters()));
    }

    section("8.3 CALL e RET - Pilha Implícita");
    {
        InstructionX86Intel call(0);
        call.Parse("call LABEL");
        check("call: dest == {rsp}, ex_source == {rsp}",
            has_mask(call.GetDestRegisters(), 'L', REG_SP, 0xFF) && has_mask(call.GetExSourceRegisters(), 'L', REG_SP, 0xFF));

        InstructionX86Intel ret(0);
        ret.Parse("ret");
        check("ret: dest == {rsp}, ex_source == {rsp}",
            has_mask(ret.GetDestRegisters(), 'L', REG_SP, 0xFF) && has_mask(ret.GetExSourceRegisters(), 'L', REG_SP, 0xFF));
    }

    // ════════════════════════════════════════════════════════════════════
    // 9. SIMD / FLOAT
    // ════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("9. SIMD E FLOAT (SSE)");

    section("9.1 ADDSS e MULPS - Aritmética SIMD");
    {
        InstructionX86Intel i(0);
        i.Parse("addss xmm0, xmm1");
        check("addss: dest == {xmm0}, ex_source == {xmm0, xmm1}",
            has_mask(i.GetDestRegisters(), 'F', XMM0, 0xFF) &&
            has_mask(i.GetExSourceRegisters(), 'F', XMM0, 0xFF) &&
            has_mask(i.GetExSourceRegisters(), 'F', XMM1, 0xFF));
        check("addss: NÃO altera flags", !has_any_flag(i.GetDestRegisters()));
    }

    section("9.2 CVTTSS2SI - Escrita pura Float para Int");
    {
        InstructionX86Intel i(0);
        i.Parse("cvttss2si eax, xmm0");
        check("cvttss2si: dest == {eax} (sem ler o eax antigo)",
            has_mask(i.GetDestRegisters(), 'R', REG_A, 0x0F) && !has_mask(i.GetExSourceRegisters(), 'R', REG_A, 0x0F));
        check("cvttss2si: ex_source == {xmm0}",
            has_mask(i.GetExSourceRegisters(), 'F', XMM0, 0xFF));
    }

    section("9.3 COMISS - Comparação SIMD -> flags");
    {
        InstructionX86Intel i(0);
        i.Parse("comiss xmm0, xmm1");
        check("comiss: dest inclui flags, ex_source == {xmm0, xmm1}",
            has_all_flags(i.GetDestRegisters()) &&
            has_mask(i.GetExSourceRegisters(), 'F', XMM0, 0xFF) &&
            has_mask(i.GetExSourceRegisters(), 'F', XMM1, 0xFF));
    }

    // ═════════════════════════════════════════════════════════════════════
    // 10. BANCO FÍSICO E FAIXAS DE IMPRESSÃO
    // ═════════════════════════════════════════════════════════════════════

    std::cout << "\n";
    print_title("10. BANCO FÍSICO E FAIXAS DE IMPRESSÃO");

    section("10.1 Slots esperados, identidade e isolamento");
    {
        const REGISTER_LAYOUT layout{InstructionX86Intel::MakeRegisterLayout()};
        const std::vector<Register>& references{layout.references};
        check("layout x86 possui exatamente 90 slots", references.size() == 90);
        check("Cada slot possui identidade (type,id,mask) única", has_unique_registers(references));

        bool xmm_ok{true};
        for (int id{32}; id < 48; id++)
            xmm_ok = xmm_ok && has_mask(references, 'F', id, 0xFF);
        check("XMM0-XMM15 existem e ocupam slots distintos", xmm_ok);

        bool flags_ok{true};
        for (int id{80}; id <= 85; id++)
            flags_ok = flags_ok && has_mask(references, 'G', id, 0xFF);
        check("CF/PF/AF/ZF/SF/OF existem em slots independentes", flags_ok);
    }

    section("10.2 banks cobre o layout uma vez e sem overflow");
    {
        const REGISTER_LAYOUT layout{InstructionX86Intel::MakeRegisterLayout()};
        std::vector<bool> covered(layout.references.size(), false);
        bool ranges_ok{true};
        for (const REGISTER_BANK& bank : layout.banks) {
            if (bank.base < 0 || bank.count < 0 ||
                static_cast<size_t>(bank.base + bank.count) > layout.references.size()) {
                ranges_ok = false;
                continue;
            }
            for (int position{bank.base}; position < bank.base + bank.count; position++) {
                if (covered[position]) ranges_ok = false;
                covered[position] = true;
            }
        }
        for (const bool slot_covered : covered)
            if (!slot_covered) ranges_ok = false;
        check("banks possui ranges válidos, sem sobreposição ou omissão", ranges_ok);
    }

    std::cout << "\n";
    print_title("11. TOKENIZER E OPERANDOS TIPADOS");

    section("11.1 Normalização canônica e endereçamento válido");
    {
        const unsigned long negative_limit{
            static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1
        };
        const std::string long_max{std::to_string(std::numeric_limits<long>::max())};
        const std::string long_min_magnitude{std::to_string(negative_limit)};
        const std::string long_max_hex{ToHexLiteral(static_cast<unsigned long>(std::numeric_limits<long>::max()))};
        const std::string long_min_hex{ToHexLiteral(negative_limit)};
        const std::vector<std::pair<std::string, std::string>> cases{
            {"mov rdx, [rax]", "mov       rdx, [rax]"},
            {"mov rdx, [r15]", "mov       rdx, [r15]"},
            {"mov rdx, [rsp]", "mov       rdx, [rsp]"},
            {"mov rdx, [r12+rbx]", "mov       rdx, [r12 + rbx]"},
            {"mov rdx, [rax+10]", "mov       rdx, [rax + 10]"},
            {" MOV RAX , [ RBP-8 ] ", "mov       rax, [rbp - 8]"},
            {"mov rax,[rbx+r14]", "mov       rax, [rbx + r14]"},
            {"mov rdx, [rax+rbx*1]", "mov       rdx, [rax + rbx]"},
            {"mov rdx, [rax+rbx*2]", "mov       rdx, [rax + rbx * 2]"},
            {"mov rdx, [rax+rbx*4]", "mov       rdx, [rax + rbx * 4]"},
            {"mov rdx, [rax+rbx*8]", "mov       rdx, [rax + rbx * 8]"},
            {"mov rdx, [rax+rbx*1+10]", "mov       rdx, [rax + rbx * 1 + 10]"},
            {"mov rdx, [rax+rbx*4-10]", "mov       rdx, [rax + rbx * 4 - 10]"},
            {"mov rax, [r15+r14*8+2147483647]", "mov       rax, [r15 + r14 * 8 + 2147483647]"},
            {"mov rdx, [rax+rax*2]", "mov       rdx, [rax + rax * 2]"},
            {"mov rdx, [r14*1]", "mov       rdx, [r14 * 1]"},
            {"mov rdx, [r14*2+10]", "mov       rdx, [r14 * 2 + 10]"},
            {"mov rax, [r14*2-0x10]", "mov       rax, [r14 * 2 - 0x10]"},
            {"mov rdx, [" + long_max + "]", "mov       rdx, [" + long_max + "]"},
            {"mov rdx, [-" + long_min_magnitude + "]", "mov       rdx, [-" + long_min_magnitude + "]"},
            {"mov rdx, [" + long_max_hex + "]", "mov       rdx, [" + long_max_hex + "]"},
            {"mov rdx, [-" + long_min_hex + "]", "mov       rdx, [-" + long_min_hex + "]"},
            {"mov rax, [rip+32]", "mov       rax, [rip + 32]"},
            {"mov rax, [rip-32]", "mov       rax, [rip - 32]"},
            {"mov rax, [rip+" + long_max + "]", "mov       rax, [rip + " + long_max + "]"},
            {"mov rax, [rip-" + long_min_magnitude + "]", "mov       rax, [rip - " + long_min_magnitude + "]"},
            {"mov rax, [rip+" + long_max_hex + "]", "mov       rax, [rip + " + long_max_hex + "]"},
            {"mov rax, [rip-" + long_min_hex + "]", "mov       rax, [rip - " + long_min_hex + "]"},
            {"mov rax, [rip+MinhaLabel]", "mov       rax, [rip + MinhaLabel]"},
            {"mov al, BYTE ptr [rbx]", "mov       al, byte ptr [rbx]"},
            {"mov ax, WoRd PtR [rbx]", "mov       ax, word ptr [rbx]"},
            {"mov eax, DWORD PTR [rbx]", "mov       eax, dword ptr [rbx]"},
            {"mov rax, QWORD PTR [rsp]", "mov       rax, qword ptr [rsp]"},
            {"mov xmm0, XMMWORD ptr [rax]", "mov       xmm0, xmmword ptr [rax]"},
            {"jmp MinhaLabel", "jmp       MinhaLabel"},
            {"add rax, 0xffffffffffffffff", "add       rax, 0xffffffffffffffff"},
            {"add rax, -0x8000000000000000", "add       rax, -0x8000000000000000"}
        };
        bool canonical{true};
        for (const auto& [input, expected] : cases) {
            InstructionX86Intel first(0);
            first.Parse(input);
            InstructionX86Intel second(0);
            second.Parse(first.GetInstructionString());
            canonical = canonical && first.GetInstructionString() == expected &&
                        second.GetInstructionString() == expected;
        }
        check("formas válidas normalizam de modo exato e estável", canonical);
    }

    section("11.2 Tipos escalares e limites nativos");
    {
        const unsigned long negative_limit{
            static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1
        };
        const std::string unsigned_max{
            std::to_string(std::numeric_limits<unsigned long>::max())
        };
        const std::string signed_min_magnitude{std::to_string(negative_limit)};
        const std::vector<std::pair<std::string, std::string>> cases{
            {"add RaX, 00042", "add       rax, 42"},
            {"add rax, -00042", "add       rax, -42"},
            {"add rax, 0X000A", "add       rax, 0xa"},
            {"add rax, -0X000A", "add       rax, -0xa"},
            {"add rax, " + unsigned_max, "add       rax, " + unsigned_max},
            {"add rax, -" + signed_min_magnitude,
             "add       rax, -" + signed_min_magnitude},
            {"jmp _MinhaLabel123", "jmp       _MinhaLabel123"}
        };
        bool parsed{true};
        for (const auto& [input, expected] : cases) {
            InstructionX86Intel instruction(0);
            instruction.Parse(input);
            parsed = parsed && instruction.GetInstructionString() == expected;
        }
        check("registradores, labels e limites decimais/hexadecimais são canônicos", parsed);

        InstructionX86Intel register_target(0);
        register_target.Parse("jmp RaX");
        check("registrador conhecido nunca é reclassificado como label",
            has_mask(register_target.GetExSourceRegisters(), 'L', REG_A, 0xFF));
    }

    section("11.3 Fontes de endereço e ciclo de vida de Parse");
    {
        InstructionX86Intel address(0);
        address.Parse("mov rcx, [rax+rbx*4+10]");
        check("componentes são armazenados e somente base/index viram fontes",
            address.GetInstructionString() == "mov       rcx, [rax + rbx * 4 + 10]" &&
            HasOnlyRegisterFamilies(address.GetExSourceRegisters(), {REG_A, REG_B}) &&
            has_unique_registers(address.GetExSourceRegisters()));

        InstructionX86Intel same_family(0);
        same_family.Parse("mov rcx, [rax+rax*2]");
        check("base e index da mesma família não duplicam dependências",
            HasOnlyRegisterFamilies(same_family.GetExSourceRegisters(), {REG_A}) &&
            has_unique_registers(same_family.GetExSourceRegisters()));

        InstructionX86Intel store(0);
        store.Parse("mov [rax+rbx*4+10], rcx");
        check("STORE separa fontes de endereço em EX e valor armazenado em MEM",
            store.GetDestRegisters().empty() &&
            HasOnlyRegisterFamilies(store.GetExSourceRegisters(), {REG_A, REG_B}) &&
            HasOnlyRegisterFamilies(store.GetMemSourceRegisters(), {REG_C}) &&
            has_unique_registers(store.GetExSourceRegisters()) &&
            has_unique_registers(store.GetMemSourceRegisters()));

        InstructionX86Intel lea(0);
        lea.Parse("lea rcx, [rax+rbx*4+10]");
        check("LEA consome endereço tipado sem acesso MEM ou flags",
            lea.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
            HasOnlyRegisterFamilies(lea.GetDestRegisters(), {REG_C}) &&
            HasOnlyRegisterFamilies(lea.GetExSourceRegisters(), {REG_A, REG_B}) &&
            lea.GetMemSourceRegisters().empty() && !has_any_flag(lea.GetDestRegisters()));

        InstructionX86Intel call(0);
        call.Parse("call [rax+rbx*4+10]");
        check("CALL indireto consome rsp, base e index tipados",
            call.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
            HasOnlyRegisterFamilies(call.GetDestRegisters(), {REG_SP}) &&
            HasOnlyRegisterFamilies(call.GetExSourceRegisters(), {REG_SP, REG_A, REG_B}) &&
            call.GetMemSourceRegisters().empty());

        InstructionX86Intel jump(0);
        jump.Parse("jmp [rax+rbx*4+10]");
        check("JMP indireto consome somente base e index tipados",
            jump.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
            jump.GetDestRegisters().empty() &&
            HasOnlyRegisterFamilies(jump.GetExSourceRegisters(), {REG_A, REG_B}) &&
            jump.GetMemSourceRegisters().empty());

        InstructionX86Intel sse_memory(0);
        sse_memory.Parse("comiss xmm0, [rax+rbx*4+10]");
        check("SSE com memória também consome base/index pelo helper tipado",
            HasOnlyRegisterFamilies(sse_memory.GetExSourceRegisters(), {XMM0, REG_A, REG_B}) &&
            has_all_flags(sse_memory.GetDestRegisters()) &&
            sse_memory.GetMemSourceRegisters().empty());

        InstructionX86Intel rip_relative(0);
        rip_relative.Parse("mov rcx, [rip+MinhaLabel]");
        check("RIP e label relativa não viram fontes renomeáveis",
            rip_relative.GetExSourceRegisters().empty());

        address.SetLatencies({99}, {99});
        address.Parse("jmp NovoAlvo");
        check("segundo Parse substitui texto, tipo, latências e dependências",
            address.GetInstructionString() == "jmp       NovoAlvo" &&
            address.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
            address.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.branch &&
            address.GetMemLatency() == 0 && address.GetDestRegisters().empty() &&
            address.GetExSourceRegisters().empty() && address.GetMemSourceRegisters().empty());
    }

    section("11.4 Rejeição determinística de sintaxe malformada");
    {
        const unsigned long negative_limit{
            static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1
        };
        const unsigned long negative_overflow{negative_limit + 1};
        std::vector<std::string> invalid{
            // F001, F004 e F006: entrada vazia, separadores e caracteres proibidos.
            "", " \t ", ",", "add rax rbx", "add , rax", "add rax,,rbx", "add rax,",
            "add rax, {rbx}", "add rax, (rbx)", "add rax, rbx;", "add rax, %rbx",
            "add rax, $1", "add rax, \"rbx\"", "add rax, 'rbx'",

            // F005 e F020-F026: colchetes e estrutura interna de memória.
            "mov rax, [rbx", "mov rax, rbx]", "mov rax, [[rbx]]", "mov rax, [rbx]]",
            "mov rax, []", "mov rax, [   ]", "mov rax, [rbx+]", "mov rax, [+rbx]",
            "mov rax, [rbx*]", "mov rax, [*rbx]", "mov rax, [rbx++4]", "mov rax, [rbx--8]",
            "mov rax, [rbx+-8]", "mov rax, [rbx**4]", "mov rax, [4*rbx]",
            "mov rax, [rbx*0]", "mov rax, [rbx*3]", "mov rax, [rbx*16]",
            "mov rax, [rax*rbx]", "mov rax, [4*8]", "mov rax, [rax+8*rbx]",
            "mov rax, [rax+rbx+rcx]", "mov rax, [rax+rbx+8]", "mov rax, [rax+rbx*4+rcx]",

            // A03/A16/A18 e F027-F030: classe/largura, SIB, RIP e símbolos.
            "mov rax, [eax]", "mov rax, [ax]", "mov rax, [al]", "mov rax, [xmm0]",
            "mov rax, [cf]", "mov rax, [eflags]", "mov rax, [rax+ebx]",
            "mov rax, [rax+rsp]", "mov rax, [rax+rsp*2]", "mov rax, [rax+r12]",
            "mov rax, [rax+r12*2]", "mov rax, [rip]", "mov rax, [rip*2]",
            "mov rax, [rip+rbx]", "mov rax, [rip+rip]", "mov rax, [rip+One+Two]",
            "mov rax, [fs:rax]", "mov rax, [Label]", "mov rax, byte [rbx]",
            "mov rax, qword [rbx]", "mov rax, ptr [rbx]", "mov rax, ptr qword [rbx]",
            "mov rax, octword ptr [rbx]", "mov rax, qword ptr ptr [rbx]",
            "mov rax, qword ptr qword ptr [rbx]", "mov rax, [rbx] qword ptr",

            // F008: sinais, tokens parciais e overflow decimal/hexadecimal.
            "add rax, +", "add rax, +1", "add rax, -", "add rax, --1", "add rax, -+1",
            "add rax, 0x", "add rax, 0X", "add rax, -0X", "add rax, 0x1g", "add rax, 1x",
            "add rax, 12abc",
            "add rax, 18446744073709551616", "add rax, -9223372036854775809",
            "add rax, 0x10000000000000000", "add rax, -0x8000000000000001",
            "mov rax, ["     + std::to_string(negative_limit)    + "]",
            "mov rax, [-"    + std::to_string(negative_overflow) + "]",
            "mov rax, ["     + ToHexLiteral(negative_limit)      + "]",
            "mov rax, [-"    + ToHexLiteral(negative_overflow)   + "]",
            "mov rax, [rip+" + std::to_string(negative_limit)    + "]",
            "mov rax, [rip-" + std::to_string(negative_overflow) + "]",
            "mov rax, [rip+" + ToHexLiteral(negative_limit)      + "]",
            "mov rax, [rip-" + ToHexLiteral(negative_overflow)   + "]",

            // F007 e pontuação residual fora da gramática.
            "jmp rip", "jmp 9Label", "jmp Bad-Label", "jmp Bad.Label", "mov rax, [rax@rbx]"
        };
        invalid.push_back(std::string("add rax, 1\0junk", 15));
        invalid.push_back(std::string("jmp A") + static_cast<char>(0xFF));

        check("helper compartilhado não confunde retorno normal com SIGABRT",
            !Aborts([]() {}));

        bool rejected{true};
        for (std::size_t case_id{}; case_id < invalid.size(); case_id++) {
            const std::string& input{invalid[case_id]};
            const bool did_abort{Aborts([&input]() {
                InstructionX86Intel parsed(0);
                parsed.Parse(input);
            })};
            if (!did_abort)
                std::cout << "  Caso inválido " << case_id << " aceito indevidamente: " << input << '\n';
            rejected = rejected && did_abort;
        }
        check("vírgulas, colchetes, caracteres, literais e endereços inválidos abortam", rejected);
    }

    section("11.5 Entrada grande permanece processável");
    {
        const std::string padding(200000, ' ');
        InstructionX86Intel instruction(0);
        instruction.Parse(padding + "mov rax, [rbx+r14*8+16]" + padding);
        check("espaçamento grande produz saída canônica determinística",
            instruction.GetInstructionString() == "mov       rax, [rbx + r14 * 8 + 16]");
    }

    section("11.6 Matriz T01-T20 completa");
    {
        struct TOKEN_CASE {
            const char* id;
            std::string input;
            std::string expected;
        };

        const std::vector<TOKEN_CASE> token_cases{
            {"T01", "add rax, rbx", "add       rax, rbx"},
            {"T02", "AdD RaX, RbX", "add       rax, rbx"},
            {"T03", "\tadd  \trax,\t rbx\t", "add       rax, rbx"},
            {"T04", "add rax , rbx", "add       rax, rbx"},
            {"T05", "mov rcx,[rax+rbx*4+10]", "mov       rcx, [rax + rbx * 4 + 10]"},
            {"T06", "mov rcx, [ rax + rbx * 4 + 10 ]", "mov       rcx, [rax + rbx * 4 + 10]"},
            {"T07", "mov rcx, [rbp-8]", "mov       rcx, [rbp - 8]"},
            {"T08", "mov rcx, [rax]", "mov       rcx, [rax]"},
            {"T09", "mov rcx, [rax+16]", "mov       rcx, [rax + 16]"},
            {"T10", "mov rcx, [rax+rbx]", "mov       rcx, [rax + rbx]"},
            {"T11", "mov rcx, [rax+rbx*8]", "mov       rcx, [rax + rbx * 8]"},
            {"T12", "mov rcx, [rbx*4+10]", "mov       rcx, [rbx * 4 + 10]"},
            {"T13", "mov rcx, [4096]", "mov       rcx, [4096]"},
            {"T14a", "mov rcx, [rip+32]", "mov       rcx, [rip + 32]"},
            {"T14b", "mov rcx, [rip+MinhaLabel]", "mov       rcx, [rip + MinhaLabel]"},
            {"T15", "jmp MinhaLabel", "jmp       MinhaLabel"},
            {"T16", "cvttss2si rax, xmm15", "cvttss2si rax, xmm15"},
            {"T17a", "add rax, 0", "add       rax, 0"},
            {"T17b", "add rax, 42", "add       rax, 42"},
            {"T17c", "add rax, -42", "add       rax, -42"},
            {"T17d", "add rax, 2147483647", "add       rax, 2147483647"},
            {"T17e", "add rax, -2147483648", "add       rax, -2147483648"},
            {"T17f", "add rax, 9223372036854775807", "add       rax, 9223372036854775807"},
            {"T17g", "add rax, -9223372036854775808", "add       rax, -9223372036854775808"},
            {"T18a", "add rax, 0X0", "add       rax, 0x0"},
            {"T18b", "add rax, 0X7F", "add       rax, 0x7f"},
            {"T18c", "add rax, 0X80000000", "add       rax, 0x80000000"},
            {"T18d", "add rax, -0X0", "add       rax, -0x0"},
            {"T18e", "add rax, -0X7F", "add       rax, -0x7f"},
            {"T18f", "add rax, -0X80000000", "add       rax, -0x80000000"}
        };

        bool token_matrix_ok{true};
        for (const TOKEN_CASE& test_case : token_cases) {
            InstructionX86Intel first(0);
            first.Parse(test_case.input);
            InstructionX86Intel round_trip(0);
            round_trip.Parse(first.GetInstructionString());

            const bool case_ok{first.GetInstructionString() == test_case.expected &&
                               round_trip.GetInstructionString() == test_case.expected};
            if (!case_ok) std::cout << "  Falha na matriz " << test_case.id << '\n';
            token_matrix_ok = token_matrix_ok && case_ok;
        }
        check("T01-T18 normalizam exatamente e permanecem estáveis", token_matrix_ok);

        // T19: objetos diferentes mantêm estado completamente independente.
        InstructionX86Intel first_object(1);
        first_object.Parse("mov rax, [rbx]");
        InstructionX86Intel second_object(2);
        second_object.Parse("jmp OutroAlvo");
        const bool independent_objects{
            first_object.GetInstructionString() == "mov       rax, [rbx]" &&
            first_object.GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
            first_object.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.load &&
            first_object.GetMemLatency() == PIPELINE_CONFIGURATION{}.memory_latencies.load &&
            HasExactRegisterFamilies(first_object.GetDestRegisters(), {REG_A}) &&
            HasExactRegisterFamilies(first_object.GetExSourceRegisters(), {REG_B}) &&
            first_object.GetMemSourceRegisters().empty() &&
            second_object.GetInstructionString() == "jmp       OutroAlvo" &&
            second_object.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
            second_object.GetDestRegisters().empty() &&
            second_object.GetExSourceRegisters().empty() &&
            second_object.GetMemSourceRegisters().empty()
        };
        check("T19 objetos diferentes não compartilham estado", independent_objects);

        // T20: novo Parse substitui texto, tipo, latências e todas as dependências.
        const std::set<int> all_flags{80, 81, 82, 83, 84, 85};
        InstructionX86Intel reusable(17);
        reusable.Parse("mov rax, [rbx]");
        reusable.SetLatencies({99}, {99});
        reusable.Parse("add rcx, rdx");
        const bool reusable_ok{
            reusable.GetPosition() == 17 &&
            reusable.GetInstructionString() == "add       rcx, rdx" &&
            reusable.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
            reusable.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.int_basic &&
            reusable.GetMemLatency() == 0 &&
            HasExactRegisterFamilies(reusable.GetDestRegisters(), {REG_C}, {}, all_flags) &&
            HasExactRegisterFamilies(reusable.GetExSourceRegisters(), {REG_C, REG_D}) &&
            reusable.GetMemSourceRegisters().empty()
        };
        check("T20 segundo Parse substitui todo o estado derivado", reusable_ok);
    }

    section("11.7 Matriz A01-A24 completa");
    {
        struct ADDRESS_CASE {
            const char* id;
            std::string input;
            std::string normalized;
            std::set<int> source_ids;
        };

        // Formas válidas são exercitadas como LOAD, STORE, LEA e operando r/m.
        const std::vector<ADDRESS_CASE> address_cases{
            {"A01", "[rax]", "[rax]", {REG_A}},
            {"A02", "[r15]", "[r15]", {X86_R15_ID}},
            {"A04", "[rax+10]", "[rax + 10]", {REG_A}},
            {"A05", "[rbp-10]", "[rbp - 10]", {REG_BP}},
            {"A06", "[rax+rbx]", "[rax + rbx]", {REG_A, REG_B}},
            {"A07", "[rax+rbx*1]", "[rax + rbx]", {REG_A, REG_B}},
            {"A08", "[rax+rbx*2]", "[rax + rbx * 2]", {REG_A, REG_B}},
            {"A09", "[rax+rbx*4]", "[rax + rbx * 4]", {REG_A, REG_B}},
            {"A10", "[rax+rbx*8]", "[rax + rbx * 8]", {REG_A, REG_B}},
            {"A11", "[rax+rbx*4+10]", "[rax + rbx * 4 + 10]", {REG_A, REG_B}},
            {"A12", "[rax+rbx*4-10]", "[rax + rbx * 4 - 10]", {REG_A, REG_B}},
            {"A13", "[r15+r14*8+2147483647]", "[r15 + r14 * 8 + 2147483647]", {X86_R15_ID, X86_R14_ID}},
            {"A14a", "[rip+MinhaLabel]", "[rip + MinhaLabel]", {}},
            {"A14b", "[rip+32]", "[rip + 32]", {}},
            {"A15", "[rax+rax*2]", "[rax + rax * 2]", {REG_A}},
            {"A17", "[rsp]", "[rsp]", {REG_SP}}
        };
        const std::set<int> all_flags{80, 81, 82, 83, 84, 85};
        bool address_matrix_ok{true};

        for (const ADDRESS_CASE& test_case : address_cases) {
            InstructionX86Intel load(0);
            load.Parse("mov rcx, " + test_case.input);
            const bool load_ok{
                load.GetInstructionString() == "mov       rcx, " + test_case.normalized &&
                load.GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
                load.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.load &&
                load.GetMemLatency() == PIPELINE_CONFIGURATION{}.memory_latencies.load &&
                HasExactRegisterFamilies(load.GetDestRegisters(), {REG_C}) &&
                HasExactRegisterFamilies(load.GetExSourceRegisters(), test_case.source_ids) &&
                load.GetMemSourceRegisters().empty()
            };

            InstructionX86Intel store(0);
            store.Parse("mov " + test_case.input + ", rcx");
            const bool store_ok{
                store.GetInstructionString() == "mov       " + test_case.normalized + ", rcx" &&
                store.GetInstructionType() == INSTRUCTION_TYPE::STORE &&
                store.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.store &&
                store.GetMemLatency() == PIPELINE_CONFIGURATION{}.memory_latencies.store &&
                store.GetDestRegisters().empty() &&
                HasExactRegisterFamilies(store.GetExSourceRegisters(), test_case.source_ids) &&
                HasExactRegisterFamilies(store.GetMemSourceRegisters(), {REG_C})
            };

            InstructionX86Intel lea(0);
            lea.Parse("lea rcx, " + test_case.input);
            const bool lea_ok{
                lea.GetInstructionString() == "lea       rcx, " + test_case.normalized &&
                lea.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
                lea.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.int_basic &&
                lea.GetMemLatency() == 0 &&
                HasExactRegisterFamilies(lea.GetDestRegisters(), {REG_C}) &&
                HasExactRegisterFamilies(lea.GetExSourceRegisters(), test_case.source_ids) &&
                lea.GetMemSourceRegisters().empty()
            };

            InstructionX86Intel read_modify(0);
            read_modify.Parse("add rcx, " + test_case.input);
            std::set<int> read_modify_sources{test_case.source_ids};
            read_modify_sources.insert(REG_C);
            const bool read_modify_ok{
                read_modify.GetInstructionString() == "add       rcx, " + test_case.normalized &&
                read_modify.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
                read_modify.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.int_basic &&
                read_modify.GetMemLatency() == 0 &&
                HasExactRegisterFamilies(read_modify.GetDestRegisters(), {REG_C}, {}, all_flags) &&
                HasExactRegisterFamilies(read_modify.GetExSourceRegisters(), read_modify_sources) &&
                read_modify.GetMemSourceRegisters().empty()
            };

            const bool case_ok{load_ok && store_ok && lea_ok && read_modify_ok};
            if (!case_ok) std::cout << "  Falha na matriz " << test_case.id << '\n';
            address_matrix_ok = address_matrix_ok && case_ok;
        }
        check("A01/A02/A04-A15/A17 passam nos quatro contextos", address_matrix_ok);

        // A19-A24: dependências exatas e sem duplicatas no endereço complexo.
        InstructionX86Intel same_family(0);
        same_family.Parse("mov rcx, [rax+rax*2]");
        const bool a19{HasExactRegisterFamilies(same_family.GetExSourceRegisters(), {REG_A})};

        InstructionX86Intel load(0);
        load.Parse("mov rcx, [rax+rbx*4+10]");
        const bool a20{load.GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
                       HasExactRegisterFamilies(load.GetDestRegisters(), {REG_C}) &&
                       HasExactRegisterFamilies(load.GetExSourceRegisters(), {REG_A, REG_B}) &&
                       load.GetMemSourceRegisters().empty()};

        InstructionX86Intel store(0);
        store.Parse("mov [rax+rbx*4+10], rcx");
        const bool a21{store.GetInstructionType() == INSTRUCTION_TYPE::STORE &&
                       store.GetDestRegisters().empty() &&
                       HasExactRegisterFamilies(store.GetExSourceRegisters(), {REG_A, REG_B}) &&
                       HasExactRegisterFamilies(store.GetMemSourceRegisters(), {REG_C})};

        InstructionX86Intel lea(0);
        lea.Parse("lea rcx, [rax+rbx*4+10]");
        const bool a22{lea.GetInstructionType() == INSTRUCTION_TYPE::INT_BASIC &&
                       HasExactRegisterFamilies(lea.GetDestRegisters(), {REG_C}) &&
                       HasExactRegisterFamilies(lea.GetExSourceRegisters(), {REG_A, REG_B}) &&
                       lea.GetMemSourceRegisters().empty() && !has_any_flag(lea.GetDestRegisters())};

        InstructionX86Intel call(0);
        call.Parse("call [rax+rbx*4+10]");
        const bool a23{call.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
                       call.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.branch &&
                       call.GetMemLatency() == 0 &&
                       HasExactRegisterFamilies(call.GetDestRegisters(), {REG_SP}) &&
                       HasExactRegisterFamilies(call.GetExSourceRegisters(), {REG_SP, REG_A, REG_B}) &&
                       call.GetMemSourceRegisters().empty()};

        InstructionX86Intel jump(0);
        jump.Parse("jmp [rax+rbx*4+10]");
        const bool a24{jump.GetInstructionType() == INSTRUCTION_TYPE::BRANCH &&
                       jump.GetExLatency() == PIPELINE_CONFIGURATION{}.execution_latencies.branch &&
                       jump.GetMemLatency() == 0 && jump.GetDestRegisters().empty() &&
                       HasExactRegisterFamilies(jump.GetExSourceRegisters(), {REG_A, REG_B}) &&
                       jump.GetMemSourceRegisters().empty()};

        check("A19-A24 preservam dependências e atributos exatos",
            a19 && a20 && a21 && a22 && a23 && a24);
    }

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";

    return failed ? 1 : 0;
}
