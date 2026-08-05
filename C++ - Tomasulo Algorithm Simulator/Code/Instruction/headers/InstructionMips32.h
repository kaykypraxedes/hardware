/* Instruction/headers/InstructionMips32.h */
#ifndef INSTRUCTION_MIPS32_H // Include guard
#define INSTRUCTION_MIPS32_H
#include "../../headers/Instruction.h"
#include <algorithm>         // para std::find
#include <cctype>            // para std::toupper

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
    };

} // namespace processor

#endif
