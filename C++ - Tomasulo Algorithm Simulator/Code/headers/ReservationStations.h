/* headers/ReservationStations.h */
#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include "Components.h"
#include "Instruction.h"
#include <string>
#include <vector>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class ReservationStation {
    public:
        // Construtor:
        ReservationStation(
            const std::string&
        );

        // Getters:
        bool              GetBusy()             const;
        int               GetCountdown()        const;
        int               GetFUPosition()       const;
        INSTRUCTION_PHASE GetInstructionPhase() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const Instruction&              GetCurrentInstruction() const;
        const std::string&              GetId()                 const;
        const std::string&              GetQj()                 const;
        const std::string&              GetQk()                 const;
        const std::vector<int>&         GetTimes()              const;
        const std::vector<std::string>& GetInstructions()       const;

        // Métodos públicos:
        bool AddIssue(
            const Instruction&,
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
            int
        );
        void ResolveDependency(
            const std::string& rs_id,
            const Register& value
        );
    private:
        // Atributos:
        bool                        busy{false};
        int                         allocation_countdown{-1};
        int                         fu_position{-1};
        std::string                 id;                     // Nome do RS ("load1", "addInt3", etc.)
        Instruction                 current_instruction;
        INSTRUCTION_PHASE           phase{INSTRUCTION_PHASE::ISSUE};
        Register                    Vj;                     // Valor do operando J (se Qj vazio)
        Register                    Vk;                     // Valor do operando K (se Qk vazio)
        std::pair<std::string, int> Qj{"", -1};             // Qj/Qk: {rs_id, start_cycle} — produtor pendente
        std::pair<std::string, int> Qk{"", -1};             // par vazio {"", -1} = operando disponível
        std::vector<int>            allocation_times;       // De 2 em 2, início da alocação e fim da alocação
        std::vector<std::string>    allocated_instructions; // De 1 em 1, instruções alocadas na RS

        // Métodos privados:
        // - AddIssue()
        void SetupNewIssue(
            const Instruction&,
            const int
        );
        void ReadSourceOperand(
            const char,
            const Register&,
            const CDB&
        );
        void AllocateDestInCDB(
            const Register&,
            CDB&,
            const int
        );
        // - UpdateDependencies()
        void CheckDependency(
            const char,
            CDB&
        );
        bool AdvancePhaseAllocation(
            FUNCTIONAL_UNITS&,
            const int
        );
        bool TryAllocateFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE,
            const int,
            const int
        );
        int FindFreeFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE,
            const int
        ) const;
        // - UpdateCountdown()
        void ReleaseFU(
            FUNCTIONAL_UNITS&,
            const INSTRUCTION_PHASE,
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
