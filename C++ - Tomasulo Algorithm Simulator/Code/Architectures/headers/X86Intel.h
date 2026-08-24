/* Architectures/headers/X86Intel.h */
#ifndef X86INTEL_H   // Include guard
#define X86INTEL_H
#include "../../headers/Architecture.h"
#include <algorithm> // para std::find
#include <cctype>    // para std::toupper

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionX86Intel : public Instruction { // Herança da classe Instruction.
    public:
        // Método estático:
        // Monta o CDB (vetor de registradores físicos) da arquitetura.
        static CDB MakeCDB();

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit InstructionX86Intel(
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
