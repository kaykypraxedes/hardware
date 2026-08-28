/* tb_InstructionStages.cpp */
// Testbench isolado do plano vetorial de Instruction.
#include "../headers/Architecture.h"
#include "../headers/InstructionFactory.h"
#include "tb_Helpers.h"

using namespace processor;

// ─── CLASSES ──────────────────────────────────────────────────────

/**
 * @brief Instrução sintética usada para testar um plano multi-etapa.
 */
class SyntheticInstruction : public Instruction {
    public:
        SyntheticInstruction() : Instruction(0) {}

    private:
        std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const override {
            return {str};
        }

        bool SetStages(
            const std::vector<std::string>& tokens
        ) override {
            if (tokens[0] != "multi") return false;

            AddStage(INSTRUCTION_TYPE::LOAD, {}, {});
            AddStage(INSTRUCTION_TYPE::INT_MUL, {}, {});
            AddStage(INSTRUCTION_TYPE::STORE, {}, {});
            return true;
        }

        void NormalizeInstruction(
            std::vector<std::string>&
        ) override {}
};

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
    SyntheticInstruction instruction;
    instruction.Parse("multi");

    const std::vector<INSTRUCTION_TYPE> expected_types{
        INSTRUCTION_TYPE::LOAD,
        INSTRUCTION_TYPE::INT_MUL,
        INSTRUCTION_TYPE::STORE
    };
    const std::vector<int> expected_ex{
        Instruction::base_ex_latencies[static_cast<int>(INSTRUCTION_TYPE::LOAD)],
        Instruction::base_ex_latencies[static_cast<int>(INSTRUCTION_TYPE::INT_MUL)],
        Instruction::base_ex_latencies[static_cast<int>(INSTRUCTION_TYPE::STORE)]
    };
    const std::vector<int> expected_mem{
        Instruction::base_mem_latencies[0],
        0,
        Instruction::base_mem_latencies[1]
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
    instruction.SetLatencies({6, 8, 9}, {0, 0, 0});
    check("zeros de MEM restauram LOAD/STORE e preservam zero intermediário",
        instruction.GetMemLatencies() == expected_mem);

    print_title("2. REJEIÇÃO DE OVERRIDES INVÁLIDOS");

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
        Instruction::base_mem_latencies[0] = 0;
        SyntheticInstruction invalid;
        invalid.Parse("multi");
    }));

    print_title("3. COMPATIBILIDADE DAS ARQUITETURAS");

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
