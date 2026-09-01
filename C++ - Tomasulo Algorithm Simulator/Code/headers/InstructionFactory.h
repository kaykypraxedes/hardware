/* headers/InstructionFactory.h */

/**
 * @file InstructionFactory.h
 *
 * @brief Módulo responsável pela identificação da arquitetura,
 * criação da tabela de instruções e realizar o "parse" da instrução.
 */

#ifndef INSTRUCTION_FACTORY_H
#define INSTRUCTION_FACTORY_H

#include "Architecture.h"
#include <vector>
#include <string>
#include <memory>    // para std::unique_ptr
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr
// Arquiteturas suportadas:
#include "../Architectures/headers/ArchSimplified.h"
#include "../Architectures/headers/Arm64.h"
#include "../Architectures/headers/Mips64.h"
#include "../Architectures/headers/RiscV.h"
#include "../Architectures/headers/X86Intel.h"

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────

enum class ARCHITECTURE {
    SIMPLIFIED, // Arquitetura didática "Computer Architecture - A Quantitative Approach - Patterson & Hennessy"
    MIPS_32,
    X86_INTEL,
    ARM_64,
    RISC_V
};

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Classe onde ocorre a decodificação das instruções e a
 * montagem do layout do banco de registradores.
 */
class InstructionFactory {
    public:
        // Elementos static:

        /**
         * @brief Decodifica o código passado em vetores de string
         * para um vetor de instruções.
         *
         * @details O vetor de instruções é genérico (simulando o
         * agrupamento que pode ser feito em Java). Todavia, esse
         * método tem de ser adaptado utilizando ponteiros.
         *
         * @param const std::vector<std::string>& trace_lines - Vetor
         * de instruções (ainda em string).
         * @param const ARCHITECTURE arch - Arquitetura que deve ser
         * considerada.
         * @param configuration Configuração que materializa as latências.
         *
         * @return std::vector<std::unique_ptr<Instruction>> - Vetor
         * com o ponteiro para as instruções (já decodificadas).
         */
        static std::vector<std::unique_ptr<Instruction>> ParseTrace(
            const std::vector<std::string>& trace_lines,
            const ARCHITECTURE             arch,
            const PIPELINE_CONFIGURATION&  configuration = PIPELINE_CONFIGURATION{}
        ){
            std::vector<std::unique_ptr<Instruction>> instructions;
            int current_position{};

            for (const std::string& line : trace_lines) {
                std::unique_ptr<Instruction> inst;

                switch (arch) {
                    case ARCHITECTURE::SIMPLIFIED:
                        inst = std::make_unique<InstructionSimplified>(current_position);
                        break;
                    case ARCHITECTURE::MIPS_32:
                        inst = std::make_unique<InstructionMips64>(current_position);
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
                inst->Parse(line, configuration);
                // Adiciona a instrução no vetor e passa para a próxima.
                // Como se trata de um unique_ptr, ele não pode ser simplesmente copiado:
                // - O std::move() garante que seu original tenha sido apagado, restando apenas 1.
                instructions.push_back(std::move(inst));
                current_position++;
            }

            return instructions;
        }

        /**
         * @brief Monta o layout dos registradores físicos da
         * arquitetura.
         *
         * @details Cada subclasse define suas características
         *
         * - Slots;
         * - Classes
         * - Faixas de impressão;
         * ...
         *
         * @param const ARCHITECTURE arch - Arquitetura.
         *
         * @return Layout imutável do banco de registradores.
         */
        static REGISTER_LAYOUT MakeRegisterLayout(
            const ARCHITECTURE arch
        ){
            switch (arch) {
                case ARCHITECTURE::SIMPLIFIED: return InstructionSimplified::MakeRegisterLayout();
                case ARCHITECTURE::MIPS_32:    return InstructionMips64::MakeRegisterLayout();
                case ARCHITECTURE::X86_INTEL:  return InstructionX86Intel::MakeRegisterLayout();
                case ARCHITECTURE::ARM_64:     return InstructionArm64::MakeRegisterLayout();
                case ARCHITECTURE::RISC_V:     return InstructionRiscV::MakeRegisterLayout();
                default:
                    std::cerr << "[ERRO] Arquitetura não suportada!\n";
                    std::abort();
            }
        }
};
} // namespace processor

#endif
