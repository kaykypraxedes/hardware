/* headers/Thread.h */
#ifndef THREAD_H      // Include guard
#define THREAD_H
#include "Instruction.h"
#include "InstructionFactory.h"
#include "ReservationStations.h"
#include "Components.h"
#include <string>
#include <vector>
#include <memory>     // para std::unique_ptr e std::shared_ptr
#include <tuple>      // para passar e receber as instruções de troca de thread na Granulação Grossa
#include <algorithm>  // para std::stable_sort
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── ELEMENTO STATIC ──────────────────────────────────────────────
static const int rob_capacity_default{32};

// ─── STRUCT ───────────────────────────────────────────────────────
struct TABLE_ROW {
    // - Thread é a "dona" do objeto via shared_ptr (RS e ROB guardam cópias do ponteiro).
    std::shared_ptr<Instruction> instruction;
    int                          issue_cycle{-1};
    std::vector<int>             ex_cycles;
    std::vector<int>             mem_cycles;
    int                          wr_cycle{-1};
    int                          commit_cycle{-1};
};

// ─── CLASSE ───────────────────────────────────────────────────────
class Thread {
    public:
        // Construtor:
        Thread(
            const std::vector<std::string>&,
            const std::vector<std::tuple<int,int,int>>& = {},
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            const int               = 1,
            const int               = 0,
            const bool              = false,
            const ARCHITECTURE      = ARCHITECTURE::MIPS_32
        );

        // Getters:
        int GetCurrentInstructionPosition() const;
        // "const &" para evitar cópia (para tipos básicos e enums o ganho é marginal).
        const CDB&                    GetCDB()   const;
        const RESERVATION_STATIONS&   GetRS()    const;
        const FUNCTIONAL_UNITS&       GetFU()    const;
        const std::vector<TABLE_ROW>& GetTable() const;

        // Métodos públicos:
        bool IsSwitchCycle();
        // - Estágios da pipeline:
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
        // - Elementos auxiliares:
        int                      num_finished_instructions{};
        int                      num_committed_instructions{};
        int                      commit_pointer{};
        int                      unresolved_branch_position{-1};
        // - Elementos funcionais:
        bool                     has_rob{false};
        int                      rob_capacity{1};
        int                      current_instruction_position{};
        bool                     has_predictor{false};
        CDB                      cdb;
        RESERVATION_STATIONS     rs;
        FUNCTIONAL_UNITS         fu;
        std::vector<int>         wr_buffer;
        std::vector<int>         pending_wr_buffer;
        std::vector<int>         switch_cycles;        // Para processadores com granulação grossa.
        std::vector<TABLE_ROW>   instruction_table;
        std::vector<std::shared_ptr<Instruction>> rob; // guarda ponteiros compartilhados.

        // Métodos privados:
        void InitializeComponents(
            const std::vector<int>&,
            const std::vector<int>&,
            const int,
            const ARCHITECTURE
        );
        std::vector<std::vector<ReservationStation>*> GetAllRSGroups();
        std::vector<std::vector<FU>*> GetAllFUGroups();

        // - EX/MEM
        void StartExOrMemPhase(
            const int
        );

        // - WR (Write Result)
        void PerformWriteResult(
            const int
        );
        void WriteResultOnComponents(
            const int,
            const int
        );
        void BroadcastOnCDBAndRS(
            const Register&,
            const int,
            const int
        );
        void DetectPhaseTransitions(
            const int
        );
};
} // namespace processor

#endif
