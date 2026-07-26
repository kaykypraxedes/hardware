/* headers/Thread.h */
#ifndef THREAD_H // Include guard
#define THREAD_H
#include <string>
#include <vector>
#include "Instrucao.h"
#include "Componentes.h"
#include "ReservationStations.h"


constexpr int CAPACIDADE_ROB = 30;

// enum:
enum class EstadoThread {
    LIBERADA,
    ESPERA,
    BLOQUEADA
};

// structs:
struct LinhaTabela{
    Instrucao        instrucao;
    int              posicao_PC{-1};
    int              ciclo_issue{-1};
    std::vector<int> ciclo_EX;
    std::vector<int> ciclo_MEM;
    int              ciclo_WR{-1};
    int              ciclo_commit{-1};
};
struct Evento {
    int           pc;
    FaseInstrucao fase_antes;
    FaseInstrucao fase_depois;
    TipoInstrucao tipo;
};

// classe:
class Thread {
    public:
        // Construtores
        Thread(
            const std::vector<std::string>&,
            bool,
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            int = 1,
            int = CAPACIDADE_ROB
        );

        Thread(
            const std::vector<int>&,
            const std::vector<std::string>&,
            bool,
            const std::vector<int>& = {},
            const std::vector<int>& = {},
            int = 1,
            int = CAPACIDADE_ROB
        );
        // Getters
        int                             getPC()           const;
        int                             getNumStalls()    const;
        EstadoThread                    getEstadoThread() const;
        // & const para evitar cópia (para tipos básicos e enums o ganho é marginal)
        const CDB&                      getCDB()          const;
        const ReservationStations&      getRS()           const;
        const UnidadesFuncionais&       getUF()           const;
        const std::vector<LinhaTabela>& getTabela()       const;
        // Métodos públicos (estágios da pipeline)
        bool Issue(int);
        bool ExMem(int);
        void Wr(int);
        void Commit(int);
        void definirLatenciaEspecifica(int, int, int=0);
    private:
        // Atributos
        // Elementos auxiliares dentro da Thread:
        int                      num_instrucoes_finalizadas{};
        int                      num_instrucoes_commitadas{};
        int                      num_stalls{};
        int                      ponteiro_commit{};
        int                      pc_branch_nao_resolvido{-1};
        EstadoThread             estado{EstadoThread::LIBERADA};
        // Elementos funcionais da Thread:
        bool                     tem_rob{false};
        int                      capacidade_rob{1};
        int                      PC{};
        bool                     tem_previsor{false};
        CDB                      cdb;
        ReservationStations      rs;
        UnidadesFuncionais       uf;
        std::vector<int>         buffer_WB;
        std::vector<int>         buffer_WB_pendente;
        std::vector<int>         instrucoes_troca;
        std::vector<Instrucao>   rob;
        std::vector<LinhaTabela> tabela_de_instrucoes;

        // ─── MÉTODOS PRIVADOS (organizados por estágio) ───

        // Inicialização
        void inicializarComponentes(
            const std::vector<int>&,
            const std::vector<int>&,
            int
        );

        // EX/MEM
        void iniciarFaseExOuMem(int);
        void coletarCandidatasParaAvancar(std::vector<RS*>&);
        void tentarAvancarRS(RS&, int);

        // WR (Write Result)
        void realizarWriteResult(int);
        void ordenarBufferWB();
        int  proximoWB() const;
        void removerWB();
        void adicionarWB(int);
        void adicionarWBPendente(int);
        void flushBufferWBPendente();
        void writeBackStoreComROB(int, int);
        void writeBackNormal(int, int);
        void broadcastCDB(int, const Registrador&, int);
        void detectarTransicoesDeFase(int);
        void coletarEventosDeTransicao(std::vector<Evento>&, int);
        void ordenarEventosPorPC(std::vector<Evento>&) const;
        void processarTransicao(const Evento&, int);
        void registrarIssue(int, int);
        void adicionarCicloEX(int, int);
        void adicionarCicloMEM(int, int);
        void definirWR(int, int);

        // COMMIT
        void liberarRSPorRegistrador(
            std::vector<RS>&,
            Registrador,
            int
        );

        // Utilitários
        void ordenarCandidatasPorPC(std::vector<RS*>&) const;
        void liberarRSPorPC(std::vector<RS>&, int, int);
};

#endif
