/* headers/Components.h */
#ifndef COMPONENTS_H
#define COMPONENTS_H
#include <string>
#include <vector>
#include <cctype>

static const int num_registers = 32;

// Classe
class Register {
    public:
        // Construtores
        Register();
        Register(
            std::string
        );

        // Getters e setters
        char                      GetType()                const;
        int                       GetId()                  const;
        bool                      GetBusy()                const;
        void                      ToggleBusy();
        std::vector<int>          GetAllocationTimes()     const;
        const std::vector<std::string>& GetAllocatedRS()   const; // const & para evitar cópia
        std::string               GetCurrentRS()           const; // retorna o produtor mais recente (último de RS_alocadas com tempo_fim == -1)
        bool                      HasPendingProducer()     const;

        bool IsDependencyResolved(
            const std::string& rs_id,
            int start_cycle
        ) const;

        int GetRSCycleStart(
            const std::string& rs_id
        ) const; // ciclo_inicio do produtor pendente mais recente com esse nome

        // Métodos públicos
        void AllocateRS(
            const std::string&,
            int
        );

        void DeallocateRS(
            const std::string&,
            int,
            int
        );
    private:
        // Atributos
        char                     type{'Z'};
        int                      id;
        bool                     busy{false};
        std::vector<int>         start_times;
        std::vector<int>         end_times;     // Pares (inicio[n] - fim[n]); fim == -1 enquanto pendente
        std::vector<std::string> allocated_rs;
        // Métodos privados
        char IdentifyType(
            const std::string&
        );
        int IdentifyId(
            const std::string&
        );
};

// structs:
struct CDB {
    std::vector<Register> R;
    std::vector<Register> F;
};
struct FU {
    bool                     busy{false};
    std::string              current_rs{};
    std::vector<int>         allocation_times{};
    std::vector<std::string> allocated_rs{};
};
struct FUNCTIONAL_UNITS {
    std::vector<FU> memory_access;
    std::vector<FU> int_basic_alu;
    std::vector<FU> int_mult_div_alu;
    std::vector<FU> float_basic_alu;
    std::vector<FU> float_mult_div_alu;
    int             wr{1};
    int             commit{1};
};
#endif
