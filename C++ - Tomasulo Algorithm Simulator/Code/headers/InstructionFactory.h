/* headers/InstructionFactory.h */
#ifndef INSTRUCTION_FACTORY_H
#define INSTRUCTION_FACTORY_H

#include "Instruction.h"
// Arquiteturas suportadas:
#include "../Instruction/headers/InstructionMips32.h"
#include "../Instruction/headers/InstructionX86Intel.h"
#include "../Instruction/headers/InstructionArm64.h"
#include "../Instruction/headers/InstructionRiscV.h"
#include <vector>
#include <string>
#include <memory>    // para std::unique_ptr
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────
enum class ARCHITECTURE {
    MIPS_32,
    X86_INTEL,
    ARM_64,
    RISC_V
};

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionFactory {
    public:
        // Elemento static:
        // Decodifica o código passado em vetores de string para um vetor de instruções.
        // - O vetor de instruções é genérico (por issa a adaptação, utilizando ponteiros).
        static std::vector<std::unique_ptr<Instruction>> ParseTrace(
            const std::vector<std::string>& trace_lines,
            const ARCHITECTURE arch
        ){
            std::vector<std::unique_ptr<Instruction>> instructions;
            int current_position{};

            for (const std::string& line : trace_lines) {
                std::unique_ptr<Instruction> inst;

                switch (arch) {
                    case ARCHITECTURE::MIPS_32:
                        inst = std::make_unique<InstructionMips32>(current_position);
                        break;
                    case ARCHITECTURE::X86_INTEL:
                        inst = std::make_unique<InstructionX86Intel>(current_position);
                        break;
                    case ARCHITECTURE::ARM_64:
                        inst = std::make_unique<InstructionArm64>(current_position);
                        break;
                    case ARCHITECTURE::RISC_V:
                        inst = std::make_unique<InstructionRiscV>(current_position);
                        break;
                    default:
                        std::cerr << "[ERRO] Arquitetura não suportada!\n";
                        std::abort();
                }
                // Traduz a instrução.
                inst->Parse(line);
                // Adiciona a instrução no vetor e passa para a próxima.
                // Como se trata de um unique_ptr, ele não pode ser simplesmente copiado:
                // - O std::move() garante que seu original tenha sido apagado, restando apenas 1.
                instructions.push_back(std::move(inst));
                current_position++;
            }

            return instructions;
        }
};
} // namespace processor

#endif
