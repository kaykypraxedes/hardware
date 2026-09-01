/* Architectures/headers/ArchSimplified.h */
#ifndef ARCH_SIMPLIFIED_H // Include guard
#define ARCH_SIMPLIFIED_H
#include "../../headers/Architecture.h"
#include <cctype>         // para std::tolower

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionSimplified : public Instruction { // Herança da classe Instruction.
    public:
        // Método estático:
        // Monta o layout de registradores físicos da arquitetura.
        static REGISTER_LAYOUT MakeRegisterLayout();

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionSimplified(
            const int = -1
        );

    protected:
        // Métodos "privados":
        // - override para implementar sua versão específica.
        std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const override;
        bool SetStages(
            const std::vector<std::string>& tokens
        ) override;
        void ValidateInstruction(
            const std::vector<std::string>& tokens
        );
        void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) override;
        void SetStageAttributes(
            const std::vector<std::string>& tokens,
            std::vector<Register>&          ex_sources,
            std::vector<Register>&          mem_sources
        );
};

} // namespace processor

#endif
