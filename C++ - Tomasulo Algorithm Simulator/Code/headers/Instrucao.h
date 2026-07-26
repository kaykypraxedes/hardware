/* headers/Instrucao.h */
#ifndef INSTRUCAO_H // Include guard
#define INSTRUCAO_H
#include <string>
#include <vector>
#include "Componentes.h"

//enums:
enum class TipoInstrucao {
    NAO_EXISTE,
    LOAD,
    STORE,
    BRANCH,
    INT_BASICO,
    INT_MUL,
    INT_DIV,
    FLOAT_BASICO,
    FLOAT_MUL,
    FLOAT_DIV
};
enum class FaseInstrucao {
    ISSUE,
    EX,
    MEM,
    WB,
    COMMIT
};

// classe:
class Instrucao {
    public:
        // Elementos static
        static std::vector<int> latencias_basicas_EX;
        static std::vector<int> latencias_basicas_MEM;
        // Construtores
        Instrucao() :
            PC(-1),
            latencia_EX(0),
            latencia_MEM(0),
            tipo(TipoInstrucao::NAO_EXISTE) {}
        Instrucao(
            int,
            std::string
        );
        // Getters e setters
        int           getPC()              const;
        int           getLatenciaEX()      const;
        int           getLatenciaMEM()     const;
        TipoInstrucao getTipoInstrucao()   const;
        Registrador   getRegDestino()      const;
        Registrador   getJ()               const;
        Registrador   getK()               const;
        // & const para evitar cópia (tipos pequenos teriam um ganho marginal)
        const std::string& getInstrucaoString() const;

        void setLatenciaMEM(
            int
        );

        void setLatenciaEX(
            int
        );
    private:
        // Atributos (instrucao_em_string DEVE vir antes de PC para que o
        // initializer list a inicialize antes de splitInstrucao() ser chamado)
        std::string   instrucao_em_string;
        int           PC{};
        int           latencia_EX;
        int           latencia_MEM{};
        TipoInstrucao tipo;
        Registrador   reg_destino;
        Registrador   reg_J;
        Registrador   reg_K;
        // Métodos privados
        void                     stringParaInstrucao();
        std::vector<std::string> splitInstrucao();
        void                     defineLatencias();

        void identificaTipo(
            const std::string&
        );

        void defineAtributos(
            std::vector<std::string>&
        );
};
#endif
