/* Instruction/headers/InstructionArm64.h */
#ifndef INSTRUCTION_ARM64_H // Include guard
#define INSTRUCTION_ARM64_H
#include "../../headers/Instruction.h"
#include <algorithm>        // para std::find
#include <cctype>           // para std::toupper

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionArm64 : public Instruction { // Herança da classe Instruction.
    public:
        // Método estático:
        // Monta o CDB (vetor de registradores físicos) da arquitetura.
        static CDB MakeCDB();

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionArm64(
            const int = -1
        );

    protected:
        // Métodos "privados":
        // - override para implementar sua versão específica.
        std::vector<std::string> SplitInstruction(
            const std::string&
        ) const override;
        bool IdentifyType(
            const std::vector<std::string>&
        ) override;
        void NormalizeInstruction(
            std::vector<std::string>&
        ) override;
        void SetAttributes(
            const std::vector<std::string>&
        ) override;
        void ValidateInstruction(
            const std::vector<std::string>&,
            const std::vector<int>&,
            const std::vector<int>&
        ) override;
};

} // namespace processor

#endif
