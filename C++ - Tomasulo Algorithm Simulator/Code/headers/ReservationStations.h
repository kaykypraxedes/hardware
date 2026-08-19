/* headers/ReservationStations.h */

/**
 * @file ReservationStations.h
 *
 * @brief Módulo responsável gerenciar as operações que ocorrem
 * dentro de um módulo da Reservatio Station (RS).
 *
 * @details Dentro desssas responsabilidades estão:
 *
 * - Gerenciar dependências de instruções.
 * - Solucionar conflitos.
 * - Atualizar os hardwares (FUs, Registradores, etc.).
 * ...
 */

#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include "Instruction.h"
#include "Components.h"
#include <string>
#include <vector>
#include <utility>   // para std::pair
#include <memory>    // para std::shared_ptr
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr

namespace processor {

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Módulo individual da Reservation Station (RS).
 *
 * @details Região onde são realizadas as operações internas da
 * instrução e onde são atualizados os componentes (Registradores,
 * FUs, etc.).
 */
class RS {
    public:
        // Construtor:
        RS(
            const std::string& id
        );

        // Getters:
        bool                       IsBusy()              const;
        int                        GetCountdown()        const;
        int                        GetFUPosition()       const;
        INSTRUCTION_PHASE_TOMASULO GetInstructionPhase() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const Instruction&              GetCurrentInstruction()  const;
        const std::string&              GetId()                  const;
        const std::vector<int>&         GetTimes()               const;
        const std::vector<std::string>& GetInstructions()        const;
        const std::vector<std::pair<std::string,int>>& GetExQ()  const;
        const std::vector<std::pair<std::string,int>>& GetMemQ() const;

        // Demais métodos:

        /**
         * @brief Tenta alocar a instrução na RS.
         *
         * @details Se a RS estiver disponível, extrai os dados que
         * já estão prontos (V) e quais estão na espera (Q) e marca
         * no histórico.
         *
         * @param const std::shared_ptr<Instruction>& instruction -
         * Instrução que vai ser alocada.
         * @param CDB& cdb - CDB com o banco de registradores (para
         * verificar a disponibilidade dos dados).
         * @param const int cycle - Ciclo atual (início da alocação).
         *
         * @return true - Conseguiu fazer a alocação (issue).
         * @return false - RS estava indisponível.
         */
        bool AddIssue(
            const std::shared_ptr<Instruction>& instruction,
            CDB&                                cdb,
            const int                           cycle
        );

        /**
         * @brief Verifica se a instrução está apta para avançar de
         * fase no pipeline.
         *
         * @details Se sim, ele tenta fazer a alocação de uma FU e
         * faz as devidas marcações (qual a nova fase e o ciclo).
         *
         * @param CDB& cdb - CDB com o banco de registradores (para
         * verificar a disponibilidade).
         * @param FUNCTIONAL_UNITS& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Instrução avançou de fase.
         * @return false - Não avançou (decrementou o contador ou
         * estava em espera).
         */
        bool UpdateDependencies(
            CDB&              cdb,
            FUNCTIONAL_UNITS& fu,
            const int         cycle
        );

        /**
         * @brief Decrementa o contador de ciclos da fase atual. Se
         * a fase não está operando (espera), não faz nada.
         *
         * @details Esse método também libera as FUs que estavam
         * sendo utilizadas pelo RS e avança a fase do pipeline.
         *
         * @param FUNCTIONAL_UNITS& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - RS terminou a execução da fase.
         * @return true - RS ainda não terminou ou está em espera.
         */
        bool UpdateCountdown(
            FUNCTIONAL_UNITS& fu,
            const int         cycle
        );

        /**
         * @brief Se esta RS estiver esperando pelo produtor de outra
         * RS captura o valor (V) e limpa a pendência (Q).
         *
         * @details É uma escolha de implementação verificar apenas o
         * rs_id é verificado (não o start_cycle). Uma RS ocupada
         * sempre tem suas dependências resolvidas antes de ser
         * liberada, não havendo Q[i] stale de alocações anteriores.
         *
         * @param const std::string& rs_id - RS liberada.
         * @param const Register& value - Registrador liberado.
         */
        void ResolveDependency(
            const std::string& rs_id,
            const Register&    value
        );

        /**
         * @brief Libera a RS e indica o fim de um do seu uso no
         * "allocation_times".
         *
         * @param const int cycle - Ciclo atual (fim).
         */
    void Release(
        const int cycle
    );
    private:
        // Atributos:
        // - Elementos do RS:
        bool                         busy{false};
        int                          allocation_countdown{-1};
        int                          fu_position{-1};
        std::string                  id;    // Nome do RS ("load1", "addInt3", etc.).
        std::vector<Register>        ex_V;  // Registradores prontos (V) separados por ciclo do pipeline que eles travam.
        std::vector<Register>        mem_V;
        std::vector<std::pair<std::string,int>> ex_Q;        // Registradores em espera (Q) compostos por {rs_id, start_cycle}.
        std::vector<std::pair<std::string,int>> mem_Q;
        // - Elementos da instrução:
        std::shared_ptr<Instruction> current_instruction;    // Instrução (compartilhada com a tabela/ROB).
        INSTRUCTION_PHASE_TOMASULO   phase{INSTRUCTION_PHASE_TOMASULO::UNUSED};
        // - Histórico:
        std::vector<int>             allocation_times;       // De 2 em 2, início da alocação e fim da alocação.
        std::vector<std::string>     allocated_instructions; // De 1 em 1, instruções alocadas na RS.

        // Métodos:

        // - AddIssue()

        /**
         * @brief Realiza a limpeza e redefinição dos dados para o
         * novo issue, e ambém marca a nova alocação nos vetores
         * (V/Q).
         *
         * @param const std::shared_ptr<Instruction>& instruction -
         * Ponteiro para a instrução.
         * const int cycle - Ciclo atual.
         */
        void SetupNewIssue(
            const std::shared_ptr<Instruction>& instruction,
            const int                           cycle
        );

        /**
         * @brief Faz a leitura efetiva do registrador da fonte "idx"
         * e verifica se a alocação é em V[idx] (dado pronto) ou em
         * Q[idx] (dependente).
         *
         * @param const bool is_mem - Flag para identificar se é o
         * registrador representa uma fonte que opera na fase de EX
         * ou MEM.
         * @param const size_t idx - Posição dentro do vetor de
         * fontes.
         * @param const Register& src - Registrador alvo (que vai
         * ser procurado no CDB).
         * @param CDB& cdb - CDB com o banco de registradores para
         * acessar o registrador alvo.
         */
        void ReadSourceOperand(
            const bool      is_mem,
            const size_t    idx,
            const Register& src,
            CDB&            cdb
        );

        /**
         * @brief Marca no CDB todos os registradores de destino da
         * instrução como pendentes desta RS.
         *
         * @param const std::vector<Register>& dests - Registradores
         * de destino.
         * @param CDB& cdb - CDB com o banco de registradores para
         * acessar o registrador alvo.
         * @param const int cycle - Ciclo atual.
         */
        void AllocateDestInCDB(
            const std::vector<Register>& dests,
            CDB&                         cdb,
            const int                    cycle
        );

        // - UpdateDependencies()

        /**
         * @brief Verifica se a dependência de registrador (Q) já foi
         * resolvida para marcar o registrador na operação (V).
         *
         * @param const bool is_mem - Flag para identificar se é o
         * registrador representa uma fonte que opera na fase de EX
         * ou MEM.
         * @param const size_t idx - Posição dentro do vetor de
         * fontes.
         * @param CDB& cdb - CDB com o banco de registradores para
         * acessar o registrador alvo.
         */
        void CheckDependency(
            const bool   is_mem,
            const size_t idx,
            CDB&         cdb
        );

        /**
         * @brief Procura unidades funcionais para começar as
         * operações da instrução.
         *
         * @details Verifica a fase da instrução para ver para qual
         * estágio ele vai avançar que exige alocar uma FU (IS -> EX
         * ou EX -> MEM) e se ele está ápto para avançar (nenhuma
         * dependência Q).
         *
         * @param FUNCTIONAL_UNITS& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Conseguiu alocar em alguma FU.
         * @return false - Não conseguiu.
         */
        bool AdvancePhaseAllocation(
            FUNCTIONAL_UNITS& fu,
            const int         cycle
        );

        /**
         * @brief Determina o tipo de FU necessário para a instrução
         * e tenta fazer a alocação nela.
         *
         * @param FUNCTIONAL_UNITS& fu - Todas as unidades
         * funcionais.
         * @param const INSTRUCTION_PHASE_TOMASULO target_phase -
         * Fase alvo para avançar (próxima para a instrução).
         * @param const int cycle - Ciclo atual.
         * @param const int latency - Latência da fase nova.
         *
         * @return true - Conseguiu alocar em alguma FU.
         * @return false - Não conseguiu.
         */
        bool TryAllocateFU(
            FUNCTIONAL_UNITS&                fu,
            const INSTRUCTION_PHASE_TOMASULO target_phase,
            const int                        cycle,
            const int                        latency
        );

        /**
         * @brief Procura uma FU livre do grupo da instrução (para
         * operar a fase específica) e retorna sua posição.
         *
         * @param const std::vector<FU>& fu_group - Apenas o grupo de
         * unidades funcionais usado naquela instrução naquela fase.
         *
         * @return int - Posição da FU unitária encontrada.
         * @return -1 - Não tem nenhuma disponível.
         */
        int FindFreeFU(
            const std::vector<FU>& fu_group
        );

        // - UpdateCountdown()

        /**
         * @brief Procura a FU que estava alocada para a instrução da
         * RS, a desaloca e realiza as marcações de tempo.
         *
         * @param FUNCTIONAL_UNITS& fu - Todas as unidades
         * funcionais.
         * @param const INSTRUCTION_PHASE_TOMASULO finished_phase -
         * Fase da instrução (para descobrir em qual grupo procurar).
         * @param const int cycle - Ciclo atual (fim da alocação).
         */
        void ReleaseFU(
            FUNCTIONAL_UNITS&                fu,
            const INSTRUCTION_PHASE_TOMASULO finished_phase,
            const int                        cycle
        );
    };

// ─── STRUCT ───────────────────────────────────────────────────────

/**
 * @brief Conjunto de todos os módulos da RS para todos os tipos de
 * instrução.
 */
struct RESERVATION_STATION {
    std::vector<RS> load;
    std::vector<RS> store;
    std::vector<RS> int_basic;
    std::vector<RS> int_mult_div;
    std::vector<RS> float_basic;
    std::vector<RS> float_mult_div;
};

} // namespace processor

#endif
