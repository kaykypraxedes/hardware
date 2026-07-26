/* ReservationStations.cpp */
#include "headers/ReservationStations.h"
#include "headers/Componentes.h"
#include "headers/Instrucao.h"
#include <string>

static bool mesmoRegistrador(const Registrador& a, const Registrador& b) {
    return a.getTipo() != 'Z' && a.getTipo() == b.getTipo() && a.getId() == b.getId();
}

// Métodos públicos
// Construtor:
RS::RS(std::string id) : id(id){}

bool                     RS::getBusy()               const { return busy; }
int                      RS::getContagemRegressiva() const { return contagem_regressiva_alocacao; }
int                      RS::getPosicaoUF()          const { return posicao_UF; }
FaseInstrucao            RS::getFaseInstrucao()      const { return fase; }
// & const para evitar cópia (tipos pequenos teriam um ganho marginal)
const Instrucao&                RS::getInstrucaoAtual()     const { return instrucao_atual; }
const std::string&              RS::getId()                 const { return id; }
const std::string&              RS::getQj()                 const { return Qj.first; }
const std::string&              RS::getQk()                 const { return Qk.first; }
const std::vector<int>&         RS::getTempos()             const { return tempos_alocacao; }
const std::vector<std::string>& RS::getInstrucoes()         const { return instrucoes_alocacao; }

// Lê Qj/Qk do CDB ANTES de marcar o destino para não se auto-bloquear.
// Qj/Qk são agora std::pair<std::string,int> = {rs_id, ciclo_inicio_no_cdb}.
// Par {"", -1} significa operando disponível (sem dependência pendente).
bool RS::addIssue(
    Instrucao& instrucao,
    CDB&       cdb,
    int        ciclo
){
    if (busy) return false;
    busy            = true;
    instrucao_atual = instrucao;
    fase            = FaseInstrucao::ISSUE;
    contagem_regressiva_alocacao = -1;
    posicao_UF                   = -1;
    Qj = Qk = {"", -1};
    Vj = Vk = Registrador{};
    destino_pendente_no_cdb = false;
    instrucoes_alocacao.push_back(instrucao.getInstrucaoString());
    tempos_alocacao.push_back(ciclo);

    Registrador dest = instrucao.getRegDestino();
    bool dest_valido = (dest.getTipo() != 'Z' && dest.getId() >= 0);

    // Lê dependências ANTES de marcar o destino para não se auto-bloquear (WAR).
    Registrador regJ = instrucao.getJ();
    Registrador regK = instrucao.getK();

    if (regJ.getTipo() != 'Z' && regJ.getId() >= 0 && regJ.getId() < NUM_REGISTRADORES) {
        Registrador& regCDBj = (regJ.getTipo() == 'F')
            ? cdb.F[regJ.getId()] : cdb.R[regJ.getId()];
        std::string tag = regCDBj.getRSatual();
        if (tag.empty()) {
            Vj = regJ; // operando disponível (inclui WAR sem produtor pendente)
        } else if (dest_valido && mesmoRegistrador(regJ, dest) && tag == id) {
            Vj = regJ; // WAR puro: o único produtor pendente é esta própria instrução
        } else {
            Qj = {tag, regCDBj.getCicloInicioRS(tag)};
        }
    }
    if (regK.getTipo() != 'Z' && regK.getId() >= 0 && regK.getId() < NUM_REGISTRADORES) {
        Registrador& regCDBk = (regK.getTipo() == 'F')
            ? cdb.F[regK.getId()] : cdb.R[regK.getId()];
        std::string tag = regCDBk.getRSatual();
        if (tag.empty()) {
            Vk = regK;
        } else if (dest_valido && mesmoRegistrador(regK, dest) && tag == id) {
            Vk = regK; // WAR puro
        } else {
            Qk = {tag, regCDBk.getCicloInicioRS(tag)};
        }
    }

    // Marca o destino no CDB agora, sempre.
    if (dest_valido && dest.getId() < NUM_REGISTRADORES) {
        if      (dest.getTipo() == 'F') cdb.F[dest.getId()].alocarRS(id, ciclo);
        else if (dest.getTipo() == 'R') cdb.R[dest.getId()].alocarRS(id, ciclo);
    }
    return true;
}

// Resolve Qj/Qk consultando o CDB via dependenciaResolvida(rs_id, ciclo_inicio).
// Quando prontos, aloca UF e inicia contagem.
bool RS::atualizarDependencias(
    CDB&                cdb,
    UnidadesFuncionais& uf,
    int                 ciclo
){
    if (!busy || contagem_regressiva_alocacao != -1) return false;

    Registrador regJ = instrucao_atual.getJ();
    Registrador regK = instrucao_atual.getK();

    // Resolver Vj / Qj
    if (Vj.getTipo() == 'Z' && !Qj.first.empty()
        && regJ.getId() >= 0 && regJ.getId() < NUM_REGISTRADORES) {
        Registrador& regCDB = (regJ.getTipo() == 'F')
            ? cdb.F[regJ.getId()] : cdb.R[regJ.getId()];
        if (regCDB.dependenciaResolvida(Qj.first, Qj.second)) {
            Vj = regJ;
            Qj = {"", -1};
        }
    }
    // Resolver Vk / Qk
    if (Vk.getTipo() == 'Z' && !Qk.first.empty()
        && regK.getId() >= 0 && regK.getId() < NUM_REGISTRADORES) {
        Registrador& regCDB = (regK.getTipo() == 'F')
            ? cdb.F[regK.getId()] : cdb.R[regK.getId()];
        if (regCDB.dependenciaResolvida(Qk.first, Qk.second)) {
            Vk = regK;
            Qk = {"", -1};
        }
    }

    TipoInstrucao tipo = instrucao_atual.getTipoInstrucao();
    bool load_store = (tipo == TipoInstrucao::LOAD || tipo == TipoInstrucao::STORE);

    if (load_store) {
        if (fase == FaseInstrucao::ISSUE && Qk.first.empty()) {
            posicao_UF = procuraUFlivre(uf, ciclo, FaseInstrucao::EX);
            if (posicao_UF == -1) return false;
            contagem_regressiva_alocacao = instrucao_atual.getLatenciaEX();
            fase = FaseInstrucao::EX;
            return true;
        }
        if (fase == FaseInstrucao::MEM && contagem_regressiva_alocacao == -1
            && ((tipo == TipoInstrucao::STORE && Qj.first.empty()) || tipo == TipoInstrucao::LOAD)) {
            posicao_UF = procuraUFlivre(uf, ciclo, FaseInstrucao::MEM);
            if (posicao_UF == -1) return false;
            contagem_regressiva_alocacao = instrucao_atual.getLatenciaMEM();
            return true;
        }
        return false;
    }

    // Instrução comum: precisa de Qj e Qk resolvidos
    if (Qj.first.empty() && Qk.first.empty()) {
        posicao_UF = procuraUFlivre(uf, ciclo, FaseInstrucao::EX);
        if (posicao_UF == -1) return false;
        contagem_regressiva_alocacao = instrucao_atual.getLatenciaEX();
        fase = FaseInstrucao::EX;
        if (destino_pendente_no_cdb) {
            Registrador dest = instrucao_atual.getRegDestino();
            if (dest.getId() >= 0 && dest.getId() < NUM_REGISTRADORES) {
                if (dest.getTipo() == 'F') cdb.F[dest.getId()].alocarRS(id, ciclo);
                else if (dest.getTipo() == 'R') cdb.R[dest.getId()].alocarRS(id, ciclo);
            }
            destino_pendente_no_cdb = false;
        }
        return true;
    }
    return false;
}

// Retorna true quando uma fase termina. Já avança 'fase' para o próximo estado para que executaWrTodos possa distinguir as transições sem ambiguidade.
bool RS::atualizaContagem(
    UnidadesFuncionais& uf,
    int                 ciclo
){
    if (!busy || contagem_regressiva_alocacao <= 0) return false;

    contagem_regressiva_alocacao--;
    if (contagem_regressiva_alocacao > 0) return false;

    TipoInstrucao tipo = instrucao_atual.getTipoInstrucao();

    if (tipo == TipoInstrucao::LOAD && fase == FaseInstrucao::EX) {
        liberarUF(uf, ciclo, FaseInstrucao::EX);
        contagem_regressiva_alocacao = -1;
        fase = FaseInstrucao::MEM; // avança para que executaWrTodos detecte EX→MEM
        return true;
    }

    if (tipo == TipoInstrucao::STORE && fase == FaseInstrucao::EX) {
        liberarUF(uf, ciclo, FaseInstrucao::EX);
        contagem_regressiva_alocacao = -1;
        fase = FaseInstrucao::MEM; // avança para que executaWrTodos detecte EX→MEM
        return true;
    }

    if (fase == FaseInstrucao::MEM) {
        liberarUF(uf, ciclo, FaseInstrucao::MEM);
        fase = FaseInstrucao::WB;
        return true;
    }
    liberarUF(uf, ciclo, FaseInstrucao::EX);
    fase = FaseInstrucao::WB;
    return true;
}

// Broadcast do CDB: se esta RS está esperando por rs_id com aquele ciclo_inicio, captura o valor agora. O par {rs_id, ciclo_inicio} identifica unicamente o produtor.
void RS::resolverDependencia(
    const std::string& rs_id,
    const Registrador& valor
){
    if (Qj.first == rs_id) { Vj = valor; Qj = {"", -1}; }
    if (Qk.first == rs_id) { Vk = valor; Qk = {"", -1}; }
}

void RS::liberar(
    int ciclo
){
    tempos_alocacao.push_back(ciclo);
    busy                         = false;
    contagem_regressiva_alocacao = -1;
    posicao_UF                   = -1;
    Qj = Qk                      = {"", -1};
    Vj = Vk                      = Registrador{};
    fase                         = FaseInstrucao::ISSUE;
}

// Recebe a fase em que a instrução VAI ENTRAR para escolher a UF correta:
// - LOAD/STORE em EX  → cálculo de endereço → ula_int_basico
// - LOAD/STORE em MEM → acesso à memória    → acessar_memoria
int RS::alocarUFLivre(
    std::vector<UF>& grupo,
    int              ciclo
) const {
    for (int i = 0; i < (int)grupo.size(); i++) {
        if (!grupo[i].busy) {
            grupo[i].busy     = true;
            grupo[i].RS_atual = id;
            grupo[i].RS_alocadas.push_back(id);
            grupo[i].tempo_alocacao.push_back(ciclo);
            return i;
        }
    }
    return -1;
}

int RS::procuraUFlivre(
    UnidadesFuncionais& uf,
    int                 ciclo,
    FaseInstrucao       fase_destino
) const {
    TipoInstrucao tipo = instrucao_atual.getTipoInstrucao();
    // LOAD e STORE: EX = cálculo de endereço (ULA inteira), MEM = acesso à memória
    if (tipo == TipoInstrucao::LOAD || tipo == TipoInstrucao::STORE) {
        if (fase_destino == FaseInstrucao::EX)  return alocarUFLivre(uf.ula_int_basico, ciclo);
        else                                    return alocarUFLivre(uf.acessar_memoria, ciclo);
    }
    switch (tipo) {
        case TipoInstrucao::INT_MUL:
        case TipoInstrucao::INT_DIV:      return alocarUFLivre(uf.ula_int_mult_div, ciclo);
        case TipoInstrucao::FLOAT_BASICO: return alocarUFLivre(uf.ula_float_basico, ciclo);
        case TipoInstrucao::FLOAT_MUL:
        case TipoInstrucao::FLOAT_DIV:    return alocarUFLivre(uf.ula_float_mult_div, ciclo);
        default:                          return alocarUFLivre(uf.ula_int_basico, ciclo);
    }
}

// Recebe a fase que ACABOU para saber de qual grupo liberar:
// - LOAD/STORE saindo de EX  → liberou ula_int_basico
// - LOAD/STORE saindo de MEM → liberou acessar_memoria
void RS::desalocarUFdoGrupo(
    std::vector<UF>& grupo,
    int              ciclo
) {
    if (posicao_UF < (int)grupo.size()) {
        grupo[posicao_UF].busy = false;
        grupo[posicao_UF].RS_atual = "";
        grupo[posicao_UF].tempo_alocacao.push_back(ciclo);
    }
}

void RS::liberarUF(
    UnidadesFuncionais& uf,
    int                 ciclo,
    FaseInstrucao       fase_que_terminou
){
    if (posicao_UF == -1) return;
    TipoInstrucao tipo = instrucao_atual.getTipoInstrucao();
    if (tipo == TipoInstrucao::LOAD || tipo == TipoInstrucao::STORE) {
        if (fase_que_terminou == FaseInstrucao::EX) desalocarUFdoGrupo(uf.ula_int_basico, ciclo);
        else                                        desalocarUFdoGrupo(uf.acessar_memoria, ciclo);
    } else {
        switch (tipo) {
            case TipoInstrucao::INT_MUL:
            case TipoInstrucao::INT_DIV:      desalocarUFdoGrupo(uf.ula_int_mult_div, ciclo);   break;
            case TipoInstrucao::FLOAT_BASICO: desalocarUFdoGrupo(uf.ula_float_basico, ciclo);   break;
            case TipoInstrucao::FLOAT_MUL:
            case TipoInstrucao::FLOAT_DIV:    desalocarUFdoGrupo(uf.ula_float_mult_div, ciclo); break;
            default:                          desalocarUFdoGrupo(uf.ula_int_basico, ciclo);     break;
        }
    }
    posicao_UF = -1;
}
