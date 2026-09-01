/* headers/Architecture.h */

/**
 * @file Architecture.h
 *
 * @brief Módulo responsável pela definição virtual (genérica) de uma
 * instrução e mecanismos para verificação da coerência dessa.
 *
 * @details Esse arquivo também inclui as fases de uma instrução
 * dentro do pipeline (clássico e tomasulo).
 */

#ifndef ACHITECTURE_H    // Include guard
#define ACHITECTURE_H
#include "Components.h"
#include <string>
#include <vector>
#include <cstddef>       // para std::size_t
#include <unordered_map>
#include <algorithm>     // para std::find
#include <cctype>        // para std::tolower
#include <cstdlib>       // para std::abort
#include <iostream>      // para std::cerr
#include <tuple>         // para std::tuple

namespace processor {

// ─── HELPERS ──────────────────────────────────────────────────────

/**
 * ATENÇÃO: A função deve ser declarada em cada um dos códigos das
 * subclasses de "Instruction" (versão própria).
 *
 * Gera warning por não ser definido em "Architecture.cpp" e em
 * outros módulos que importam esse header.
 *
 * O warning é ignorado pela diretiva.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/**
 * @brief Cria uma tabela com todos os registradores, flags e
 * mascaras característicos de cada arquitetura e retorna ela pronta.
 *
 * @details Após a primeira criação, ele passa a retornar apenas a
 * tabela (evita realizar a operação a cada chamada).
 *
 * @return static const std::unordered_map<std::string, Register>& -
 * Tabela completa com os registradores da arquitetura.
 */
static const std::unordered_map<std::string, Register>& RegisterTable();
#pragma GCC diagnostic pop

/**
 * ATENÇÃO: O método "MakeRegisterLayout()" não pode ser declarado "static"
 * nesse header para ser implementado em cada um dos programas
 * (o que parece uma ótima ideia para evitar reescrita de código
 * em cada header a princípio).
 *
 * O "InstructionFactory" não chama com base em uma arquitetura
 * específica (o método de diferenciação é se ele está dentro da
 * classe - onde ele é declarado "static", e chamado pelo namespace
 * da sua especificação).
 */

// Não são "static" para não forçar uma reimplementação em cada .cpp (são iguais).

/**
 * @brief Adiciona as referências de uma classe ao layout arquitetural.
 *
 * @details Tem de ser chamado para cada classe diferente da
 * arquitetura.
 *
 * @param REGISTER_LAYOUT& layout - Descrição do banco de registradores.
 * @param const char reg_class - A classe do registrador.
 * @param const int id_base - O id de início a ser considerado
 * @param const int count - A quantidade de registradores a ser
 * adicionada
 * @param const int mask - A mascara que ocupa aquela classe de
 * registradores.
 */
void FillRegisterLayout(
    REGISTER_LAYOUT& layout,
    const char       reg_class,
    const int        id_base,
    const int        count,
    const int        mask = 255
);

/**
 * @brief Verifica se o nome é de um registrador válido diretamente
 * na tabela retornada pela função "RegisterTable()".
 *
 * @details É feito para evitar alguma confusão como por exemplo
 * labels com prefixo de registrador ("$L2", "X99", "R99", etc.).
 *
 * Esse método de verificação impede casos de falso positivo
 * por simplificação de sintaxe, como começar com "$".
 *
 * Esse helper não foi implementado em "Components" pois ele
 * depende da definição do banco de registradores (que é feito pela
 * função "RegisterTable()", desse módulo).
 * )
 *
 * @param const std::string& token - Nome alvo.
 * @param const std::unordered_map<std::string, Register>& table -
 * Tabela do "RegisterTable()".
 *
 * @return true - Encontrou o nome na tabela (é um registrador).
 * @return false - Não encontrou (não é um registrador).
 */
bool IsRegister(
    const std::string&                               token,
    const std::unordered_map<std::string, Register>& table
);

/**
 * @brief Verifica se o OpCode existe (está contido dentro do vetor
 * enviado).
 *
 * @param const std::vector<std::string>& vec - Vetor com as opções
 * de OpCode.
 * @param const std::string& op - OpCode alvo.
 *
 * @return true - O OpCode existe dentro daquele vetor.
 * @return false - Não existe.
 */
bool ContainsOpcode(
    const std::vector<std::string>& vec,
    const std::string&              op
);

// ─── ENUMS ────────────────────────────────────────────────────────

enum class INSTRUCTION_TYPE {
    INVALID,
    LOAD,
    STORE,
    BRANCH,
    INT_BASIC,
    INT_MUL,
    INT_DIV,
    FLOAT_BASIC,
    FLOAT_MUL,
    FLOAT_DIV
};
enum class INSTRUCTION_PHASE_CLASSIC {
    UNUSED,
    IF,    // Instruction Fetch
    ID,    // Instruction Decode
    EX,    // Execute
    MEM,   // Memory Access
    WB     // Write Back
};
enum class INSTRUCTION_PHASE_TOMASULO {
    UNUSED,
    IS,    // Issue
    EX,    // Execute
    MEM,   // Memory Access
    WR,    // Write Results
    COMMIT
};

/**
 * @brief Override de latências associado à posição de uma instrução.
 *
 * @details Os vetores de EX e MEM seguem a ordenação das etapas da
 * instrução.
 */
using LATENCY_OVERRIDE =
    std::tuple<int, std::vector<int>, std::vector<int>>;

// ─── CLASSE ───────────────────────────────────────────────────────

/**
 * @brief Classe genérica de instrução individual do programa.
 *
 * @details Para torná-la funcional, é necessário gerar uma subclasse
 * e definir os detalhes da arquitetura:
 *
 * - Registradores suportados;
 * - Sintaxe;
 * - Flags;
 * - Aliases de registrador;
 * ...
 */
class Instruction {
    public:
        // Elementos static:

        /**
         * @brief As latências da etapa EX são armazenadas em vetores
         * cuja ordenação corresponde à do "enum" "INSTRUCTION_TYPE".
         *
         * @details É "static" para ser compartilhada entre todos os
         * objetos instanciados da classe, mas não são "const" para
         * serem configuráveis (definir um novo valor padrão).
         *
         * Casos únicos de latência ainda podem ser adicionados em
         * objetos individuais.
         */
        static std::vector<int> base_ex_latencies;

        /**
         * @brief Latências da etapa MEM ([0] = load; [1] = store).
         *
         * @details Mesmas características de "base_ex_latencies".
         */
        static std::vector<int> base_mem_latencies;

        // Construtor:
        Instruction(
            const int position = -1
        );

        // Destrutor (importante em se tratando de um vetor de ponteiros compartilhados):
        // - "virtual" para permitir override das suas sub-classes.
        virtual ~Instruction() = default;

        // Getters:
        // Não são "const &" pois tem ganho marginal pequeno (por serem de tipos simples).
        int GetPosition()   const;
        int GetExLatency()  const;
        int GetMemLatency() const;

        INSTRUCTION_TYPE GetInstructionType() const;

        /**
         * @brief Retorna a quantidade de etapas da descrição completa.
         *
         * @return Quantidade de etapas da instrução.
         */
        std::size_t GetStageCount() const;

        /**
         * @brief Retorna o tipo de uma etapa específica.
         *
         * @param stage Índice da etapa na descrição completa.
         *
         * @return Tipo da etapa indicada.
         */
        INSTRUCTION_TYPE GetInstructionType(
            const std::size_t stage
        ) const;

        /**
         * @brief Retorna a latência EX de uma etapa específica.
         *
         * @param stage Índice da etapa na descrição completa.
         *
         * @return Latência EX da etapa indicada.
         */
        int GetExLatency(
            const std::size_t stage
        ) const;

        /**
         * @brief Retorna a latência MEM de uma etapa específica.
         *
         * @param stage Índice da etapa na descrição completa.
         *
         * @return Latência MEM da etapa indicada.
         */
        int GetMemLatency(
            const std::size_t stage
        ) const;

        // "const &" para evitar cópia de dados grandes.

        const std::string& GetInstructionString() const;

        /**
         * @brief Retorna todos os registradores de destino da
         * instrução (principais, aliases, flags, etc.).
         *
         * @return const std::vector<Register>& - Vetor com os
         * registradores.
         */
        const std::vector<Register>& GetDestRegisters() const;

        /**
         * @brief Retorna todos os registradores de responsáveis pela
         * etapa EX da instrução (principais, aliases, flags, etc.).
         *
         * @return const std::vector<Register>& - Vetor com os
         * registradores.
         */
        const std::vector<Register>& GetExSourceRegisters() const;

        /**
         * @brief Retorna todos os registradores de responsáveis pela
         * etapa MEM da instrução (principais, aliases, flags, etc.).
         *
         * @return const std::vector<Register>& - Vetor com os
         * registradores.
         */
        const std::vector<Register>& GetMemSourceRegisters() const;

        /**
         * @brief Retorna as fontes EX de uma etapa específica.
         *
         * @param stage Índice da etapa na descrição completa.
         *
         * @return Fontes EX da etapa indicada.
         */
        const std::vector<Register>& GetExSourceRegisters(
            const std::size_t stage
        ) const;

        /**
         * @brief Retorna as fontes MEM de uma etapa específica.
         *
         * @param stage Índice da etapa na descrição completa.
         *
         * @return Fontes MEM da etapa indicada.
         */
        const std::vector<Register>& GetMemSourceRegisters(
            const std::size_t stage
        ) const;

        /**
         * @brief Retorna a sequência completa de tipos da instrução.
         *
         * @return Vetor ordenado com todas as etapas da instrução.
         */
        const std::vector<INSTRUCTION_TYPE>& GetInstructionTypes() const;

        /**
         * @brief Retorna as latências EX de todas as etapas.
         *
         * @return Vetor alinhado com GetInstructionTypes().
         */
        const std::vector<int>& GetExLatencies() const;

        /**
         * @brief Retorna as latências MEM de todas as etapas.
         *
         * @return Vetor alinhado com GetInstructionTypes().
         */
        const std::vector<int>& GetMemLatencies() const;

        /**
         * @brief Retorna as fontes EX separadas por estágio.
         *
         * @return Vetor externo alinhado com GetInstructionTypes().
         */
        const std::vector<std::vector<Register>>& GetAllExSourceRegisters() const;

        /**
         * @brief Retorna as fontes MEM separadas por estágio.
         *
         * @return Vetor externo alinhado com GetInstructionTypes().
         */
        const std::vector<std::vector<Register>>& GetAllMemSourceRegisters() const;

        // Demais métodos:

        /**
         * @brief Configura os dados da instrução a partir da
         * informção extraida da string.
         *
         * @details Wrapper que une funções que executa múltiplas
         * etapas:
         * - Separa a instrução;
         * - Identifica componentes;
         * - Formata a string;
         * ...
         *
         * @param const std::string& str - Instrução.
         */
        void Parse(
            const std::string& str
        );

        /**
         * @brief Sobrescreve atomicamente as latências de todas as etapas.
         *
         * @details Cada vetor deve possuir uma entrada por etapa. Em MEM,
         * zero preserva a latência-base correspondente ao tipo da etapa.
         *
         * @param new_ex_latencies Novas latências de EX.
         * @param new_mem_latencies Novas latências de MEM.
         */
        void SetLatencies(
            const std::vector<int>& new_ex_latencies,
            const std::vector<int>& new_mem_latencies
        );

    protected:
        // Atributos:
        int                   position{-1};
        std::string           instruction_string;

        /**
         * @brief Arquiteturas como X86, ARM, etc. Podem ter
         * múltiplos destinos (aliases, máscaras, flags, etc.).
         */
        std::vector<Register> dest_registers;

        // Método igual para todos:

        /**
         * @brief Adiciona atomicamente uma etapa completa da instrução.
         *
         * @param instruction_type Tipo da nova etapa.
         * @param ex_sources Fontes consumidas durante EX.
         * @param mem_sources Fontes consumidas durante MEM.
         */
        void AddStage(
            const INSTRUCTION_TYPE     instruction_type,
            const std::vector<Register>& ex_sources,
            const std::vector<Register>& mem_sources
        );

        // Métodos virtuais (cada arquitetura deve implementar sua versão):
        virtual std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const = 0;

        /**
         * @brief Valida a instrução e monta todas as suas etapas.
         *
         * @param tokens Componentes extraídos da instrução.
         *
         * @return true para opcode suportado; false para opcode desconhecido.
         */
        virtual bool SetStages(
            const std::vector<std::string>& tokens
        ) = 0;

        virtual void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) = 0;

    private:
        // Uma posição em cada vetor representa a mesma etapa lógica.
        std::vector<INSTRUCTION_TYPE>        instruction_types;
        std::vector<int>                     ex_latencies;
        std::vector<int>                     mem_latencies;
        std::vector<std::vector<Register>>   ex_source_registers;
        std::vector<std::vector<Register>>   mem_source_registers;
        /**
         * @brief Retorna a latência-base de MEM correspondente ao tipo.
         *
         * @return Latência de LOAD/STORE ou zero para os demais tipos.
         */
        static int GetBaseMemLatency(
            const INSTRUCTION_TYPE instruction_type
        );

        /**
         * @brief Confirma que todos os vetores por etapa estão alinhados.
         */
        void ValidateStageVectors() const;

        /**
         * @brief Confirma que o índice pertence à descrição completa.
         *
         * @param stage Índice consultado.
         */
        void ValidateStageIndex(
            const std::size_t stage
        ) const;

        /**
         * @brief Confirma a validade das latências efetivas de cada etapa.
         *
         * @param ex_values Latências efetivas de EX.
         * @param mem_values Latências efetivas de MEM.
         */
        void ValidateLatencies(
            const std::vector<int>& ex_values,
            const std::vector<int>& mem_values
        ) const;

};

} // namespace processor

#endif
