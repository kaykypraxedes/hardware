/* headers/Processador.h */
#ifndef PROCESSADOR_H // Include guard
#define PROCESSADOR_H
#include <string>
#include <vector>
#include "Instrucao.h"
#include "Thread.h"

//enums:
enum class TipoProcessador {
    IN_ORDER,
    TOMASULO_SEM_ROB,
    TOMASULO_COM_ROB
};
enum class ModeloMultithreading {
    SMT,
    GRANULACAO_FINA,
    GRANULACAO_GROSSA
};

// Structs
struct LinhaCiclos{
    std::string thread;
    Instrucao   instrucao;
};

// classe:
class Processador {
    public:
        // Construtor
        Processador(
            int,
            int,
            bool,
            TipoProcessador,
            ModeloMultithreading,
            const std::vector<std::string>&,
            const std::vector<int>& = {},
            std::vector<int> = {},
            const std::vector<int>& = {}
        );
        // Métodos públicos
        // Se for granulação grossa, os 2 últimos parâmetros são o num máximo de stalls e o número do PC das instruções delimitadoras
        bool                     executarCiclo();
        std::vector<LinhaCiclos> getTabelaCiclos() const;
        TipoProcessador          getTipo()         const;

        Thread getThread(
            int
        ) const;

        std::vector<LinhaTabela> getTabelaThread(
            int
        ) const;
    private:
        // Atributos
        int                  largura_de_despacho{1};
        int                  ciclo_atual{1};
        int                  ponteiro_da_thread{};
        int                  num_ufs_por_tipo{1};
        bool                 tem_previsor;
        TipoProcessador      tipo;
        ModeloMultithreading modelo_MT;
        std::vector<Thread>  threads;
        // Métodos privados
        bool executarExMemWr();
        void executarIssue();
        void avancarPonteiroRoundRobin();

        void iniciarThreads(
            const std::vector<std::string>&,
            bool,
            int,
            const std::vector<int>&,
            const std::vector<int>&,
            const std::vector<int>& = {}
        );
};

#endif
