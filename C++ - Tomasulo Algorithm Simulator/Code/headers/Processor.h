/* headers/Processor.h */

/**
 * @file Processor.h
 *
 * @brief Módulo responsável por gerenciar as threads (avançar o
 * ciclo, trocar de thread, etc).
*/

#ifndef PROCESSOR_H   // Include guard
#define PROCESSOR_H
#include "Thread.h"
#include "Architecture.h"
#include <string>
#include <vector>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr

namespace processor {

// ─── ENUMS ────────────────────────────────────────────────────────

enum class PROCESSOR_TYPE {
    IN_ORDER,
    TOMASULO_CLASSIC,     // Sem ROB
    TOMASULO_ESPECULATIVE // Com ROB
};
enum class MULTITHREADING_MODEL {
    NONE,
    SMT,
    FINE_GRAINED,
    COARSE_GRAINED
};

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Classe onde são armazenadas as threads e a suas execuçẽoes
 * são gerenciadas.
 */
class Processor {
    public:
        // Construtor:
        Processor(
            const int,
            const bool,
            const PROCESSOR_TYPE,
            const MULTITHREADING_MODEL,
            const std::vector<std::string>&,
            const PIPELINE_CONFIGURATION&,
            const std::vector<int>& = {},
            const std::vector<LATENCY_OVERRIDE>& = {},
            const ARCHITECTURE = ARCHITECTURE::SIMPLIFIED
        );

        // Getters:
        PROCESSOR_TYPE GetType() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const Thread& GetThread(
            const int
        ) const;
        const std::vector<TABLE_ROW>& GetThreadTable(
            const int
        ) const;

        // Método:

        /**
         * @brief
         *
         * @details É uma escolha de implementação que as
         * "Reservation Stations" sejam liberadas antes do "Issue" no
         * mesmo ciclo.
         *
         * Dessa maneira, elas podem ser reutilizadas imediatamente
         * por novas instruções emitidas após serem liberadas (não
         * precisa de um ciclo de liberação).
         *
         * @return true - Todas as instruções foram finalizadas.
         * @return false - Ainda faltam instruções a serem
         * processadas.
         */
        bool ExecuteCycle();
    private:
        // Atributos:
        PIPELINE_CONFIGURATION configuration;
        int                  current_cycle{1};
        int                  thread_pointer{};
        int                  fus_per_type{1};
        bool                 has_predictor;
        PROCESSOR_TYPE       type;
        MULTITHREADING_MODEL mt_model;
        std::vector<Thread>  threads;

        // Métodos:

        /**
         * @brief Executa as fases "ex", "mem", "wr" e "commit" das
         * instruções já adicionadas.
         *
         * @return true - Todas as instruções foram finalizadas
         * (todas as fases e tabela completa).
         * @return false - Ainda faltam instruções a serem
         * processadas.
         */
        bool ExecuteOperations();

        /**
         * @brief Tenta adicionar a próxima instrução da fila em uma
         * célula da RS (issue).
         *
         * @details Loop de dispatch (a política de escalonamento
         * varia conforme o modelo de multithreading):
         *
         * - SMT: Rotaciona a cada dispatch múltiplas threads a cada
         * cíclo.
         * - Granulação Fina: Mantém thread até falhar e troca a
         * cada cíclo.
         * - Granulação Grossa: Mantém thread até falhar e só troca
         * em stalls longos.
         */
        void AddInstructions();
};

} // namespace processor

#endif
