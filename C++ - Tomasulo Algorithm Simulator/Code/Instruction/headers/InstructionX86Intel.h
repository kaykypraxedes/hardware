/* Instruction/headers/InstructionX86Intel.h */
#ifndef INSTRUCTION_X86INTEL_H // Include guard
#define INSTRUCTION_X86INTEL_H
#include "../../headers/Instruction.h"
#include <algorithm>           // para std::find
#include <cctype>              // para std::toupper

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionX86Intel : public Instruction { // Herança da classe Instruction.
    public:
        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionX86Intel(
            const int = -1
        );

    protected:
        // Métodos "privados":
        // - override para implementar sua versão específica.
        std::vector<std::string> SplitInstruction(
            const std::string&
        ) const override;
        bool IdentifyType(
            const std::string&
        ) override;
        void NormalizeInstruction(
            std::vector<std::string>&
        ) override;
        void SetAttributes(
            const std::vector<std::string>&
        ) override;
    };

} // namespace processor

#endif
