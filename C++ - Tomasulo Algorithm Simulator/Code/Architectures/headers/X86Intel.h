/* Architectures/headers/X86Intel.h */
#ifndef X86INTEL_H   // Include guard
#define X86INTEL_H
#include "../../headers/Architecture.h"
#include <cctype>    // para std::tolower e std::isalnum
#include <limits>    // para std::numeric_limits

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionX86Intel : public Instruction { // Herança da classe Instruction.
    private:
        enum class OPERAND_TYPE {
            REGISTER,
            IMMEDIATE,
            LABEL,
            MEMORY
        };

        enum class MEMORY_SIZE {
            NONE,
            BYTE,
            WORD,
            DWORD,
            QWORD,
            XMMWORD
        };

        struct INTEGER_LITERAL {
            bool        negative{};
            unsigned long magnitude{};
            int         radix{10};
            std::string normalized;
        };

        struct MEMORY_OPERAND {
            MEMORY_SIZE size{MEMORY_SIZE::NONE};
            std::string base;
            std::string index;
            int         scale{1};
            bool        has_displacement{};
            INTEGER_LITERAL displacement;
            bool        rip_relative{};
            std::string label;
        };

        struct OPERAND {
            OPERAND_TYPE   type{OPERAND_TYPE::LABEL};
            std::string    register_name;
            INTEGER_LITERAL immediate;
            std::string    label;
            MEMORY_OPERAND memory;
        };

        std::vector<OPERAND> operands;

        OPERAND ParseOperand(
            const std::string& token
        ) const;
        static INTEGER_LITERAL ParseInteger(
            const std::string& token,
            bool               memory_displacement
        );
        static bool IsNumericPiece(
            const std::string& piece
        );
        static bool IsGpr64(
            const std::string& piece
        );
        static bool IsValidMemoryIndex(
            const std::string& piece
        );
        static void SetMemoryDisplacement(
            MEMORY_OPERAND&    memory,
            const std::string& sign,
            const std::string& value
        );
        static void SetMemoryScale(
            MEMORY_OPERAND&    memory,
            const std::string& value
        );
        MEMORY_OPERAND ParseMemoryOperand(
            const std::string& token
        ) const;
        std::string NormalizeOperand(
            const OPERAND& operand
        ) const;
        void PushOperandSources(
            std::vector<Register>& sources,
            const OPERAND&         operand
        ) const;

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
