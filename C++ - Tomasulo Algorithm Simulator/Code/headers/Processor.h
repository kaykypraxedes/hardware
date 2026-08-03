/* headers/Processor.h */
#ifndef PROCESSOR_H // Include guard
#define PROCESSOR_H
#include "Instruction.h"
#include "Thread.h"
#include <string>
#include <vector>

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────
enum class PROCESSOR_TYPE {
    IN_ORDER,
    TOMASULO_WITHOUT_ROB,
    TOMASULO_WITH_ROB
};
enum class MULTITHREADING_MODEL {
    SMT,
    FINE_GRAINED,
    COARSE_GRAINED
};

struct CYCLE_LINE {
    std::string thread;
    Instruction instruction;
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Processor {
    public:
        // Construtor:
        Processor(
            int,
            int,
            bool,
            PROCESSOR_TYPE,
            MULTITHREADING_MODEL,
            const std::vector<std::string>&,
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            const std::vector<int>& = {}
        );

        // Métodos públicos:
        std::vector<CYCLE_LINE> GetCycleTable() const;
        PROCESSOR_TYPE          GetType()       const;
        bool                    ExecuteCycle();
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const Thread& GetThread(
            int
        ) const;
        const std::vector<TABLE_ROW>& GetThreadTable(
            int
        ) const;
    private:
        // Atributos:
        int                  dispatch_width{1};
        int                  current_cycle{1};
        int                  thread_pointer{};
        int                  fus_per_type{1};
        bool                 has_predictor;
        PROCESSOR_TYPE       type;
        MULTITHREADING_MODEL mt_model;
        std::vector<Thread>  threads;
        // Métodos privados:
        bool ExecuteExMemWr();
        void ExecuteIssue();
        void AdvanceRoundRobinPointer();
        void InitializeThreads(
            const std::vector<std::string>&,
            int,
            const std::vector<int>&,
            const std::vector<int>&,
            const std::vector<int>& = {}
        );
};
} // namespace processor

#endif
