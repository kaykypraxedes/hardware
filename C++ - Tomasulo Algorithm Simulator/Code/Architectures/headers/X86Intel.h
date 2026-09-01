/* Architectures/headers/X86Intel.h */
#ifndef X86INTEL_H   // Include guard
#define X86INTEL_H
#include "../../headers/Architecture.h"
#include <cctype>    // std::isalnum(), std::isalpha(), std::isdigit() e std::tolower().
#include <limits>    // std::numeric_limits.

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class InstructionX86Intel : public Instruction { // Herança da classe Instruction.
    private:

        /**
         * @brief Classifica a representação exclusiva de um operando x86.
         */
        enum class OPERAND_TYPE {
            REGISTER,
            IMMEDIATE,
            LABEL,
            MEMORY
        };

        /**
         * @brief Representa o qualificador opcional de largura de memória.
         */
        enum class MEMORY_SIZE {
            NONE,
            BYTE,
            WORD,
            DWORD,
            QWORD,
            XMMWORD
        };

        // ─── STRUCTS ──────────────────────────────────────────────────────

        /**
         * @brief Armazena um literal inteiro sem perder sinal, base ou forma canônica.
         */
        struct INTEGER_LITERAL {
            bool        negative{};
            unsigned long magnitude{};
            int         radix{10};
            std::string normalized;
        };

        /**
         * @brief Armazena os componentes validados de um endereço x86-64.
         */
        struct MEMORY_OPERAND {
            MEMORY_SIZE     size{MEMORY_SIZE::NONE};
            std::string     base;
            std::string     index;
            int             scale{1};
            bool            has_displacement{};
            INTEGER_LITERAL displacement;
            bool            rip_relative{};
            std::string     label;
        };

        /**
         * @brief Associa exatamente um tipo de operando à sua representação validada.
         */
        struct OPERAND {
            OPERAND_TYPE    type{OPERAND_TYPE::LABEL};
            std::string     register_name;
            INTEGER_LITERAL immediate;
            std::string     label;
            MEMORY_OPERAND  memory;
        };

        /**
         * @brief Cache privado reconstruído para os operandos da instrução atual.
         */
        std::vector<OPERAND> operands;

        // ─── HELPERS ──────────────────────────────────────────────────────

        /**
         * @brief Classifica e valida um operando escalar ou de memória.
         *
         * @param token Texto integral do operando.
         *
         * @return Operando privado tipado e validado.
         */
        OPERAND ParseOperand(
            const std::string& token
        ) const;

        /**
         * @brief Converte um literal decimal ou hexadecimal sem overflow intermediário.
         *
         * @param token Texto integral do literal.
         * @param memory_displacement Restringe valores positivos ao intervalo de long.
         *
         * @return Sinal, magnitude, base e spelling canônico do literal.
         */
        static INTEGER_LITERAL ParseInteger(
            const std::string& token,
            bool               memory_displacement
        );

        /**
         * @brief Verifica a gramática de uma label e exclui nomes reservados.
         *
         * @param token Texto candidato a label.
         *
         * @return true quando o texto representa uma label válida.
         */
        static bool IsValidLabel(
            const std::string& token
        );

        /**
         * @brief Verifica se uma peça de memória deve ser validada como número.
         *
         * @param piece Peça atômica da expressão de endereço.
         *
         * @return true quando a peça começa por um dígito decimal.
         */
        static bool IsNumericPiece(
            const std::string& piece
        );

        /**
         * @brief Verifica se uma peça representa um GPR64 válido como base.
         *
         * @param piece Peça atômica da expressão de endereço.
         *
         * @return true quando a peça representa um GPR64.
         */
        static bool IsGpr64(
            const std::string& piece
        );

        /**
         * @brief Verifica se um GPR64 pode ocupar a posição de index.
         *
         * @param piece Peça atômica da expressão de endereço.
         *
         * @return true para GPR64 diferente de rsp e r12.
         */
        static bool IsValidMemoryIndex(
            const std::string& piece
        );

        /**
         * @brief Valida e armazena o displacement com seu sinal.
         *
         * @param memory Operando de memória em construção.
         * @param sign Operador de soma ou subtração anterior ao valor.
         * @param value Magnitude decimal ou hexadecimal do displacement.
         */
        static void SetMemoryDisplacement(
            MEMORY_OPERAND&    memory,
            const std::string& sign,
            const std::string& value
        );

        /**
         * @brief Valida e armazena uma scale do conjunto 1, 2, 4 ou 8.
         *
         * @param memory Operando de memória em construção.
         * @param value Texto decimal da scale.
         */
        static void SetMemoryScale(
            MEMORY_OPERAND&    memory,
            const std::string& value
        );

        /**
         * @brief Decompõe e valida um endereço conforme a gramática x86-64 suportada.
         *
         * @param token Operando completo, incluindo qualificador opcional e colchetes.
         *
         * @return Componentes tipados do endereço.
         */
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
        // Monta o layout de registradores físicos da arquitetura.
        static REGISTER_LAYOUT MakeRegisterLayout();

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
        bool SetStages(
            const std::vector<std::string>& tokens
        ) override;
        void ValidateInstruction(
            const std::vector<std::string>& tokens,
            INSTRUCTION_TYPE                instruction_type
        );
        void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) override;
        void SetStageAttributes(
            const std::vector<std::string>& tokens,
            INSTRUCTION_TYPE                instruction_type,
            std::vector<Register>&          ex_sources,
            std::vector<Register>&          mem_sources
        );
};

} // namespace processor

#endif
