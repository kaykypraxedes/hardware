/* Architectures/headers/RiscV.h */
#ifndef RISCV_H      // Include guard
#define INSTRUCTION_RISCV_H
#include "../../headers/Architecture.h"
#include <cctype>    // para std::tolower

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionRiscV : public Instruction { // Herança da classe Instruction.
    public:
        // Método estático:
        // Monta o CDB (vetor de registradores físicos) da arquitetura.
        static CDB MakeCDB();

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionRiscV(
            const int = -1
        );

    protected:
        // Métodos "privados":
        // - override para implementar sua versão específica.
        std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const override;
        bool IdentifyType(
            const std::vector<std::string>& tokens
        ) override;
        void ValidateInstruction(
            const std::vector<std::string>& tokens
        ) override;
        void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) override;
        void SetAttributes(
            const std::vector<std::string>& tokens
        ) override;
};

} // namespace processor

#endif
