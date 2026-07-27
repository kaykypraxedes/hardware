/* headers/Instruction.h */
#ifndef INSTRUCTION_H
#define INSTRUCTION_H
#include <string>
#include <vector>
#include "Components.h"

// ─── ENUMS ────────────────────────────────────────────────────────
enum class INSTRUCTION_TYPE {
    NONEXISTENT,
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
enum class INSTRUCTION_PHASE {
    ISSUE,
    EX,
    MEM,
    WB,
    COMMIT
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Instruction {
    public:
        // Elementos static:
        static std::vector<int> base_ex_latencies;
        static std::vector<int> base_mem_latencies;

        // Construtores:
        Instruction() :
            PC(-1),
            ex_latency(0),
            mem_latency(0),
            type(INSTRUCTION_TYPE::NONEXISTENT) {}
        Instruction(
            int,
            std::string
        );

        // Getters:
        int                GetPC()                const;
        int                GetExLatency()         const;
        int                GetMemLatency()        const;
        INSTRUCTION_TYPE   GetInstructionType()   const;
        Register           GetDestRegister()      const;
        Register           GetJ()                 const;
        Register           GetK()                 const;
        // const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const std::string& GetInstructionString() const;
        // Métodos públicos:
        void SetMemLatency(
            int
        );
        void SetExLatency(
            int
        );
    private:
        // Atributos:
        // (instruction_string DEVE vir antes de PC para que o initializer list a inicialize antes de SplitInstruction() ser chamado)
        std::string      instruction_string;
        int              PC{};
        int              ex_latency;
        int              mem_latency{};
        INSTRUCTION_TYPE type;
        Register         dest_register;
        Register         reg_j;
        Register         reg_k;
        // Métodos privados:
        void                     ParseInstruction();
        void                     SetLatencies();
        std::vector<std::string> SplitInstruction();
        void IdentifyType(
            const std::string&
        );
        void SetAttributes(
            std::vector<std::string>&
        );
};
#endif
