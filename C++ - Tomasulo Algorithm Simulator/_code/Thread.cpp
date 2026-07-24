/* Thread.cpp */
#include "headers/Thread.h"
#include "headers/Componentes.h"
#include "headers/Instrucao.h"
#include <string>
#include <vector>
#include <algorithm>

// Métodos públicos
// Construtores:
Thread::Thread(
    const std::vector<std::string>& assembly,
    bool                           tem_rob,
    const std::vector<int>&        num_rs,
    const std::vector<int>&        num_ufs,
    int                            largura_despacho,
    int                            capacidade_rob
):
    tem_rob(tem_rob),
    capacidade_rob(tem_rob ? capacidade_rob : 1)
{
    int i{};
    for (const std::string& instr : assembly)
        tabela_de_instrucoes.push_back({Instrucao(i++, instr), 0, 0, {}, {}, 0, 0});
    inicializarComponentes(num_rs, num_ufs, largura_despacho);
}

Thread::Thread(
    const std::vector<int>&         instrucoes_troca,
    const std::vector<std::string>& assembly,
    bool                            tem_rob,
    const std::vector<int>&         num_rs,
    const std::vector<int>&         num_ufs,
    int                             largura_despacho,
    int                             capacidade_rob
):
    tem_rob(tem_rob),
    capacidade_rob(tem_rob ? capacidade_rob : 1),
    instrucoes_troca(instrucoes_troca)
{
    int i{};
    for (const std::string& instr : assembly)
        tabela_de_instrucoes.push_back({Instrucao(i++, instr), 0, 0, {}, {}, 0, 0});
    inicializarComponentes(num_rs, num_ufs, largura_despacho);
}
// Códigos pequenos:
int                      Thread::getPC()           const { return PC; }
int                      Thread::getNumStalls()    const { return num_stalls; }
EstadoThread             Thread::getEstadoThread() const { return estado; }
CDB                      Thread::getCDB()          const { return cdb; }
ReservationStations      Thread::getRS()           const { return rs; }
UnidadesFuncionais       Thread::getUF()           const { return uf; }
std::vector<LinhaTabela> Thread::getTabela()       const { return tabela_de_instrucoes; }
// Códigos grandes:

// ─── INICIALIZAÇÃO ─────────────────────────────────────────────────

void Thread::inicializarComponentes(
    const std::vector<int>& num_rs,
    const std::vector<int>& num_ufs,
    int                     largura_despacho
){
    for(int i{}; i < NUM_REGISTRADORES; i++){
        cdb.R.push_back(Registrador("R" + std::to_string(i)));
        cdb.F.push_back(Registrador("F" + std::to_string(i)));
    }
    std::vector<int> aux;
    if(num_rs.size() >= 6) aux = num_rs;
    else aux = {5,5,5,4,3,2};
    for(int i{}; i < aux[0]; i++) rs.load.push_back(RS("load" + std::to_string(i)));
    for(int i{}; i < aux[1]; i++) rs.store.push_back(RS("store" + std::to_string(i)));
    for(int i{}; i < aux[2]; i++) rs.int_basico.push_back(RS("int_basico" + std::to_string(i)));
    for(int i{}; i < aux[3]; i++) rs.int_mult_div.push_back(RS("int_mult_div" + std::to_string(i)));
    for(int i{}; i < aux[4]; i++) rs.float_basico.push_back(RS("float_basico" + std::to_string(i)));
    for(int i{}; i < aux[5]; i++) rs.float_mult_div.push_back(RS("float_mult_div" + std::to_string(i)));
    if(num_ufs.size() >= 6) aux = num_ufs;
    else aux = {1,1,1,1,1,2};
    for(int i{}; i < aux[0]; i++) uf.acessar_memoria.push_back(UF{});
    for(int i{}; i < aux[1]; i++) uf.ula_int_basico.push_back(UF{});
    for(int i{}; i < aux[2]; i++) uf.ula_int_mult_div.push_back(UF{});
    for(int i{}; i < aux[3]; i++) uf.ula_float_basico.push_back(UF{});
    for(int i{}; i < aux[4]; i++) uf.ula_float_mult_div.push_back(UF{});
    uf.wr     = aux[5];
    uf.commit = largura_despacho;
}

void Thread::definirLatenciaEspecifica(
    int posicao,
    int latencia_EX,
    int latencia_MEM
){
    if (posicao < 0 || static_cast<size_t>(posicao) >= tabela_de_instrucoes.size()) return;
    tabela_de_instrucoes[posicao].instrucao.setLatenciaEX(latencia_EX);
    if (latencia_MEM > 0)
        tabela_de_instrucoes[posicao].instrucao.setLatenciaMEM(latencia_MEM);
}

// ─── ESTÁGIO ISSUE ────────────────────────────────────────────────

bool Thread::Issue(int ciclo) {
    if (estado == EstadoThread::BLOQUEADA) return false;
    if (PC >= static_cast<int>(tabela_de_instrucoes.size())) return false;
    if (rob.size() >= static_cast<size_t>(capacidade_rob)) return false;

    Instrucao& instrucao = tabela_de_instrucoes[PC].instrucao;
    TipoInstrucao tipo   = instrucao.getTipoInstrucao();
    if (tipo == TipoInstrucao::NAO_EXISTE) { PC++; return false; }

    std::vector<RS>* grupo = nullptr;
    switch (tipo) {
        case TipoInstrucao::LOAD:         grupo = &rs.load;          break;
        case TipoInstrucao::STORE:        grupo = &rs.store;         break;
        case TipoInstrucao::FLOAT_BASICO: grupo = &rs.float_basico;  break;
        case TipoInstrucao::INT_MUL:
        case TipoInstrucao::INT_DIV:      grupo = &rs.int_mult_div;  break;
        case TipoInstrucao::FLOAT_MUL:
        case TipoInstrucao::FLOAT_DIV:    grupo = &rs.float_mult_div;break;
        default:                          grupo = &rs.int_basico;    break;
    }

    for (RS& r : *grupo) {
        if (r.addIssue(instrucao, cdb, ciclo)) {
            tabela_de_instrucoes[PC].ciclo_issue = ciclo;
            tabela_de_instrucoes[PC].posicao_PC  = PC;
            if(tem_rob) rob.push_back(instrucao);
            PC++;
            if (tipo == TipoInstrucao::BRANCH && !tem_rob)
                pc_branch_nao_resolvido = PC - 1;
            return true;
        }
    }
    return false;
}

// ─── ESTÁGIO EX/MEM ───────────────────────────────────────────────

bool Thread::ExMem(int ciclo) {
    if(static_cast<size_t>(num_instrucoes_commitadas) == tabela_de_instrucoes.size() ||
       (!tem_rob && static_cast<size_t>(num_instrucoes_finalizadas) == tabela_de_instrucoes.size()))
       return true;
    if (estado == EstadoThread::ESPERA)
        estado = EstadoThread::LIBERADA;
    if(static_cast<size_t>(num_instrucoes_finalizadas) != tabela_de_instrucoes.size())
        iniciarFaseExOuMem(ciclo);
    return false;
}

void Thread::iniciarFaseExOuMem(int ciclo) {
    std::vector<RS*> candidatas;
    coletarCandidatasParaAvancar(candidatas);
    ordenarCandidatasPorPC(candidatas);

    for (RS* rp : candidatas) {
        RS& r = *rp;
        tentarAvancarRS(r, ciclo);
    }
}

void Thread::coletarCandidatasParaAvancar(std::vector<RS*>& candidatas) {
    auto coletar = [&](std::vector<RS>& grupo) {
        for (RS& r : grupo) {
            if (!r.getBusy()) continue;
            int inst_pc = r.getInstrucaoAtual().getPC();
            if (pc_branch_nao_resolvido >= 0 && inst_pc > pc_branch_nao_resolvido) continue;
            TipoInstrucao tipo = r.getInstrucaoAtual().getTipoInstrucao();
            if (tipo == TipoInstrucao::STORE && tem_rob && r.getFaseInstrucao() == FaseInstrucao::MEM)
                continue;
            candidatas.push_back(&r);
        }
    };
    coletar(rs.load);      coletar(rs.store);
    coletar(rs.int_basico); coletar(rs.int_mult_div);
    coletar(rs.float_basico); coletar(rs.float_mult_div);
}

void Thread::ordenarCandidatasPorPC(std::vector<RS*>& candidatas) const {
    std::sort(candidatas.begin(), candidatas.end(), [](RS* a, RS* b) {
        return a->getInstrucaoAtual().getPC() < b->getInstrucaoAtual().getPC();
    });
}

void Thread::tentarAvancarRS(RS& r, int ciclo) {
    if (r.atualizarDependencias(cdb, uf, ciclo)) {
        int pc = r.getInstrucaoAtual().getPC();
        if (r.getFaseInstrucao() == FaseInstrucao::EX)
            tabela_de_instrucoes[pc].ciclo_EX.push_back(ciclo);
        else if (r.getFaseInstrucao() == FaseInstrucao::MEM)
            tabela_de_instrucoes[pc].ciclo_MEM.push_back(ciclo);
        if (pc == pc_branch_nao_resolvido && r.getFaseInstrucao() == FaseInstrucao::EX)
            pc_branch_nao_resolvido = -1;
    }
}

// ─── ESTÁGIO WR (WRITE RESULT) ──────────────────────────────────

void Thread::Wr(int ciclo) {
    for (int pc : buffer_WB_pendente)
        buffer_WB.push_back(pc);
    buffer_WB_pendente.clear();
    realizarWriteResult(ciclo);
    detectarTransicoesDeFase(ciclo);
    if (!tem_rob) {
        for (int i : buffer_WB_pendente) {
            if (tabela_de_instrucoes[i].instrucao.getTipoInstrucao() == TipoInstrucao::BRANCH)
                estado = EstadoThread::ESPERA;
        }
    }
}

void Thread::realizarWriteResult(int ciclo) {
    std::sort(buffer_WB.begin(), buffer_WB.end());

    int escritas{};
    while (!buffer_WB.empty() && escritas < uf.wr) {
        int pc = buffer_WB.front();
        TipoInstrucao auxTipo{tabela_de_instrucoes[pc].instrucao.getTipoInstrucao()};
        bool store_com_rob = (auxTipo == TipoInstrucao::STORE && tem_rob);

        if (store_com_rob) {
            writeBackStoreComROB(pc, ciclo);
            continue;
        }

        writeBackNormal(pc, ciclo);
        buffer_WB.erase(buffer_WB.begin());
        num_instrucoes_finalizadas++;
        if(auxTipo != TipoInstrucao::BRANCH) escritas++;
    }
}

void Thread::writeBackStoreComROB(int pc, int ciclo) {
    for (RS& r : rs.store)
        if (r.getBusy() && r.getInstrucaoAtual().getPC() == pc)
            r.liberar(ciclo);
    buffer_WB.erase(buffer_WB.begin());
    num_instrucoes_finalizadas++;
}

void Thread::writeBackNormal(int pc, int ciclo) {
    if(tabela_de_instrucoes[pc].instrucao.getTipoInstrucao() != TipoInstrucao::STORE &&
       tabela_de_instrucoes[pc].instrucao.getTipoInstrucao() != TipoInstrucao::BRANCH)
        tabela_de_instrucoes[pc].ciclo_WR = ciclo;

    Registrador dest = tabela_de_instrucoes[pc].instrucao.getRegDestino();
    broadcastCDB(pc, dest, ciclo);

    TipoInstrucao t{tabela_de_instrucoes[pc].instrucao.getTipoInstrucao()};
    if(t == TipoInstrucao::LOAD)
        liberarRSPorRegistrador(rs.load, dest, ciclo);
    else if(t == TipoInstrucao::STORE)
        liberarRSPorPC(rs.store, pc, ciclo);
    else if(t == TipoInstrucao::BRANCH)
        liberarRSPorPC(rs.int_basico, pc, ciclo);
    else if(t == TipoInstrucao::FLOAT_BASICO)
        liberarRSPorRegistrador(rs.float_basico, dest, ciclo);
    else if(t == TipoInstrucao::INT_MUL || t == TipoInstrucao::INT_DIV)
        liberarRSPorRegistrador(rs.int_mult_div, dest, ciclo);
    else if(t == TipoInstrucao::FLOAT_MUL || t == TipoInstrucao::FLOAT_DIV)
        liberarRSPorRegistrador(rs.float_mult_div, dest, ciclo);
    else
        liberarRSPorRegistrador(rs.int_basico, dest, ciclo);
}

void Thread::broadcastCDB(int pc, const Registrador& dest, int ciclo) {
    auto propagarParaTodasRS = [&](const std::string& rs_id) {
        auto resolver = [&](std::vector<RS>& grp) {
            for (RS& dep : grp)
                if (dep.getBusy()) dep.resolverDependencia(rs_id, dest);
        };
        resolver(rs.load);    resolver(rs.store);
        resolver(rs.int_basico);    resolver(rs.int_mult_div);
        resolver(rs.float_basico);  resolver(rs.float_mult_div);
    };

    auto buscarEPropagar = [&](std::vector<RS>& grupo) {
        for (RS& r : grupo) {
            if (!r.getBusy() || r.getFaseInstrucao() != FaseInstrucao::WB) continue;
            if (r.getInstrucaoAtual().getPC() != pc) continue;
            if (dest.getTipo() == 'Z' || dest.getId() < 0) continue;
            std::string rs_id = r.getId();
            int ciclo_inicio = -1;
            if (dest.getTipo() == 'F') {
                ciclo_inicio = cdb.F[dest.getId()].getCicloInicioRS(rs_id);
                cdb.F[dest.getId()].desalocarRS(rs_id, ciclo_inicio, ciclo);
            } else if (dest.getTipo() == 'R') {
                ciclo_inicio = cdb.R[dest.getId()].getCicloInicioRS(rs_id);
                cdb.R[dest.getId()].desalocarRS(rs_id, ciclo_inicio, ciclo);
            }
            propagarParaTodasRS(rs_id);
        }
    };
    buscarEPropagar(rs.load);          buscarEPropagar(rs.store);
    buscarEPropagar(rs.int_basico);    buscarEPropagar(rs.int_mult_div);
    buscarEPropagar(rs.float_basico);  buscarEPropagar(rs.float_mult_div);
}

void Thread::detectarTransicoesDeFase(int ciclo) {
    std::vector<Evento> eventos;
    coletarEventosDeTransicao(eventos, ciclo);

    std::sort(eventos.begin(), eventos.end(), [](const Evento& a, const Evento& b) {
        return a.pc < b.pc;
    });

    for (const Evento& e : eventos)
        processarTransicao(e, ciclo);
}

void Thread::coletarEventosDeTransicao(std::vector<Evento>& eventos, int ciclo) {
    auto coletar = [&](std::vector<RS>& grupo) {
        for (RS& r : grupo) {
            if (!r.getBusy()) continue;
            FaseInstrucao fase_antes = r.getFaseInstrucao();
            if (r.atualizaContagem(uf, ciclo)) {
                eventos.push_back({
                    r.getInstrucaoAtual().getPC(),
                    fase_antes,
                    r.getFaseInstrucao(),
                    r.getInstrucaoAtual().getTipoInstrucao()
                });
            }
        }
    };
    coletar(rs.load);      coletar(rs.store);
    coletar(rs.int_basico); coletar(rs.int_mult_div);
    coletar(rs.float_basico); coletar(rs.float_mult_div);
}

void Thread::processarTransicao(const Evento& e, int ciclo) {
    int pc             = e.pc;
    bool tem_mem       = (e.tipo == TipoInstrucao::LOAD || e.tipo == TipoInstrucao::STORE);
    bool store_com_rob = (e.tipo == TipoInstrucao::STORE && tem_rob);

    if (e.fase_antes == FaseInstrucao::EX && e.fase_depois == FaseInstrucao::MEM) {
        tabela_de_instrucoes[pc].ciclo_EX.push_back(ciclo);
        if (store_com_rob)
            buffer_WB_pendente.push_back(pc);
    } else if (e.fase_depois == FaseInstrucao::WB) {
        if (tem_mem && !store_com_rob)
            tabela_de_instrucoes[pc].ciclo_MEM.push_back(ciclo);
        else if (!tem_mem)
            tabela_de_instrucoes[pc].ciclo_EX.push_back(ciclo);
        buffer_WB_pendente.push_back(pc);
    }
}

// ─── ESTÁGIO COMMIT ──────────────────────────────────────────────

void Thread::Commit(int ciclo) {
    if (!tem_rob) return;

    int escritas{};
    while (!rob.empty() && escritas < uf.commit){
        LinhaTabela& linha{tabela_de_instrucoes[ponteiro_commit]};
        TipoInstrucao tipo = linha.instrucao.getTipoInstrucao();
        bool store_com_rob = (tipo == TipoInstrucao::STORE && tem_rob);
        bool pronto = false;

        if (store_com_rob) {
            if (linha.ciclo_MEM.empty()) {
                linha.ciclo_MEM.push_back(ciclo);
            }
            if (linha.ciclo_MEM.size() == 1) {
                int fim_mem = linha.ciclo_MEM[0] + linha.instrucao.getLatenciaMEM() - 1;
                if (ciclo >= fim_mem) {
                    linha.ciclo_MEM.pop_back();
                    pronto = true;
                }
            }
        } else if (tipo == TipoInstrucao::BRANCH) {
            pronto = (linha.ciclo_EX.size() == 2);
        } else {
            pronto = (linha.ciclo_WR != 0 && linha.ciclo_WR < ciclo);
        }

        if (pronto) {
            linha.ciclo_commit = ciclo;
            num_instrucoes_commitadas++;
            escritas++;
            ponteiro_commit++;
            rob.erase(rob.begin());
            if (tipo == TipoInstrucao::BRANCH && !(tem_previsor && tem_rob)) break;
        }
        else break;
    }
}

// ─── UTILITÁRIOS ─────────────────────────────────────────────────

void Thread::liberarRSPorRegistrador(
    std::vector<RS>& rs,
    Registrador      reg_destino,
    int              ciclo
){
    for(RS& r : rs){
        if(!r.getBusy()) continue;
        Registrador aux{r.getInstrucaoAtual().getRegDestino()};
        if(r.getFaseInstrucao() == FaseInstrucao::WB &&
            aux.getTipo() == reg_destino.getTipo() &&
            aux.getId() == reg_destino.getId())
            r.liberar(ciclo);
    }
}

void Thread::liberarRSPorPC(
    std::vector<RS>& grupo,
    int              pc,
    int              ciclo
){
    for (RS& r : grupo) {
        if (r.getBusy() && r.getFaseInstrucao() == FaseInstrucao::WB
            && r.getInstrucaoAtual().getPC() == pc)
            r.liberar(ciclo);
    }
}
