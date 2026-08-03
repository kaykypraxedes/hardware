/* headers/Processor.h */
#ifndef PROCESSOR_H   // Include guard
#define PROCESSOR_H
#include "Instruction.h"
#include "Thread.h"
#include <string>
#include <vector>
#include <tuple>      // para passar e receber as instruções de troca de thread na Granulação Grossa
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────
enum class PROCESSOR_TYPE {
    IN_ORDER,
    TOMASULO_CLASSIC,     // Sem ROB
    TOMASULO_ESPECULATIVE // Com ROB
};
enum class MULTITHREADING_MODEL {
    NONE,
    SMT,
    FINE_GRAINED,
    COARSE_GRAINED
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Processor {
    public:
        // Construtor:
        Processor(
            const int,
            const int,
            const bool,
            const PROCESSOR_TYPE,
            const MULTITHREADING_MODEL,
            const std::vector<std::string>&,
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            const std::vector<std::tuple<int,int,int>>& = {}
        );

        // Getters:
        PROCESSOR_TYPE GetType() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const Thread& GetThread(
            int
        ) const;
        const std::vector<TABLE_ROW>& GetThreadTable(
            int
        ) const;

        // Métodos públicos:
        bool ExecuteCycle();
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
        void InitializeThreads(
            const std::vector<std::string>&,
            const int,
            const std::vector<int>&,
            const std::vector<int>&,
            const std::vector<int>& = {},
            const std::vector<std::tuple<int,int,int>>& = {}
        );
        bool ExecuteExMemWrCommit();
        void ExecuteIssue();
        void AdvanceRoundRobinPointer();
};
} // namespace processor

#endif
