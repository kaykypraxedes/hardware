/* headers/Instruction.h */
#ifndef INSTRUCTION_H // Include guard
#define INSTRUCTION_H
#include "Components.h"
#include <string>
#include <vector>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

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
        // Elementos static:
        static std::vector<int> base_ex_latencies;
        static std::vector<int> base_mem_latencies;

        // Construtor:
        Instruction(
            const int = -1,
            const std::string& = {}
        );

        // Getters:
        int                GetPosition()          const;
        int                GetExLatency()         const;
        int                GetMemLatency()        const;
        INSTRUCTION_TYPE   GetInstructionType()   const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const Register&    GetDestRegister()      const;
        const Register&    GetJ()                 const;
        const Register&    GetK()                 const;
        const std::string& GetInstructionString() const;
        // Métodos públicos:
        void SetMemLatency(
            const int
        );
        void SetExLatency(
            const int
        );
    private:
        // Atributos:
        int              position{-1}; // Funciona como um ID para diferenciar instruções iguais.
        std::string      instruction_string;
        int              ex_latency{};
        int              mem_latency{};
        INSTRUCTION_TYPE type{INSTRUCTION_TYPE::INVALID};
        Register         dest_register;
        Register         reg_j;
        Register         reg_k;
        // Métodos privados:
        void ParseInstruction(
            const std::string&
        );
        std::vector<std::string> SplitInstruction(
            const std::string&
        ) const;
        bool IdentifyType(
            const std::string&
        );
        void NormalizeInstruction(
            std::vector<std::string>&
        );
        void SetAttributes(
            std::vector<std::string>&
        );
        void SetLatencies();
};
} // namespace processor

#endif
