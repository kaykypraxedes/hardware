/* headers/Thread.h */

/**
 * @file Thread.h
 *
 * @brief Módulo responsável por gerenciar as operações que ocorrem
 * dentro de uma thread.
 *
 * @details Dentro dessas responsabilidades estão:
 *
 * - Enviar as instruções para a RS.
 * - Gerenciar seus componentes de hardware (CDB, FUs, RS, etc.).
 * - Realizar previsões de "branches".
 * ...
 */

#ifndef THREAD_H     // Include guard
#define THREAD_H
#include "Architecture.h"
#include "InstructionFactory.h"
#include "ReservationStations.h"
#include "Components.h"
#include <string>
#include <vector>
#include <memory>    // para std::unique_ptr e std::shared_ptr
#include <algorithm> // para std::stable_sort
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr

namespace processor {

// ─── STRUCT ───────────────────────────────────────────────────────

/**
 * @brief Estrutura que agrupa todas as informações necessárias para
 * imprimir com detalhes o comportamento ciclo a ciclo de cada
 * instrução enviada (bem como seu estado final).
 *
 * @details Por conveniência, a estrutura também foi aproveitada para
 * armazenar os dados de cada instrução internamente (reutilização da
 * estrutura pronta para evitar escrita duplicada de código).
 */
struct TABLE_ROW {
    // - Thread é a "dona" do objeto via shared_ptr (RS e ROB guardam cópias do ponteiro).
    std::shared_ptr<Instruction> instruction;
    int                          issue_cycle{-1};
    std::vector<int>             ex_cycles;
    std::vector<int>             mem_cycles;
    int                          wr_cycle{-1};
    int                          commit_cycle{-1};
};

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Thread única de um processador, responsável por gerenciar
 * suas instruções e com conjunto de hardware próprio.
 *
 * @details Unidade responsável por:
 *
 * - Decodificação as instruções (de string para Instruction);
 * - Gerenciamento do hardware (CDB, FU, RS, etc).
 * - Registrar todos os dados na tabela (pelo qual é realizado o
 * histórico e são utilizados os dados para saber qual etapa deve
 * ser realizada).
 * ...
 */
class Thread {
    public:
        // Construtor:
        Thread(
            const std::vector<std::string>&      assembly, // Único dado que não pode ser inferido.
            const std::vector<LATENCY_OVERRIDE>& latency_overrides = {},
            const std::vector<int>&              num_rs = {},
            const std::vector<int>&              num_fus = {},
            const std::vector<int>&              switch_cycles = {},
            const int                            dispatch_width = 1,
            const int                            rob_capacity = 0,
            const bool                           has_predictor = false,
            const ARCHITECTURE                   arch = ARCHITECTURE::SIMPLIFIED
        );

        // Getters:
        int GetCurrentInstructionPosition() const;
        // "const &" para evitar cópia (para tipos básicos o ganho é marginal).
        const CDB&                    GetCDB()   const;
        const RESERVATION_STATION&    GetRS()    const;
        const FUNCTIONAL_UNITS&       GetFU()    const;
        const std::vector<TABLE_ROW>& GetTable() const;

        // Demais métodos:

        /**
         * @brief Método para marcar a troca de ciclos que é efetuada
         * em multithread de "granulação grossa" (Coarse-Grained).
         *
         * @return true - É o ciclo de troca.
         * @return false - Não é.
         */
        bool IsSwitchCycle();

        // - Estágios da pipeline:

        /**
         * @brief Tenta adicionar uma intrução em uma RS disponível
         * (issue).
         *
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Instrução adicionada.
         * @return false - Nenhuma unidade da RS disponível.
         */
        bool Issue(
            const int cycle
        );

        /**
         * @brief Verifica se as instruções já estão aptas a começar
         * a fase EX/MEM.
         *
         * @details Se não há nenhuma dependência (Q vazio), ele
         * tenta alocar a FU e posteriormente marca na tabela.
         *
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Todas as instruções já foram finalizadas
         * ("wr" e/ou "commit").
         * @return false - Ainda faltam instruções.
         */
        bool ExMem(
            const int cycle
        );

        /**
         * @brief Verifica quais instruções já foram devidamente
         * processadas para marcar seu fim na tabela.
         *
         * @details Para garantir a coerência das dependências de RS,
         * o método primeiro marca na tabela as instruções que
         * estavam na fase "wr" para depois detectar instruções que
         * passaram para essa fase nesse ciclo.
         *
         * Essa implementação é necessária pela ordem de chamada das
         * funções pelo processador - primeiro é realizado o "wr"
         * para depois realizar o "is". Dessa maneira, uma instrução
         * teria acesso à um dado de no mesmo ciclo de "wr" (só pode
         * ocorrer no próximo ciclo).
         *
         * @param const int cycle - Ciclo atual.
         */
        void Wr(
            const int cycle
        );

        /**
         * @brief Verifica instruções aptas ao "commit" e as marca na
         * tabela.
         *
         * @details É importante salientar que esse método só
         * funciona se a thread possuir um ROB e que a quantidade de
         * instruções commitadas no ciclo são limitadas por:
         *
         * 1. Quantidade de instruções finalizadas;
         * 2. Despacho da thread.
         *
         * @param const int cycle - Ciclo atual.
         */
        void Commit(
            const int cycle
        );
    private:
        // Atributos:
        // - Elementos da thread:
        bool                     has_rob{false};
        int                      rob_capacity{1};
        int                      current_instruction_position{};
        bool                     has_predictor{false};
        CDB                      cdb;
        RESERVATION_STATION      rs;
        FUNCTIONAL_UNITS         fu;
        std::vector<int>         wr_buffer;
        std::vector<int>         pending_wr_buffer;
        std::vector<TABLE_ROW>   instruction_table;
        std::vector<std::shared_ptr<Instruction>> rob; // Guarda ponteiros compartilhados.
        // - Elementos auxiliares/histórico:
        int                      num_finished_instructions{};
        int                      num_committed_instructions{};
        int                      commit_pointer{};
        int                      unresolved_branch_position{-1};
        std::vector<int>         switch_cycles;        // Para processadores com granulação grossa.

        // Métodos:

        /**
         * @brief Inicializa os componentes de hardware (CDB, FU, RS,
         * etc.)
         *
         * @details A alocação em parte é configurável (como o número
         * de unidades que cada grupo de FU e RS terá), mas alguns
         * são inalteráveis (como o CDB com o banco de registradores,
         * para não gerar divergências com a arquitetura).
         *
         * @param const std::vector<int>& num_rs - Vetor com o número
         * de unidades em cada grupo de RS.
         * @param const std::vector<int>& num_fus - Vetor com o número
         * de unidades em cada grupo de FU.
         * @param const int dispatch_width - Largura de despacho.
         * Indica quantas instruções podem ser iniciadas (issue) e
         * commitadas de uma vez.
         * @param const ARCHITECTURE arch - Qual a arquitetura (para
         * montar o banco de registradores).
         */
        void InitializeComponents(
            const std::vector<int>& num_rs,
            const std::vector<int>& num_fus,
            const int               dispatch_width,
            const ARCHITECTURE      arch
        );

        // - EX/MEM

        /**
         * @brief Inicia a as fases de execução da instrução
         * (IS -> EX_inicio ou EX_concluido -> MEM) se não tem mais
         * nenhuma dependência (Q vazio) e tem uma FU disponível.
         *
         * @details O método inicia a nova fase das instruções com
         * prioridade na ordenação (instrição mais antiga -> mais
         * nova).
         *
         * @param const int cycle - Ciclo atual.
         */
        void StartPhase(
            const int cycle
        );

        // - WR (Write Result)

        /**
         * @brief Faz a marcação na tabela e propaga o resultado do
         * registrador liberado nos componentes (dentro do CDB e nas
         * unidades da RS).
         *
         * @param const int cycle - Ciclo atual.
         */
        void PerformWriteResult(
            const int cycle
        );

        /**
         * @brief Propaga o resultado no CDB (libera os
         * registradores), resolve as dependências nos RSs que
         * estavam esperando ("Q"s pendentes) e libera a célula da RS
         * produtora.
         *
         * @param const int position - Posição da instrução na
         * tabela.
         * @param const int cycle - Ciclo atual.
         * @param const INSTRUCTION_TYPE instr_type - Tipo da
         * instrução (para identificar o grupo de RS).
         */
        void WriteResultOnComponents(
            const int              position,
            const int              cycle,
            const INSTRUCTION_TYPE instr_type
        );

        /**
         * @brief Encontra instruções que encerraram sua fase após a
         * passagem do ciclo.
         *
         * @details É importante salientar que essas instruções não
         * estão necessariamente aptas a começar uma nova fase pois
         * as dependências são verificadas no método "StartPhase()".
         *
         * Se a instrução tiver acabado todas as suas execuções, ela
         * é enviada para o buffer de "wr" (já que a impressão
         * depende da capacidade do hardware).
         *
         * @param const int cycle - Ciclo atual.
         */
        void DetectPhaseTransitions(
            const int cycle
        );
};

} // namespace processor

#endif
