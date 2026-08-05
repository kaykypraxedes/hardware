/* headers/InstructionFactory.h */
#ifndef INSTRUCTION_FACTORY_H
#define INSTRUCTION_FACTORY_H

#include "Instruction.h"
#include "../Instruction/headers/InstructionMips32.h"
#include "../Instruction/headers/InstructionX86Intel.h"
#include "../Instruction/headers/InstructionArm64.h"
#include "../Instruction/headers/InstructionRiscV.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <cstdlib>

namespace processor {

enum class Architecture {
    MIPS_32,
    X86_INTEL,
    ARM_64,
    RISC_V
};

class InstructionFactory {
public:
    static std::vector<std::unique_ptr<Instruction>> ParseTrace(
        const std::vector<std::string>& trace_lines,
        const Architecture arch)
    {
        std::vector<std::unique_ptr<Instruction>> instructions;
        int current_position = 0;

        for (const std::string& line : trace_lines) {
            std::unique_ptr<Instruction> inst;

            switch (arch) {
                case Architecture::MIPS_32:
                    inst = std::make_unique<InstructionMips32>(current_position);
                    break;
                case Architecture::X86_INTEL:
                    inst = std::make_unique<InstructionX86Intel>(current_position);
                    break;
                case Architecture::ARM_64:
                    inst = std::make_unique<InstructionArm64>(current_position);
                    break;
                case Architecture::RISC_V:
                    inst = std::make_unique<InstructionRiscV>(current_position);
                    break;
                default:
                    std::cerr << "[ERRO] Arquitetura não suportada!\n";
                    std::abort();
            }

            inst->Parse(line);
            instructions.push_back(std::move(inst));
            current_position++;
        }

        return instructions;
    }
};

} // namespace processor

#endif
