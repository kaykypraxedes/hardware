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
#include <unordered_map>
#include <algorithm>     // para std::find
#include <cctype>        // para std::tolower
#include <cstdlib>       // para std::abort
#include <iostream>      // para std::cerr

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
 * ATENÇÃO: O método "MakeCDB()" não pode ser declarado "static"
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
 * @brief Cria os registradores de uma classe dentro do banco.
 *
 * @details Tem de ser chamado para cada classe diferente da
 * arquitetura.
 *
 * @param CDB& cdb - Onde está o banco de registradores.
 * @param const char reg_class - A classe do registrador.
 * @param const int id_base - O id de início a ser considerado
 * @param const int count - A quantidade de registradores a ser
 * adicionada
 * @param const int mask - A mascara que ocupa aquela classe de
 * registradores.
 */
void FillCDB(
    CDB&       cdb,
    const char reg_class,
    const int  id_base,
    const int  count,
    const int  mask = 255
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
         * @brief Seta uma latência específica de MEM para a
         * instrução.
         *
         * @details Útil para simular um cache miss, por exemplo.
         *
         * @param const int latency - Latência
         */
        void SetMemLatency(
            const int latency
        );

        /**
         * @brief Seta uma latência específica de EX para a
         * instrução.
         *
         * @param const int latency - Latência
         */
        void SetExLatency(
            const int latency
        );

    protected:
        // Atributos:
        int                   position{-1};
        std::string           instruction_string;
        int                   ex_latency{};
        int                   mem_latency{};
        INSTRUCTION_TYPE      type{INSTRUCTION_TYPE::INVALID};

        /**
         * @brief Arquiteturas como X86, ARM, etc. Podem ter
         * múltiplos destinos (aliases, máscaras, flags, etc.).
         */
        std::vector<Register> dest_registers;
        std::vector<Register> ex_source_registers;  // Ajudam a identificar quais registradores travam quais partes do pipeline.
        std::vector<Register> mem_source_registers;

        // Método igual para todos:

        /**
         * @brief Define as latências com base no tipo de instrução,
         * definidas pelos vetores de latência.
         */
        void SetLatencies();

        // Métodos virtuais (cada arquitetura deve implementar sua versão):
        virtual std::vector<std::string> SplitInstruction(
            const std::string& str
        ) const = 0;
        virtual bool IdentifyType(
            const std::vector<std::string>& tokens
        ) = 0;
        virtual void ValidateInstruction(
            const std::vector<std::string>& tokens
        ) = 0;
        virtual void NormalizeInstruction(
            std::vector<std::string>& tokens
        ) = 0;
        virtual void SetAttributes(
            const std::vector<std::string>& tokens
        ) = 0;
};

} // namespace processor

#endif
