/* headers/Componentes.h */
#ifndef COMPONENTES_H // Include guard
#define COMPONENTES_H
#include <string>
#include <vector>
#include <cctype>

constexpr int NUM_REGISTRADORES = 32;

// Classe
class Registrador {
    public:
        // Construtores
        Registrador();
        Registrador(
            std::string
        );
        // Getters e setters
        char                      getTipo()             const;
        int                       getId()               const;
        bool                      getBusy()             const;
        void                      trocaBusy();
        std::vector<int>                getTempoAlocacao()    const;
        const std::vector<std::string>& getRSalocadas()       const; // const & para evitar cópia
        std::string                     getRSatual()          const; // retorna o produtor mais recente (último de RS_alocadas com tempo_fim == -1)
        bool                      temProdutorPendente() const;

        bool dependenciaResolvida(
            const std::string& rs_id,
            int ciclo_inicio
        ) const;

        int getCicloInicioRS(
            const std::string& rs_id
        ) const; // ciclo_inicio do produtor pendente mais recente com esse nome

        // Métodos públicos
        void alocarRS(
            const std::string&,
            int
        );

        void desalocarRS(
            const std::string&,
            int,
            int
        );
    private:
        // Atributos
        char                     tipo{'Z'};
        int                      id;
        bool                     busy{false};
        std::vector<int>         tempo_inicio;
        std::vector<int>         tempo_fim;     // Pares (inicio[n] - fim[n]); fim == -1 enquanto pendente
        std::vector<std::string> RS_alocadas;
        // Métodos privados
        char identificaTipo(
            const std::string&
        );
        int identificaId(
            const std::string&
        );
};

// structs:
struct CDB {
    std::vector<Registrador> R;
    std::vector<Registrador> F;
};
struct UF {
    bool                     busy{false};
    int                      id{0};
    std::string              RS_atual{};
    std::vector<int>         tempo_alocacao{};
    std::vector<std::string> RS_alocadas{};
};
struct UnidadesFuncionais {
    std::vector<UF> acessar_memoria;
    std::vector<UF> ula_int_basico;
    std::vector<UF> ula_int_mult_div;
    std::vector<UF> ula_float_basico;
    std::vector<UF> ula_float_mult_div;
    int             wr{1};
    int             commit{1};
};
#endif
