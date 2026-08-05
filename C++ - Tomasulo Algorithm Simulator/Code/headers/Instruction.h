/* headers/Instruction.h */
#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include "Components.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <iostream>

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────
enum class INSTRUCTION_TYPE {
    INVALID,
    LOAD,
    STORE,
    BRANCH,
    INT_BASIC,
    INT_MUL,
    INT_DIV,
    FLOAT_BASIC,
    FLOAT_MUL,
    FLOAT_DIV
};

enum class INSTRUCTION_PHASE_TOMASULO {
    UNUSED,
    IS,
    EX,
    MEM,
    WR,
    COMMIT
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Instruction {
    public:
        static std::vector<int> base_ex_latencies;
        static std::vector<int> base_mem_latencies;

        // Construtor:
        explicit Instruction(
            const int = -1
        );

        // Destrutor: (importante em se tratando de um vetor de ponteiros compartilhados)
        virtual ~Instruction() = default;

        /// Getters:
        int GetPosition()   const;
        int GetExLatency()  const;
        int GetMemLatency() const;
        INSTRUCTION_TYPE GetInstructionType() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const std::vector<Register>& GetDestRegisters()   const;
        const std::vector<Register>& GetSourceRegisters() const;
        const std::string& GetInstructionString()         const;

        // Métodos públicos:
        void SetMemLatency(
            const int
        );
        void SetExLatency(
            const int
        );
        void Parse(
            const std::string&
        );

    protected:
        // Atributos:
        int position{-1};
        std::string instruction_string;
        int ex_latency{};
        int mem_latency{};
        INSTRUCTION_TYPE type{INSTRUCTION_TYPE::INVALID};
        std::vector<Register> dest_registers;  // [0] destino direto; [1] flag (se tiver).
        std::vector<Register> source_registers;

        // Métodos virtuais puros (cada arquitetura deve implementar sua versão)
        virtual std::vector<std::string> SplitInstruction(
            const std::string&
        ) const = 0;
        virtual bool IdentifyType(
            const std::string&
        ) = 0;
        virtual void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) = 0;
        virtual void SetAttributes(
            const std::vector<std::string>& tokens
        ) = 0;
        void SetLatencies();
};
} // namespace processor

#endif
