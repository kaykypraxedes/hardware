/* headers/ReservationStations.h */
#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include <string>
#include <vector>
#include <utility>
#include "Instrucao.h"
#include "Componentes.h"

// classe:
class RS {
    public:
        // Construtor
        RS(std::string);
        // Getters
        bool                     getBusy()               const;
        int                      getContagemRegressiva() const;
        int                      getPosicaoUF()          const;
        const Instrucao&                getInstrucaoAtual()     const; // const & para evitar cópia
        FaseInstrucao                   getFaseInstrucao()      const;
        const std::string&              getId()                 const; // const & para evitar cópia
        const std::string&              getQj()                 const; // const & para evitar cópia
        const std::string&              getQk()                 const; // const & para evitar cópia
        const std::vector<int>&         getTempos()             const; // const & para evitar cópia
        const std::vector<std::string>& getInstrucoes()         const; // const & para evitar cópia
        // Métodos públicos
        bool addIssue(
            Instrucao&,
            CDB&,
            int
        );

        bool atualizarDependencias(
            CDB&,
            UnidadesFuncionais&,
            int
        );

        bool atualizaContagem(
            UnidadesFuncionais&,
            int
        );

        void liberar(
            int
        );

        void resolverDependencia(
            const std::string& rs_id,
            const Registrador& valor
        );
    private:
        // Atributos
        bool                        busy{false};
        bool                        destino_pendente_no_cdb{false};
        int                         contagem_regressiva_alocacao{-1};
        int                         posicao_UF{-1};
        std::string                 id;         // Nome do RS ("load1", "addInt3", etc.)
        Instrucao                   instrucao_atual;
        FaseInstrucao               fase;
        Registrador                 Vj;                            // Valor do operand J (se Qj vazio)
        Registrador                 Vk;                            // Valor do operand K (se Qk vazio)
        std::pair<std::string, int> Qj{"", -1};                    // Qj/Qk: {rs_id, ciclo_inicio} — produtor pendente
        std::pair<std::string, int> Qk{"", -1};                    // par vazio {"", -1} = operando disponível
        std::vector<int>            tempos_alocacao;     // De 2 em 2, início da alocação e fim da alocação
        std::vector<std::string>    instrucoes_alocacao; // De 1 em 1, instruções alocadas na RS
        // Métodos privados
        int procuraUFlivre(
            UnidadesFuncionais&,
            int,
            FaseInstrucao
        ) const;

        int alocarUFLivre(
            std::vector<UF>&,
            int
        ) const;

        void liberarUF(
            UnidadesFuncionais&,
            int,
            FaseInstrucao
        );

        void desalocarUFdoGrupo(
            std::vector<UF>&,
            int
        );
    };

// struct:
struct ReservationStations {
    std::vector<RS> load;
    std::vector<RS> store;
    std::vector<RS> int_basico;
    std::vector<RS> int_mult_div;
    std::vector<RS> float_basico;
    std::vector<RS> float_mult_div;
};

#endif
