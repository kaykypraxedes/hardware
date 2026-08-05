/* headers/ReservationStations.h */
#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include "Instruction.h"
#include "Components.h"
#include <string>
#include <vector>
#include <memory>    // para std::shared_ptr
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class ReservationStation {
    public:
        // Construtor:
        ReservationStation(
            const std::string&
        );

        // Getters:
        bool                       IsBusy()              const;
        int                        GetCountdown()        const;
        int                        GetFUPosition()       const;
        INSTRUCTION_PHASE_TOMASULO GetInstructionPhase() const;
        // Não são "const &" pois podem não achar o Q (sendo enviada uma string vazia que é local).
        std::string                     GetQj()                 const;
        std::string                     GetQk()                 const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const Instruction&              GetCurrentInstruction() const;
        const std::string&              GetId()                 const;
        const std::vector<int>&         GetTimes()              const;
        const std::vector<std::string>& GetInstructions()       const;

        // Métodos públicos:
        bool AddIssue(
            const std::shared_ptr<Instruction>&,
            CDB&,
            const int
        );
        bool UpdateDependencies(
            CDB&,
            FUNCTIONAL_UNITS&,
            const int
        );
        bool UpdateCountdown(
            FUNCTIONAL_UNITS&,
            const int
        );
        void Release(
            const int
        );
        void ResolveDependency(
            const std::string& rs_id,
            const Register& value
        );
    private:
        // Atributos:
        bool                         busy{false};
        int                          allocation_countdown{-1};
        int                          fu_position{-1};
        std::string                  id;                     // Nome do RS ("load1", "addInt3", etc.).
        std::shared_ptr<Instruction> current_instruction;    // Instrução (compartilhada com a tabela/ROB).
        INSTRUCTION_PHASE_TOMASULO   phase{INSTRUCTION_PHASE_TOMASULO::UNUSED};
        std::vector<int>             allocation_times;       // De 2 em 2, início da alocação e fim da alocação.
        std::vector<std::string>     allocated_instructions; // De 1 em 1, instruções alocadas na RS.
        std::vector<Register>                   V;           // Vj e Vk.
        std::vector<std::pair<std::string,int>> Q;           // Qj e Qk (rs_id, start_cycle).

        // Métodos privados:
        // - AddIssue()
        void SetupNewIssue(
            const std::shared_ptr<Instruction>&,
            const int
        );
        void ReadSourceOperand(
            const size_t,
            const Register&,
            const CDB&
        );
        void AllocateDestInCDB(
            const std::vector<Register>&,
            CDB&,
            const int
        );
        // - UpdateDependencies()
        void CheckDependency(
            const size_t,
            CDB&
        );
        bool AdvancePhaseAllocation(
            FUNCTIONAL_UNITS&,
            const int
        );
        bool TryAllocateFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE_TOMASULO,
            const int,
            const int
        );
        int FindFreeFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE_TOMASULO,
            const int
        );
        // - UpdateCountdown()
        void ReleaseFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE_TOMASULO,
            const int
        );
    };

// ─── STRUCT ───────────────────────────────────────────────────────
struct RESERVATION_STATIONS {
    std::vector<ReservationStation> load;
    std::vector<ReservationStation> store;
    std::vector<ReservationStation> int_basic;
    std::vector<ReservationStation> int_mult_div;
    std::vector<ReservationStation> float_basic;
    std::vector<ReservationStation> float_mult_div;
};

} // namespace processor

#endif
