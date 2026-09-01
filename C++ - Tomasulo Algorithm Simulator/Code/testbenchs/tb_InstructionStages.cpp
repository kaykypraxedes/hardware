/* tb_InstructionStages.cpp */
// Testbench isolado do plano vetorial de Instruction.
#include "../headers/Architecture.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"
#include "tb_SyntheticInstruction.h"

using namespace processor;

// ─── HELPERS ──────────────────────────────────────────────────────

// Confirma que uma arquitetura existente continua produzindo um plano unitário válido.
static bool HasValidUnitPlan(
    const ARCHITECTURE arch,
    const std::string& instruction
) {
    std::vector<std::unique_ptr<Instruction>> parsed{
        InstructionFactory::ParseTrace({instruction}, arch)
    };
    const Instruction& current{*parsed[0]};
    const INSTRUCTION_TYPE type{current.GetInstructionType()};
    const bool uses_memory{
        type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE
    };

    return current.GetInstructionTypes().size() == 1 &&
           current.GetExLatencies().size() == 1 &&
           current.GetMemLatencies().size() == 1 &&
           current.GetAllExSourceRegisters().size() == 1 &&
           current.GetAllMemSourceRegisters().size() == 1 &&
           current.GetExLatency() > 0 &&
           (uses_memory ? current.GetMemLatency() > 0 : current.GetMemLatency() == 0);
}

int main() {
    print_title("1. LATÊNCIAS VETORIAIS");

    section("1.1 Derivação das latências-base por etapa");
    const PIPELINE_CONFIGURATION default_configuration;
    SyntheticInstruction instruction(30);
    instruction.Parse("multi", default_configuration);

    const std::vector<INSTRUCTION_TYPE> expected_types{
        INSTRUCTION_TYPE::LOAD,
        INSTRUCTION_TYPE::INT_MUL,
        INSTRUCTION_TYPE::STORE
    };
    const std::vector<int> expected_ex{
        default_configuration.execution_latencies.load,
        default_configuration.execution_latencies.int_mult,
        default_configuration.execution_latencies.store
    };
    const std::vector<int> expected_mem{
        default_configuration.memory_latencies.load,
        0,
        default_configuration.memory_latencies.store
    };

    check("tipos preservam a ordem LOAD -> INT_MUL -> STORE",
        instruction.GetInstructionTypes() == expected_types);
    check("EX deriva a latência-base de cada tipo",
        instruction.GetExLatencies() == expected_ex);
    check("MEM existe somente nas etapas LOAD e STORE",
        instruction.GetMemLatencies() == expected_mem);

    section("1.2 Override completo e compatibilidade dos getters escalares");
    instruction.SetLatencies({3, 7, 5}, {2, 0, 4});
    check("override EX altera cada etapa correta",
        instruction.GetExLatencies() == std::vector<int>({3, 7, 5}));
    check("override MEM altera LOAD/STORE e mantém zero em INT_MUL",
        instruction.GetMemLatencies() == std::vector<int>({2, 0, 4}));
    check("getters escalares continuam consultando a primeira etapa",
        instruction.GetExLatency() == 3 && instruction.GetMemLatency() == 2);

    section("1.3 Zero em MEM restaura os valores-base");
    instruction.Parse("multi", default_configuration);
    instruction.SetLatencies({6, 8, 9}, {0, 0, 0});
    check("zeros de MEM restauram LOAD/STORE e preservam zero intermediário",
        instruction.GetMemLatencies() == expected_mem);

    print_title("2. DESCRIÇÃO DAS ETAPAS");

    check("fontes EX permanecem separadas por etapa",
        instruction.GetAllExSourceRegisters().size() == 3 &&
        instruction.GetAllExSourceRegisters()[0].size() == 1 &&
        instruction.GetAllExSourceRegisters()[1].size() == 2 &&
        instruction.GetAllExSourceRegisters()[2].size() == 1);
    check("fontes MEM existem somente na etapa STORE",
        instruction.GetAllMemSourceRegisters().size() == 3 &&
        instruction.GetAllMemSourceRegisters()[0].empty() &&
        instruction.GetAllMemSourceRegisters()[1].empty() &&
        instruction.GetAllMemSourceRegisters()[2].size() == 2);
    check("destino arquitetural permanece descritivo",
        instruction.GetDestRegisters().size() == 1 &&
        instruction.GetDestRegisters()[0].GetId() == 6);

    check("getters indexados consultam cada etapa sem alterar o plano",
        instruction.GetStageCount() == 3 &&
        instruction.GetInstructionType(0) == INSTRUCTION_TYPE::LOAD &&
        instruction.GetInstructionType(1) == INSTRUCTION_TYPE::INT_MUL &&
        instruction.GetInstructionType(2) == INSTRUCTION_TYPE::STORE &&
        instruction.GetExSourceRegisters(1).size() == 2 &&
        instruction.GetExSourceRegisters(1)[0].GetId() == 6 &&
        instruction.GetMemSourceRegisters(2).size() == 2);
    check("getters escalares preservam o significado original",
        instruction.GetInstructionType() == INSTRUCTION_TYPE::LOAD &&
        instruction.GetInstructionTypes() == expected_types);
    check("índice fora da descrição aborta", Aborts([&instruction]() {
        instruction.GetInstructionType(3);
    }));

    instruction.Parse("multi", default_configuration);
    check("Parse repetido restaura a mesma descrição completa",
        instruction.GetInstructionTypes() == expected_types &&
        instruction.GetAllExSourceRegisters().size() == 3);

    print_title("3. REJEIÇÃO DE OVERRIDES INVÁLIDOS");

    check("quantidade EX divergente aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, 2}, {1, 0, 1});
    }));
    check("quantidade MEM divergente aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, 2, 3}, {1, 0});
    }));
    check("EX zero aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, 0, 3}, {1, 0, 1});
    }));
    check("EX negativa aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, -2, 3}, {1, 0, 1});
    }));
    check("MEM negativa aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, 2, 3}, {-1, 0, 1});
    }));
    check("MEM positiva em etapa não-memória aborta", Aborts([&instruction]() {
        instruction.SetLatencies({1, 2, 3}, {1, 1, 1});
    }));
    check("MEM-base inválida em LOAD aborta durante Parse", Aborts([]() {
        PIPELINE_CONFIGURATION invalid_configuration;
        invalid_configuration.memory_latencies.load = 0;
        SyntheticInstruction invalid;
        invalid.Parse("multi", invalid_configuration);
    }));

    section("3.1 Configurações intercaladas não interferem");
    {
        PIPELINE_CONFIGURATION fast;
        PIPELINE_CONFIGURATION slow;
        fast.execution_latencies.int_mult = 2;
        slow.execution_latencies.int_mult = 17;

        SyntheticInstruction first(0);
        SyntheticInstruction second(1);
        SyntheticInstruction third(2);
        first.Parse("multi", fast);
        second.Parse("multi", slow);
        third.Parse("multi", fast);

        check("cada plano preserva sua configuração de instância",
            first.GetExLatency(1) == 2 &&
            second.GetExLatency(1) == 17 &&
            third.GetExLatency(1) == 2);
    }

    print_title("4. COMPATIBILIDADE DAS ARQUITETURAS");

    check("ArchSimplified mantém plano unitário",
        HasValidUnitPlan(ARCHITECTURE::SIMPLIFIED, "add r1, r2, r3"));
    check("Mips64 mantém plano unitário",
        HasValidUnitPlan(ARCHITECTURE::MIPS_32, "add r1, r2, r3"));
    check("Arm64 mantém plano unitário",
        HasValidUnitPlan(ARCHITECTURE::ARM_64, "add x0, x1, x2"));
    check("RiscV mantém plano unitário",
        HasValidUnitPlan(ARCHITECTURE::RISC_V, "add x1, x2, x3"));
    check("X86Intel mantém plano unitário",
        HasValidUnitPlan(ARCHITECTURE::X86_INTEL, "add eax, ebx"));

    std::cout << "\n-----------------------------\n";
    std::cout << "Resultado: " << passed << " OK, " << failed << " FALHOU\n";
    return failed ? 1 : 0;
}
