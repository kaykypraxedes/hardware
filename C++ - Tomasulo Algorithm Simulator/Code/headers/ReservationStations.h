/* headers/ReservationStations.h */
#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include <string>
#include <vector>
#include <utility>
#include "Instruction.h"
#include "Components.h"

// classe:
class ReservationStation {
    public:
        // Construtor
        ReservationStation(std::string);

        bool                     GetBusy()                const;
        int                      GetCountdown()           const;
        int                      GetFUPosition()          const;
        INSTRUCTION_PHASE        GetInstructionPhase()    const;
        const Instruction&       GetCurrentInstruction()  const; // const & para evitar cópia
        const std::string&       GetId()                  const; // const & para evitar cópia
        const std::string&       GetQj()                  const; // const & para evitar cópia
        const std::string&       GetQk()                  const; // const & para evitar cópia
        const std::vector<int>&  GetTimes()               const; // const & para evitar cópia
        const std::vector<std::string>& GetInstructions() const; // const & para evitar cópia

        // Métodos públicos
        bool AddIssue(
            Instruction&,
            CDB&,
            int
        );

        bool UpdateDependencies(
            CDB&,
            FUNCTIONAL_UNITS&,
            int
        );

        bool UpdateCountdown(
            FUNCTIONAL_UNITS&,
            int
        );

        void Release(
            int
        );

        void ResolveDependency(
            const std::string& rs_id,
            const Register& value
        );
    private:
        // Atributos
        bool                        busy{false};
        bool                        dest_pending_in_cdb{false};
        int                         allocation_countdown{-1};
        int                         fu_position{-1};
        std::string                 id;         // Nome do RS ("load1", "addInt3", etc.)
        Instruction                 current_instruction;
        INSTRUCTION_PHASE           phase;
        Register                    Vj;                            // Valor do operand J (se Qj vazio)
        Register                    Vk;                            // Valor do operand K (se Qk vazio)
        std::pair<std::string, int> Qj{"", -1};                    // Qj/Qk: {rs_id, ciclo_inicio} — produtor pendente
        std::pair<std::string, int> Qk{"", -1};                    // par vazio {"", -1} = operando disponível
        std::vector<int>            allocation_times;     // De 2 em 2, início da alocação e fim da alocação
        std::vector<std::string>    allocated_instructions; // De 1 em 1, instruções alocadas na RS
        // Métodos privados
        int FindFreeFU(
            FUNCTIONAL_UNITS&,
            int,
            INSTRUCTION_PHASE
        ) const;

        int AllocateFreeFU(
            std::vector<FU>&,
            int
        ) const;

        void ReleaseFU(
            FUNCTIONAL_UNITS&,
            int,
            INSTRUCTION_PHASE
        );

        void DeallocateFUFromGroup(
            std::vector<FU>&,
            int
        );
    };

// struct:
struct RESERVATION_STATIONS {
    std::vector<ReservationStation> load;
    std::vector<ReservationStation> store;
    std::vector<ReservationStation> int_basic;
    std::vector<ReservationStation> int_mult_div;
    std::vector<ReservationStation> float_basic;
    std::vector<ReservationStation> float_mult_div;
};

#endif
