/* headers/Instruction.h */
#ifndef INSTRUCTION_H // Include guard
#define INSTRUCTION_H
#include "Components.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── ELEMENTO STATIC ──────────────────────────────────────────────
// ATENÇÃO: A função deve ser declarada em cada uma das subclasses de Instruction (versão própria).
// - Gera warning (por não ser definido em Instruction.cpp), que é ignorado pela diretiva.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static const std::unordered_map<std::string, Register>& RegisterTable();
#pragma GCC diagnostic pop

// ─── HELPERS ──────────────────────────────────────────────────────
// Não são static para não forçar uma reimplementação em cada .cpp (são iguais).
void FillCDB(
    CDB&,
    const char,
    const int,
    const int,
    const int = 255
);
Register LookupRegister(
    const std::string&,
    const std::string&,
    const std::unordered_map<std::string, Register>&
);
// Validação de registrador compartilhada por todas as arquiteturas:
// - Determina a partir da verificação direta do token na RegisterTable() (sem falso positivo).
bool IsRegister(
    const std::string&,
    const std::unordered_map<std::string, Register>&
);

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
        // Elementos static:
        static std::vector<int> base_ex_latencies;
        static std::vector<int> base_mem_latencies;

        // Construtor:
        // - explicit para impedir o cast implícito.
        explicit Instruction(
            const int = -1
        );

        // Destrutor (importante em se tratando de um vetor de ponteiros compartilhados):
        // - virtual para permitir override das suas sub-classes.
        virtual ~Instruction() = default;

        /// Getters:
        int GetPosition()   const;
        int GetExLatency()  const;
        int GetMemLatency() const;
        INSTRUCTION_TYPE GetInstructionType() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const std::vector<Register>& GetDestRegisters()   const;
        const std::vector<Register>& GetSourceRegisters() const;
        const std::string& GetInstructionString()         const;

        // Métodos públicos:
        void Parse(
            const std::string&
        );
        void SetMemLatency(
            const int
        );
        void SetExLatency(
            const int
        );

    protected:
        // Atributos:
        int position{-1};
        std::string instruction_string;
        int ex_latency{};
        int mem_latency{};
        INSTRUCTION_TYPE type{INSTRUCTION_TYPE::INVALID};
        std::vector<Register> dest_registers;   // X86, ARM, etc. Podem ter múltiplos destinos (como aliases e flags).
        std::vector<Register> source_registers;

        // Métodos virtuais (cada arquitetura deve implementar sua versão):
        virtual std::vector<std::string> SplitInstruction(
            const std::string&
        ) const = 0;
        virtual bool IdentifyType(
            const std::vector<std::string>&
        ) = 0;
        virtual void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) = 0;
        virtual void SetAttributes(
            const std::vector<std::string>& tokens
        ) = 0;
        // Método igual para todos.
        void SetLatencies();
};

} // namespace processor

#endif
