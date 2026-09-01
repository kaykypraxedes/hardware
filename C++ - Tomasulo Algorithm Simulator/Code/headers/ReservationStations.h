/* headers/ReservationStations.h */

/**
 * @file ReservationStations.h
 *
 * @brief Módulo responsável por gerenciar as operações que ocorrem
 * dentro de um módulo da Reservatio Station (RS) e também definí-la
 * completamente ("struct" com o conjunto dos unidades do hardware).
 *
 * @details Dentro das responsabilidades de uma unidade da RS estão:
 *
 * - Gerenciar dependências de instruções.
 * - Solucionar conflitos.
 * - Atualizar os hardwares (FUs, Registradores, etc.).
 * ...
 */

#ifndef RESERVATION_STATIONS_H // Include guard
#define RESERVATION_STATIONS_H
#include "Architecture.h"
#include "Components.h"
#include <string>
#include <vector>
#include <memory>    // para std::shared_ptr
#include <cstddef>   // para std::size_t
#include <cstdlib>   // para std::abort
#include <iostream>  // para std::cerr
#include <algorithm> // para std::stable_sort

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
        std::size_t                GetCurrentStage()     const;
        INSTRUCTION_PHASE_TOMASULO GetInstructionPhase() const;
        // "const &" para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno).
        const Instruction&              GetCurrentInstruction()  const;
        const std::string&              GetId()                  const;
        const std::vector<int>&         GetTimes()               const;
        const std::vector<std::string>& GetInstructions()        const;
        const std::vector<Register>&    GetExValues()             const;
        const std::vector<int>&         GetExDependencies()       const;
        const std::vector<Register>&    GetMemValues()            const;
        const std::vector<int>&         GetMemDependencies()      const;

        // Demais métodos:

        /**
         * @brief Tenta alocar a instrução na RS.
         *
         * @details Se a RS estiver disponível, captura V/Q somente para a
         * etapa atual e reserva destinos apenas na primeira admissão lógica.
         *
         * @param const std::shared_ptr<Instruction>& instruction -
         * Instrução que vai ser alocada.
         * @param register_status Estado funcional dos registradores.
         * @param const int cycle - Ciclo atual (início da alocação).
         * @param new_instruction Indica a primeira admissão lógica.
         * @param stage Índice da etapa alocada na descrição completa.
         *
         * @return true - Conseguiu fazer a alocação (issue).
         * @return false - RS estava indisponível.
         */
        bool AddIssue(
            const std::shared_ptr<Instruction>& instruction,
            RegisterStatusTable&                register_status,
            const int                           cycle,
            const bool                          new_instruction,
            const std::size_t                   stage = 0
        );

        /**
         * @brief Verifica se a instrução está apta para avançar de
         * fase no pipeline.
         *
         * @details Se sim, ele tenta fazer a alocação de uma FU e
         * faz as devidas marcações (qual a nova fase e o ciclo).
         *
         * @param register_status Estado funcional consultado para reconhecer conclusões.
         * @param FuncionalUnits& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Instrução avançou de fase.
         * @return false - Não avançou (decrementou o contador ou
         * estava em espera).
         */
        bool UpdateDependencies(
            const RegisterStatusTable& register_status,
            FuncionalUnits&            fu,
            const int                  cycle
        );

        /**
         * @brief Decrementa o contador de ciclos da fase atual. Se
         * a fase não está operando (espera), não faz nada.
         *
         * @details Esse método também libera as FUs que estavam
         * sendo utilizadas pelo RS e avança a fase do pipeline.
         *
         * @param FuncionalUnits& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - RS terminou a execução da fase.
         * @return true - RS ainda não terminou ou está em espera.
         */
        bool UpdateCountdown(
            FuncionalUnits& fu,
            const int       cycle
        );

        /**
         * @brief Se esta RS estiver esperando por uma instrução produtora,
         * captura o valor (V) e limpa a pendência (Q).
         *
         * @param producer_position Identidade lógica liberada.
         * @param value Registrador liberado.
         */
        void ResolveDependency(
            const int       producer_position,
            const Register& value
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
        // - Elementos da instrução:
        std::shared_ptr<Instruction> current_instruction;    // Instrução (compartilhada com a tabela/ROB).
        std::size_t                   current_stage{};
        INSTRUCTION_PHASE_TOMASULO   phase{INSTRUCTION_PHASE_TOMASULO::UNUSED};
        std::vector<Register>        ex_V;
        std::vector<int>             ex_Q;
        std::vector<Register>        mem_V;
        std::vector<int>             mem_Q;
        // - Histórico:
        std::vector<int>             allocation_times;       // De 2 em 2, início da alocação e fim da alocação.
        std::vector<std::string>     allocated_instructions; // De 1 em 1, instruções alocadas na RS.

        // Métodos:

        // - AddIssue()

        /**
         * @brief Realiza a limpeza e redefinição dos dados para o
         * novo Issue e marca a nova ocupação física no histórico.
         *
         * @param const std::shared_ptr<Instruction>& instruction -
         * Ponteiro para a instrução.
         * const int cycle - Ciclo atual.
         * @param stage Índice da etapa alocada.
         */
        void SetupNewIssue(
            const std::shared_ptr<Instruction>& instruction,
            const int                           cycle,
            const std::size_t                   stage
        );

        /**
         * @brief Captura V/Q para um grupo de fontes da etapa atual.
         */
        void CaptureSources(
            const std::vector<Register>& sources,
            std::vector<Register>&       values,
            std::vector<int>&            dependencies,
            const RegisterStatusTable&   register_status,
            const int                    consumer_position
        );

        /**
         * @brief Reconhece conclusões para Qs vinculados ao Issue atual.
         */
        void RefreshDependencyGroup(
            const std::vector<Register>& sources,
            std::vector<Register>&       values,
            std::vector<int>&            dependencies,
            const RegisterStatusTable&   register_status
        );

        /**
         * @brief Resolve localmente ocorrências exatas de um broadcast.
         */
        void ResolveDependencyInGroup(
            const std::vector<Register>& sources,
            std::vector<Register>&       values,
            std::vector<int>&            dependencies,
            const int                    producer_position,
            const Register&              value
        );

        static bool SameReference(
            const Register& left,
            const Register& right
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
         * @param FuncionalUnits& fu - Todas as unidades
         * funcionais.
         * @param const int cycle - Ciclo atual.
         *
         * @return true - Conseguiu alocar em alguma FU.
         * @return false - Não conseguiu.
         */
        bool AdvancePhaseAllocation(
            FuncionalUnits& fu,
            const int       cycle
        );

        /**
         * @brief Determina o tipo de FU necessário para a instrução
         * e tenta fazer a alocação nela.
         *
         * @param FuncionalUnits& fu - Todas as unidades
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
            FuncionalUnits&                  fu,
            const INSTRUCTION_PHASE_TOMASULO target_phase,
            const int                        cycle,
            const int                        latency
        );

        // - UpdateCountdown()

        /**
         * @brief Procura a FU que estava alocada para a instrução da
         * RS, a desaloca e realiza as marcações de tempo.
         *
         * @param FuncionalUnits& fu - Todas as unidades
         * funcionais.
         * @param const INSTRUCTION_PHASE_TOMASULO finished_phase -
         * Fase da instrução (para descobrir em qual grupo procurar).
         * @param const int cycle - Ciclo atual (fim da alocação).
         */
        void ReleaseFU(
            FuncionalUnits&                  fu,
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
    public:
        RESERVATION_STATION() = default;

        /**
         * @brief Inicializa os seis grupos físicos na ordem configurável atual.
         */
        explicit RESERVATION_STATION(
            const RESERVATION_STATION_CAPACITIES& capacities
        );

        const std::vector<RS>& GetLoadStations() const;
        const std::vector<RS>& GetStoreStations() const;
        const std::vector<RS>& GetIntBasicStations() const;
        const std::vector<RS>& GetIntMultDivStations() const;
        const std::vector<RS>& GetFloatBasicStations() const;
        const std::vector<RS>& GetFloatMultDivStations() const;

        /**
         * @brief Roteia e tenta alocar uma etapa na primeira RS livre.
         */
        bool AddIssue(
            const std::shared_ptr<Instruction>& instruction,
            RegisterStatusTable&                register_status,
            const int                           cycle,
            const bool                          new_instruction,
            const std::size_t                   stage = 0
        );

        bool IsPositionAllocated(
            const int position
        ) const;

        void ReleaseByPosition(
            const int position,
            const int cycle
        );

        /**
         * @brief Coleta candidatas locais e as ordena por posição lógica.
         */
        std::vector<RS*> CollectReadyCandidates();

        /**
         * @brief Coleta todas as células ocupadas na ordem física dos grupos.
         */
        std::vector<RS*> CollectBusyStations();

        /**
         * @brief Distribui um evento transitório para todas as RSs ocupadas.
         *
         * @param broadcast Identidade do produtor e destino transmitido.
         */
        void ResolveBroadcast(
            const CDB_BROADCAST& broadcast
        );
    private:
        std::vector<RS> load;
        std::vector<RS> store;
        std::vector<RS> int_basic;
        std::vector<RS> int_mult_div;
        std::vector<RS> float_basic;
        std::vector<RS> float_mult_div;

        std::vector<RS>& GetGroup(
            const RESERVATION_STATION_GROUP group
        );

        std::vector<std::vector<RS>*> GetGroups();

        std::vector<const std::vector<RS>*> GetGroups() const;
};

} // namespace processor

#endif
