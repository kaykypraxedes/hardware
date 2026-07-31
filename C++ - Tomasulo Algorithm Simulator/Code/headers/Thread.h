/* headers/Thread.h */
#ifndef THREAD_H // Include guard
#define THREAD_H
#include "Components.h"
#include "Instruction.h"
#include "ReservationStations.h"
#include "SortUtils.h"
#include <string>
#include <vector>
#include <tuple>      // para passar e receber as instruções de troca de thread na Granulação Grossa
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── ELEMENTO STATIC ──────────────────────────────────────────────
static const int ROB_CAPACITY_DEFAULT = 30; // Valor arbitrário

// ─── ENUMS ────────────────────────────────────────────────────────
enum class THREAD_STATE {
    ACTIVE,           // Executando.
    BRANCH_RESOLVING  // Após branch sem ROB, aguardando a finalização do ciclo.
};
enum class STORE_COMMIT_STATE {
    PENDING,
    WAITING_MEM,
    READY
};

// ─── STRUCTS ──────────────────────────────────────────────────────
struct TABLE_ROW {
    Instruction       instruction;
    int               issue_cycle{-1};
    std::vector<int>  ex_cycles;
    std::vector<int>  mem_cycles;
    int               wr_cycle{-1};
    int               commit_cycle{-1};
    STORE_COMMIT_STATE store_commit_state{STORE_COMMIT_STATE::PENDING};
};
struct EVENT {
    int               pc;
    INSTRUCTION_PHASE phase_before;
    INSTRUCTION_PHASE phase_after;
    INSTRUCTION_TYPE  type;
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Thread {
    public:
        // Construtor:
        Thread(
            const std::vector<std::string>&,
            const std::vector<std::tuple<int,int,int>>& = {},
            const std::vector<int>&                     = {},
            const std::vector<int>&                     = {},
            const std::vector<int>&                     = {},
            const int                                   = 1,
            const int                                   = 0,
            const bool                                  = false
        );

        // Getters:
        int          GetCurrentInstructionPosition() const;
        int          GetNumStalls()                  const;
        THREAD_STATE GetThreadState()                const;
        // "const &" para evitar cópia (para tipos básicos e enums o ganho é marginal)
        const CDB&                    GetCDB()   const;
        const RESERVATION_STATIONS&   GetRS()    const;
        const FUNCTIONAL_UNITS&       GetFU()    const;
        const std::vector<TABLE_ROW>& GetTable() const;

        // Métodos públicos:
        bool IsSwitchCycle();
        // - Estágios da pipeline.
        bool Issue(
            const int
        );
        bool ExMem(
            const int
        );
        void Wr(
            const int
        );
        void Commit(
            const int
        );
    private:
        // Atributos:
        // - Elementos auxiliares dentro da Thread.
        int                      num_finished_instructions{};
        int                      num_committed_instructions{};
        int                      num_stalls{};
        int                      commit_pointer{};
        int                      unresolved_branch_pc{-1};
        THREAD_STATE             state{THREAD_STATE::ACTIVE};
        // - Elementos funcionais da Thread.
        bool                     has_rob{false};
        int                      rob_capacity{1};
        int                      current_instruction_position{};
        bool                     has_predictor{false};
        CDB                      cdb;
        RESERVATION_STATIONS     rs;
        FUNCTIONAL_UNITS         fu;
        std::vector<int>         wb_buffer;
        std::vector<int>         pending_wb_buffer;
        std::vector<int>         switch_cycles;
        std::vector<Instruction> rob;
        std::vector<TABLE_ROW>   instruction_table;

        // Métodos privados:
        void InitializeComponents(
            const std::vector<int>&,
            const std::vector<int>&,
            int
        );
        // - Organizados por estágio.

        // ISSUE
        void RegisterIssue(
            int
        );

        // EX/MEM
        void StartExOrMemPhase(
            int
        );
        void CollectCandidatesToAdvance(
            std::vector<ReservationStation*>&
        );
        void CollectCandidatesFromGroup(
            std::vector<ReservationStation>&,
            std::vector<ReservationStation*>&
        );
        void TryAdvanceRS(
            ReservationStation&,
            int
        );

        // WR (Write Result)
        void PerformWriteResult(
            int
        );
        void SortWBBuffer();
        int  NextWB() const;
        void RemoveWB();
        void AddWB(
            int
        );
        void AddPendingWB(
            int
        );
        void FlushPendingWBBuffer();
        void WriteBackStoreWithROB(
            int,
            int
        );
        void WriteBackNormal(
            int,
            int
        );
        void BroadcastCDB(
            int,
            const Register&,
            int
        );
        void DetectPhaseTransitions(
            int
        );
        void CollectTransitionEvents(
            std::vector<EVENT>&,
            int
        );
        void ProcessTransition(
            const EVENT&,
            int
        );
        void AddExCycle(
            int,
            int
        );
        void AddMemCycle(
            int,
            int
        );
        void SetWR(
            int,
            int
        );
        void FindWBInGroup(
            std::vector<ReservationStation>&,
            int,
            const Register&,
            int
        );
        void CollectEventsFromGroup(
            std::vector<ReservationStation>&,
            std::vector<EVENT>&,
            int
        );

};
} // namespace processor

#endif
