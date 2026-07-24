/* Componentes.cpp */
#include "headers/Componentes.h"

// Métodos públicos
// Construtores:
Registrador::Registrador(){}
Registrador::Registrador(
    std::string registrador_em_string
)
{
    tipo = identificaTipo(registrador_em_string);
    id   = identificaId(registrador_em_string);
}
// Códigos pequenos:
char Registrador::getTipo()                           const { return tipo;}
int  Registrador::getId()                             const { return id;}
bool Registrador::getBusy()                           const { return busy;}
std::vector<std::string> Registrador::getRSalocadas() const { return RS_alocadas;}
void Registrador::trocaBusy()                               { busy = !busy; }

// Códigos grandes:
// Retorna o produtor pendente mais recente (último RS_alocadas com tempo_fim == -1).
// É este que novas instruções devem aguardar em caso de WAW.
std::string Registrador::getRSatual() const {
    for (int i = (int)RS_alocadas.size() - 1; i >= 0; i--) {
        if (tempo_fim[i] == -1) return RS_alocadas[i];
    }
    return "";
}

bool Registrador::temProdutorPendente() const {
    for (int i = 0; i < (int)tempo_fim.size(); i++)
        if (tempo_fim[i] == -1) return true;
    return false;
}

// Retorna o ciclo_inicio do produtor pendente mais recente com esse nome.
// Usado pelo addIssue para montar o par (rs_id, ciclo_inicio) de Qj/Qk.
int Registrador::getCicloInicioRS(
    const std::string& rs_id
) const {
    for (int i = (int)RS_alocadas.size() - 1; i >= 0; i--) {
        if (RS_alocadas[i] == rs_id && tempo_fim[i] == -1)
            return tempo_inicio[i];
    }
    return -1;
}

// Para evitar ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes.
bool Registrador::dependenciaResolvida(
    const std::string& rs_id,
    int ciclo_inicio
) const {
    for (int i = 0; i < (int)RS_alocadas.size(); i++) {
        if (RS_alocadas[i] == rs_id && tempo_inicio[i] == ciclo_inicio) {
            return tempo_fim[i] != -1;
        }
    }
    return false; // não encontrado — trata como não resolvido
}

std::vector<int> Registrador::getTempoAlocacao() const {
    std::vector<int> resultado;
    resultado.reserve(tempo_inicio.size() * 2);
    for (size_t i = 0; i < tempo_inicio.size(); i++) {
        resultado.push_back(tempo_inicio[i]);
        resultado.push_back(tempo_fim[i]);
    }
    return resultado;
}

// Registra o novo produtor. tempo_fim começa em -1 (pendente).
// Múltiplos produtores pendentes (WAW) ficam na sequência de RS_alocadas.
void Registrador::alocarRS(
    const std::string& rs,
    int                inicio
){
    busy     = true;
    RS_alocadas.push_back(rs);
    tempo_inicio.push_back(inicio);
    tempo_fim.push_back(-1);     // -1 = ainda pendente
}

// Usa o par (rs_id, ciclo_inicio) para identificar a entrada exata,
// evitando ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes (WAW).
void Registrador::desalocarRS(
    const std::string& rs_id,
    int                ciclo_inicio,
    int                ciclo_fim
){
    for (int i = 0; i < (int)RS_alocadas.size(); i++) {
        if (RS_alocadas[i] == rs_id && tempo_inicio[i] == ciclo_inicio) {
            tempo_fim[i] = ciclo_fim;
            break;
        }
    }
    busy = temProdutorPendente();
}

// Métodos privados
// Códigos grandes:
char Registrador::identificaTipo(
    const std::string& s
){
    if (s.empty()) return 'Z';
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    if (c == 'F') return 'F';
    if (c == 'R') return 'R';
    return 'Z';
}

int Registrador::identificaId(
    const std::string& s
){
    if (s.size() < 2 || !std::isdigit(static_cast<unsigned char>(s[1]))) return -1;
    return std::stoi(s.substr(1));
}
