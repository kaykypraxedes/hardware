/* Architectures/headers/Mips32.h */
#ifndef MIPS32_H     // Include guard
#define MIPS32_H
#include "../../headers/Architecture.h"
#include <algorithm> // para std::find
#include <cctype>    // para std::toupper

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionMips32 : public Instruction { // Herança da classe Instruction.
    public:
        // Método estático:
        // Monta o CDB (vetor de registradores físicos) da arquitetura.
        static CDB MakeCDB();

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionMips32(
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
