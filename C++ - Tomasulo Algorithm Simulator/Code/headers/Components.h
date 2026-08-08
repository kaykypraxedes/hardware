/* headers/Components.h */
#ifndef COMPONENTS_H  // Include guard
#define COMPONENTS_H
#include <string>
#include <vector>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr


namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────
class Register {
    public:
        // Construtores:
        Register() = default;
        Register(
            const char,
            const int,
            const int = 255 // Máscara para identificar sombreamento de registradores (x86 e ARM).
        );

        // Getters:
        char GetType() const;
        int  GetId()   const;
        int  GetMask() const;
        bool GetBusy() const;
        // Não são "const &" pois enviam dados de variáveis locais (apagados ao final da função).
        std::vector<int> GetAllocationTimes() const;
        std::string      GetCurrentRS()       const; // Produtor mais recente (último de allocated_rs com end_times == -1).
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const std::vector<std::string>& GetAllocatedRS() const;

        // Métodos públicos:
        void ToggleBusy();
        bool IsDependencyResolved(
            const std::string&,
            const int
        ) const;
        int GetRSCycleStart(
            const std::string&
        ) const; // start_cycle do produtor pendente mais recente com esse nome.
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
        int                      mask{255};    // Em binário = 11111111.
        bool                     busy{false};
        std::vector<int>         start_times;
        std::vector<int>         end_times;    // Pares (inicio[n] - fim[n]); fim == -1 enquanto pendente.
        std::vector<std::string> allocated_rs;

        // Métodos privados:
        bool HasPendingProducer() const;
};

// ─── STRUCTS ──────────────────────────────────────────────────────
struct CDB_BANK {
    char reg_class; // Classe de registrador (B, W, R, L, F, S, V, G).
    int  base;      // Primeiro id físico da faixa.
    int  count;     // Quantidade de registradores da faixa.
};
// Um slot um slot por par (id, máscara):
// - id =      localização física do registrador;
// - máscara = sub-registrador naquele endereço físico (que pode ou não barrar outros sub-registradores naquele id);
struct CDB {
    // Em x86 e ARM64, por exemplo, aliases sobrepostos têm slots próprios por variante:
    // - al (id 0, mask 00000001) - ah (id 0, mask 00000010). A máscara diz qual trecho de bits o registrador cobre.
    // - Demais arquiteturas: máscara uniforme 11111111 (um slot por id).
    std::vector<Register> registers;
    // Faixas de impressão definidas por arquitetura (índice exibido = id - base).
    std::vector<CDB_BANK> print_banks;
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

// ─── HELPERS ──────────────────────────────────────────────────────
// Não são static para não forçar uma reimplementação em cada .cpp (são iguais).
Register& GetReg(
    CDB&,
    const Register&
);
const Register& GetReg(
    const CDB&,
    const Register&
);

} // namespace processor

#endif
