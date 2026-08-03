/* headers/Components.h */
#ifndef COMPONENTS_H  // Include guard
#define COMPONENTS_H
#include <string>
#include <vector>
#include <cctype>     // para std::isdigit
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr


namespace processor {

// ─── ELEMENTO STATIC ──────────────────────────────────────────────
static const int num_registers = 32;

// ─── CLASSE ───────────────────────────────────────────────────────
class Register {
    public:
        // Construtor:
        Register(
            const std::string& = ""
        );

        // Getters:
        char GetType() const;
        int  GetId()   const;
        bool GetBusy() const;
        // Não são "const &" pois podem enviar dados de variáveis locais (apagados ao final da função)
        std::vector<int> GetAllocationTimes() const;
        std::string      GetCurrentRS()       const; // retorna o produtor mais recente (último de allocated_rs com end_times == -1)
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
        const std::vector<std::string>& GetAllocatedRS() const;
        // Métodos públicos:
        void ToggleBusy();
        bool IsDependencyResolved(
            const std::string&,
            const int
        ) const;
        int GetRSCycleStart(
            const std::string&
        ) const; // start_cycle do produtor pendente mais recente com esse nome
        void AllocateRS(
            const std::string&,
            const int
        );
        bool DeallocateRS(
            const std::string&,
            const int,
            const int
        );
    private:
        // Atributos:
        char                     type{'Z'};
        int                      id{-1};
        bool                     busy{false};
        std::vector<int>         start_times;
        std::vector<int>         end_times;    // Pares (inicio[n] - fim[n]); fim == -1 enquanto pendente
        std::vector<std::string> allocated_rs;
        // Métodos privados:
        bool ParseType(
            const std::string&
        );
        bool ParseId(
            const std::string&
        );
        bool HasPendingProducer() const;
};

// ─── STRUCTS ──────────────────────────────────────────────────────
struct CDB {
    std::vector<Register> R;
    std::vector<Register> F;
};
struct FU {
    bool                     busy{false};
    std::string              current_rs;
    std::vector<int>         allocation_times;
    std::vector<std::string> allocated_rs;
};
struct FUNCTIONAL_UNITS {
    std::vector<FU> memory_access;
    std::vector<FU> int_basic_alu;
    std::vector<FU> int_mult_div_alu;
    std::vector<FU> float_basic_alu;
    std::vector<FU> float_mult_div_alu;
    int             wr{1};
    int             commit{0};
};

} // namespace processor

#endif
