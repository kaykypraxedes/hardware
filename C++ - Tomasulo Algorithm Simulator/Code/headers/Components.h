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
 * @brief Registrador individual do banco.
 *
 * @details A implementação atual não armazenamento de valores
 * (adaptação necessária caso seja usado como base para a
 * implementação de um emulador).
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
        bool GetBusy() const;

        // Não são "const &" pois enviam dados de variáveis locais (apagados ao final da função).

        /**
         * @brief Agrupa em um vetor com todos os tempos de alocação
         * em pares (posições pares são os "start_cycles" e os
         * impares são os "end_cycles" correspondentes).
         *
         * @return std::vector<int> - Tempos de alocação.
         */
        std::vector<int> GetAllocationTimes() const;

        /**
         * @brief Indica a posição do produtor pendente mais recente.
         *
         * @details É esse produtor que novas instruções devem
         * aguardar em caso de WAW.
         *
         * A lógica se mantem funcional e eficiente apesar do seu uso
         * em conjunto com a renomeação de registradores (flag que é
         * opcional).
         *
         * @return Posição lógica do produtor ou -1 quando não houver.
         */
        int GetCurrentProducer() const;

        /**
         * @brief Retorna as posições lógicas dos produtores registrados.
         *
         * @return Vetor alinhado com GetAllocatedRS() e GetAllocationTimes().
         */
        const std::vector<int>& GetProducerPositions() const;

        /**
         * @brief Retorna o histórico das RSs físicas iniciais dos produtores.
         *
         * @return const std::vector<std::string>& - Nome da RS física
         * inicial de cada produtor.
         */
        const std::vector<std::string>& GetAllocatedRS() const; // "const &" para evitar cópia de dados grandes.

        // Demais métodos:

        /**
         * @brief Inverte a flag "busy" do registrador (se era "true"
         * vira false e vice-versa).
         */
        void ToggleBusy();

        /**
         * @brief Verifica se o produtor já finalizou a operação e
         * liberou o registrador.
         *
         * @param producer_position Posição lógica do produtor.
         *
         * @return true quando o produtor terminou; false quando ainda
         * estiver pendente ou não existir.
         */
        bool IsDependencyResolved(
            const int producer_position
        ) const;

        /**
         * @brief Registra o novo produtor do registrador.
         *
         * @details A posição identifica a dependência; RS e ciclos são
         * preservados somente como histórico físico e visual.
         *
         * @param producer_position Posição lógica da instrução.
         * @param rs_id Nome da RS física inicial.
         * @param cycle Ciclo de início da alocação.
         */
        void AllocateProducer(
            const int          producer_position,
            const std::string& rs_id,
            const int          cycle
        );

        /**
         * @brief Registra o fim da alocação daquele produtor.
         *
         * @details Esse fim não garante necessariamente
         * "busy == false" pois podem existir mais produtores pendentes,
         * e marca o fim do ciclo correspondente à posição lógica.
         *
         * @param producer_position Posição lógica da instrução.
         * @param cycle Ciclo de fim da alocação.
         *
         * @return true - Desalocado com sucesso.
         * @return false - Não desalocou (o for pode não ter
         * encontrado nenhuma correspondência).
         */
        bool DeallocateProducer(
            const int producer_position,
            const int cycle
        );
    private:
        // Atributos:
        // - Elementos do registrador (valor default).
        char                     type{'Z'};
        bool                     busy{false};

        /**
         * @brief Localização física do registrador.
         */
        int                      id{-1};

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
        int                      mask{255};    // Em binário: 255 = 11111111 (usa 100% do registrador principal).

        // - Dados para a impressão (debugging).
        std::vector<int>         start_times;
        std::vector<int>         end_times;    // Pares (inicio[n] - fim[n]); fim == -1 enquanto pendente.
        std::vector<std::string> allocated_rs;
        std::vector<int>         producer_positions;

        // Método:

        /**
         * @brief Procura produtores pendentes.
         *
         * @return true - Ainda existem.
         * @return false - Não existem.
         */
        bool HasPendingProducer() const;
};

// ─── STRUCTS ──────────────────────────────────────────────────────

/**
 * @brief "struct" auxiliar para delimitar os tipos de registradores
 * dentro do vetor.
 */
struct CDB_BANK {
    char reg_class; // Classe de registrador (nomeclatura varia com a arquitetura).
    int  base;      // Primeiro id físico da faixa.
    int  count;     // Quantidade de registradores da faixa.
};

/**
 * @brief Common Data Bus (CDB): Simplificação de implementação.
 * Ferramenta para a transmissão dos dados entre os componentes.
 *
 * @details Essa implementação já representa a escrita do dado
 * nos registradores reais em fase pré-commit (as instruções
 * buscam sempre o dado mais atualizado, independente se ele está
 * armazenado nos registradores intermediários do ROB ou no banco
 * de registradores - pós-commit).
 *
 * - Essa simplificação é um possível ponto de modificação em caso de
 * aproveitamento da estrutura do projeto para a montagem de um
 * emulador que efetivamente realiza as operações (o flush iria ter
 * que alterar dados que já estavam consolidados no banco).
 *
 * - Uma possível adaptação seria criar um vetor rob_registers que
 * armazenariam os dados pré-commit e limpá-los após o commit (esses
 * sendo a ferramenta de busca primária de dados - mais atualizados).
 */
struct CDB {
    std::vector<Register> registers;
    std::vector<CDB_BANK> print_banks;
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
 * @brief Conjunto de todas as unidades funcionais.
 *
 * @details As de mesmo tipo são agrupados em vetores de tamanho
 * configurável.
 *
 * Os componentes "wr" (Write Result) e "commit" são apenas um valor
 * de int referente à quantidades de instruções que pode ser
 * processada durante cada fase (o valor commit é referente à
 * largura de despacho se possuir ROB - obrigatório).
 *
 */
struct FUNCTIONAL_UNITS {
    std::vector<FU> memory_access;
    std::vector<FU> int_basic_alu;
    std::vector<FU> int_mult_div_alu;
    std::vector<FU> float_basic_alu;
    std::vector<FU> float_mult_div_alu;
    int             wr{1};
    int             commit{0};
};

// ─── HELPER ───────────────────────────────────────────────────────

/**
 * @brief Identifica dentro do banco de registradores (no CDB) o
 * registrador alvo.
 *
 * @param const CDB& cdb - Banco de registradores.
 * @param const Register& reg - Registrador alvo.
 *
 * @return Register& - Referência ao registrador dentro do CDB (esse
 * não pode ser "const", portanto).
 */
Register& GetReg( // Não é "static" para não forçar uma reimplementação em cada .cpp que o utiliza (igual).
    CDB&            cdb,
    const Register& reg
);

} // namespace processor

#endif
