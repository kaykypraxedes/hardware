/* headers/Components.h */

/**
 * @file Components.h
 *
 * @brief Módulo responsável pela definição dos componentes básicos de hardware
 * do processador.
 *
 * @details Esse arquivo é composto principalmente dos componentes:
 *
 * - Registradores;
 * - Common Data Bus (CDB);
 * - Funcional Unitys (FU);
 *
 * Ele também fornece "structs" auxiliares e "helpers" utilizados em outros
 * arquivos independentemente e/ou intermediariamente em funções e estruturas
 * mais complexas.
 */

#ifndef COMPONENTS_H  // Include guard
#define COMPONENTS_H
#include <string>
#include <vector>
#include <cstdlib>    // para std::abort
#include <iostream>   // para std::cerr


namespace processor {

// ─── DICIONÁRIO ───────────────────────────────────────────────────

/*
 * - Produtor: Instrução que usa o registrador como destino para o
 * resultado da sua operação (basicamente aloca o registrador).
 * Sua identidade lógica é a posição da instrução dentro da Thread;
 * a RS identifica somente sua localização física atual.
 * Podem existir múltiplos produtores simultâneos do mesmo
 * registrador (WAW).
 */

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Referência arquitetural imutável de registrador.
 */
class Register {
    public:
        // Construtores:
        Register() = default;
        Register(
            const char type,
            const int  id,
            const int  mask = 255 // Máscara para identificar sombreamento de registradores.
        );

        // Getters:
        // Não são "const &" pois tem ganho marginal pequeno (por serem de tipos simples).
        char GetType() const;
        int  GetId()   const;
        int  GetMask() const;
    private:
        char type{'Z'};

        /**
         * @brief Localização física do registrador.
         */
        int  id{-1};

        /**
         * @brief Máscara de bits para identificação de sobreposição
         * de registradores.
         *
         * @details A identificação do é feita pelo registrador par
         * (id, mask):
         * - id (localização física do registrador);
         * - mask (posição de ocupação desse registrador em relação
         * ao registrador principal - 100%);
         *
         * Em algumas arquitetura os registradores podem ser
         * sobrepostos (um registrador de 32 bits é apenas a metade
         * inferior de um de 64 bits, por exemplo), e essa máscara é
         * importante para não gerar falsas dependências.
         *
         * Em x86, por exemplo, existem os registradores com o mesmo
         * id (compartilham o mesmo hardware):
         * - "ax" (16 bits, id = 0, mask = 00000011);
         * - "al" (8 bits,  id = 0, mask = 00000001);
         * - "ah" (8 bits,  id = 0, mask = 00000010);
         * O "ax" bloqueia os outros dois registradores, mas o "al"
         * ou o "ah" só bloqueiam o "ax" (visto que eles ocupam
         * máscaras que não se intersectam).
         */
        int  mask{255};
};

/**
 * @brief Visualização somente leitura de um slot do Register Status.
 */
struct REGISTER_STATUS_VIEW {
    Register                 reference;
    bool                     busy{false};
    std::vector<int>         producer_positions;
    std::vector<std::string> allocated_rs;
    std::vector<int>         allocation_times;
};

/**
 * @brief Mantém produtores lógicos e trace por referência arquitetural exata.
 */
class RegisterStatusTable {
    public:
        RegisterStatusTable() = default;

        /**
         * @brief Inicializa o estado persistente a partir de um layout.
         *
         * @param references Referências arquiteturais em ordem física.
         */
        explicit RegisterStatusTable(
            const std::vector<Register>& references
        );

        /**
         * @brief Adiciona uma referência exata ao layout.
         */
        void AddReference(
            const Register& reference
        );

        /**
         * @brief Retorna uma visualização somente leitura do slot.
         */
        REGISTER_STATUS_VIEW FindStatus(
            const Register& reference
        ) const;

        /**
         * @brief Registra um produtor funcional e seu evento inicial de trace.
         */
        void AllocateProducer(
            const Register&    reference,
            const int          producer_position,
            const std::string& rs_id,
            const int          cycle
        );

        /**
         * @brief Conclui um produtor funcional e seu evento de trace.
         *
         * @return true quando encontrou o produtor pendente.
         */
        bool DeallocateProducer(
            const Register& reference,
            const int       producer_position,
            const int       cycle
        );

        /**
         * @brief Encontra o produtor de maior posição anterior à consumidora.
         *
         * @return Posição do produtor, concluído ou pendente; -1 se ausente.
         */
        int FindLatestProducerBefore(
            const Register& reference,
            const int       consumer_position
        ) const;

        /**
         * @brief Verifica se um produtor conhecido concluiu.
         */
        bool IsProducerResolved(
            const Register& reference,
            const int       producer_position
        ) const;

        /**
         * @brief Deriva busy da existência de produtores pendentes.
         */
        bool IsBusy(
            const Register& reference
        ) const;

        /**
         * @brief Retorna as referências na ordem do layout.
         */
        std::vector<Register> GetReferences() const;

        /**
         * @brief Retorna visualizações na ordem do layout.
         */
        std::vector<REGISTER_STATUS_VIEW> GetStatuses() const;

        /**
         * @brief Retorna a quantidade de referências do layout.
         */
        std::size_t Size() const;
    private:
        struct PRODUCER_RECORD {
            int  position{-1};
            bool pending{false};
        };

        struct TRACE_RECORD {
            int         producer_position{-1};
            std::string rs_id;
            int         start_cycle{-1};
            int         end_cycle{-1};
        };

        struct STATUS_ENTRY {
            Register                     reference;
            std::vector<PRODUCER_RECORD> producers;
            std::vector<TRACE_RECORD>    trace;
        };

        std::vector<STATUS_ENTRY> entries;

        STATUS_ENTRY& FindEntry(
            const Register& reference
        );

        const STATUS_ENTRY& FindEntry(
            const Register& reference
        ) const;

        static bool SameReference(
            const Register& left,
            const Register& right
        );

        static REGISTER_STATUS_VIEW MakeView(
            const STATUS_ENTRY& entry
        );
};

// ─── STRUCTS ──────────────────────────────────────────────────────

/**
 * @brief Faixa de apresentação de referências do banco de registradores.
 */
struct REGISTER_BANK {
    char reg_class; // Classe de registrador (nomeclatura varia com a arquitetura).
    int  base;      // Primeiro id físico da faixa.
    int  count;     // Quantidade de registradores da faixa.
};

/**
 * @brief Descrição imutável das referências e faixas de uma arquitetura.
 */
struct REGISTER_LAYOUT {
    std::vector<Register>      references;
    std::vector<REGISTER_BANK> banks;
};

/**
 * @brief Evento transitório transmitido pelo Common Data Bus.
 *
 * @details A conclusão permanece no Register Status. O evento contém somente
 * a identidade necessária para cada RS decidir se resolve algum Q.
 */
struct CDB_BROADCAST {
    int      producer_position{-1};
    Register destination;

    /**
     * @brief Conclui o produtor persistente associado ao evento.
     *
     * @param register_status Estado persistente dos produtores.
     * @param cycle Ciclo da conclusão.
     *
     * @return true na primeira conclusão; false para produtor ausente ou já
     * concluído.
     */
    bool CompleteProducer(
        RegisterStatusTable& register_status,
        const int            cycle
    ) const;
};

/**
 * @brief Funcional Unity (FU): Componente de hardware individual que
 * executa uma operação.
 *
 * @details Esse componente pode ser o responsável por:
 * - Acessar a memória;
 * - Operações aritméticas básicas com inteiros;
 * - Divisão com floats;
 * ...
 */
struct FU {
    bool                     busy{false};
    std::string              current_rs;
    std::vector<int>         allocation_times;
    std::vector<std::string> allocated_rs;
};

/**
 * @brief Grupos físicos controlados pelo banco de estações de reserva.
 */
enum class RESERVATION_STATION_GROUP {
    LOAD,
    STORE,
    INT_BASIC,
    INT_MULT_DIV,
    FLOAT_BASIC,
    FLOAT_MULT_DIV
};

/**
 * @brief Grupos físicos controlados pelo banco de unidades funcionais.
 */
enum class FUNCTIONAL_UNIT_GROUP {
    MEMORY_ACCESS,
    INT_BASIC,
    INT_MULT_DIV,
    FLOAT_BASIC,
    FLOAT_MULT_DIV
};

/**
 * @brief Capacidades nomeadas dos grupos de estações de reserva.
 */
struct RESERVATION_STATION_CAPACITIES {
    int load{5};
    int store{5};
    int int_basic{5};
    int int_mult_div{4};
    int float_basic{3};
    int float_mult_div{2};
};

/**
 * @brief Capacidades nomeadas dos grupos de unidades funcionais.
 */
struct FUNCTIONAL_UNIT_CAPACITIES {
    int memory_access{1};
    int int_basic{1};
    int int_mult_div{1};
    int float_basic{1};
    int float_mult_div{1};
};

/**
 * @brief Conjunto de todas as unidades funcionais.
 *
 * @details As de mesmo tipo são agrupados em vetores de tamanho
 * configurável.
 *
 */
class FuncionalUnits {
    public:
        FuncionalUnits() = default;

        /**
         * @brief Inicializa os grupos físicos com capacidades nomeadas.
         *
         * @param capacities Capacidades dos grupos de FU.
         */
        explicit FuncionalUnits(
            const FUNCTIONAL_UNIT_CAPACITIES& capacities
        );

        const std::vector<FU>& GetMemoryAccessUnits() const;
        const std::vector<FU>& GetIntBasicUnits() const;
        const std::vector<FU>& GetIntMultDivUnits() const;
        const std::vector<FU>& GetFloatBasicUnits() const;
        const std::vector<FU>& GetFloatMultDivUnits() const;

        /**
         * @brief Aloca a primeira FU livre do grupo solicitado.
         *
         * @return Índice alocado ou -1 quando o grupo está saturado.
         */
        int Allocate(
            const FUNCTIONAL_UNIT_GROUP group,
            const std::string&          rs_id,
            const int                   cycle
        );

        /**
         * @brief Libera atomicamente uma FU pertencente à RS informada.
         */
        void Release(
            const FUNCTIONAL_UNIT_GROUP group,
            const int                   position,
            const std::string&          rs_id,
            const int                   cycle
        );
    private:
        std::vector<FU> memory_access;
        std::vector<FU> int_basic_alu;
        std::vector<FU> int_mult_div_alu;
        std::vector<FU> float_basic_alu;
        std::vector<FU> float_mult_div_alu;

        std::vector<FU>& GetGroup(
            const FUNCTIONAL_UNIT_GROUP group
        );

        bool HasAllocation(
            const std::string& rs_id
        ) const;
};

// ─── HELPER ───────────────────────────────────────────────────────

/**
 * @brief Converte as seis capacidades externas de RS para campos nomeados.
 */
RESERVATION_STATION_CAPACITIES MakeReservationStationCapacities(
    const std::vector<int>& values
);

/**
 * @brief Converte as cinco primeiras capacidades externas de FU.
 *
 * @details O formato externo possui uma sexta posição reservada à largura de
 * WR. Ela é validada, mas não pertence ao banco de FUs.
 */
FUNCTIONAL_UNIT_CAPACITIES MakeFunctionalUnitCapacities(
    const std::vector<int>& values
);

/**
 * @brief Retorna a largura de WR da sexta posição do formato externo.
 */
int GetExternalWriteResultWidth(
    const std::vector<int>& values
);

} // namespace processor

#endif
