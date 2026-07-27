/* headers/Processor.h */
#ifndef PROCESSOR_H
#define PROCESSOR_H
#include <string>
#include <vector>
#include "Instruction.h"
#include "Thread.h"

//enums:
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

// classe:
class Processor {
    public:
        // Construtor
        Processor(
            int,
            int,
            bool,
            PROCESSOR_TYPE,
            MULTITHREADING_MODEL,
            const std::vector<std::string>&,
            const std::vector<int>& = {},
            std::vector<int> = {},
            const std::vector<int>& = {}
        );

        // Métodos públicos
        bool                     ExecuteCycle();
        std::vector<CYCLE_LINE>  GetCycleTable() const;
        PROCESSOR_TYPE           GetType()         const;

        const Thread& GetThread(
            int
        ) const; // const & para evitar cópia

        const std::vector<TABLE_ROW>& GetThreadTable(
            int
        ) const; // const & para evitar cópia
    private:
        // Atributos
        int                  dispatch_width{1};
        int                  current_cycle{1};
        int                  thread_pointer{};
        int                  num_ufs_por_tipo{1};
        bool                 has_predictor;
        PROCESSOR_TYPE       type;
        MULTITHREADING_MODEL mt_model;
        std::vector<Thread>  threads;
        // Métodos privados
        bool ExecuteExMemWr();
        void ExecuteIssue();
        void AdvanceRoundRobinPointer();

        void InitializeThreads(
            const std::vector<std::string>&,
            bool,
            int,
            const std::vector<int>&,
            const std::vector<int>&,
            const std::vector<int>& = {}
        );
};

#endif
