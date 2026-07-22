/* Processador.cpp */
#include "headers/Processador.h"
#include "headers/Thread.h"
#include <string>
#include <vector>

// Métodos públicos
// Construtor:
Processador::Processador(
    int                           num_threads,
    int                           largura_de_despacho,
    bool                          tem_previsor,
    TipoProcessador               tipo,
    ModeloMultithreading          modelo,
    const std::vector<std::string>& Assembly,
    const std::vector<int>&        num_rs,
    std::vector<int>               num_ufs,
    const std::vector<int>&        instrucoes_troca
):
    largura_de_despacho(largura_de_despacho),
    tem_previsor       (tem_previsor),
    tipo               (tipo),
    modelo_MT          (modelo)
{
    bool tem_rob{tipo == TipoProcessador::TOMASULO_COM_ROB};
    if (instrucoes_troca.empty())
        iniciarThreads(Assembly, tem_rob, num_threads, num_rs, num_ufs);
    else
        iniciarThreads(Assembly, tem_rob, num_threads, num_rs, num_ufs, instrucoes_troca);
}

// Códigos pequenos:
TipoProcessador Processador::getTipo() const{ return tipo; }
Thread Processador::getThread(int i)   const{ return threads[i]; }
std::vector<LinhaTabela> Processador::getTabelaThread(int i) const { return threads[i].getTabela(); }

bool Processador::executarCiclo() {
    if(executarExMemWr()) return true;
    // Reservation Stations são liberadas no mesmo ciclo em que a instrução deixa a estação, podendo ser reutilizadas imediatamente por novas instruções emitidas no mesmo ciclo
    executarIssue();
    ciclo_atual++;
    return false;
}

// ExMem antes de Wr: a contagem é decrementada depois de registrar o início.
bool Processador::executarExMemWr() {
    bool executou_todos{true};
    for (Thread& t : threads) {
        bool exmem_done = t.ExMem(ciclo_atual);
        t.Wr(ciclo_atual);
        t.Commit(ciclo_atual); // commit após WR: ciclo_WR já preenchido
        executou_todos &= exmem_done;
    }
    return executou_todos;
}

void Processador::executarIssue() {
    int despachadas   = 0;
    int voltas        = 0;
    int total_threads = static_cast<int>(threads.size());

    while (despachadas < largura_de_despacho) {
        if (voltas >= total_threads) break; // percorreu todas sem conseguir despachar

        bool ok = threads[ponteiro_da_thread].Issue(ciclo_atual);
        if (ok) {
            despachadas++;
            if(threads[ponteiro_da_thread].getTabela()[threads[ponteiro_da_thread].getPC() - 1].instrucao.getTipoInstrucao() ==
                TipoInstrucao::BRANCH && modelo_MT != ModeloMultithreading::SMT
                && !tem_previsor) // Instrução é um branch
                break;
            // SMT: rotaciona após issue bem-sucedido
            // (garante alternância entre threads a cada despacho)
            if (modelo_MT == ModeloMultithreading::SMT) avancarPonteiroRoundRobin();
        } else {
            voltas++;
            if (modelo_MT == ModeloMultithreading::SMT || despachadas == 0)
                avancarPonteiroRoundRobin();
            else
                break;
        }
    }
}

void Processador::avancarPonteiroRoundRobin() {
    ponteiro_da_thread = (ponteiro_da_thread + 1) % static_cast<int>(threads.size());
}

void Processador::iniciarThreads(
    const std::vector<std::string>& Assembly,
    bool                           tem_rob,
    int                            num_threads,
    const std::vector<int>&        num_rs,
    const std::vector<int>&        num_ufs,
    const std::vector<int>&        instrucoes_troca
){
    for (int i = 0; i < num_threads; i++) {
        if (modelo_MT == ModeloMultithreading::GRANULACAO_GROSSA)
            threads.push_back(Thread(instrucoes_troca, Assembly, tem_rob, num_rs, num_ufs, largura_de_despacho));
        else
            threads.push_back(Thread(Assembly, tem_rob, num_rs, num_ufs, largura_de_despacho));
    }
}
