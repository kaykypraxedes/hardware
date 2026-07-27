/* Components.cpp */
#include "headers/Components.h"

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
char Register::GetType() const { return type; }

// Público:
int  Register::GetId()   const { return id;   }

// Público:
bool Register::GetBusy() const { return busy; }

// Público:
// const & para evitar cópia (não usado em tipos pequenos por ganho marginal pequeno)
const std::vector<std::string>& Register::GetAllocatedRS() const { return allocated_rs; }

// ─── CONSTRUTORES ─────────────────────────────────────────────────
// Público:
Register::Register(){}

// Público:
Register::Register(
    std::string register_string
)
{
    type = IdentifyType(register_string);
    id   = IdentifyId(register_string);
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
char Register::IdentifyType(
    const std::string& s
){
    if (s.empty()) return 'Z';
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    if (c == 'F') return 'F';
    if (c == 'R') return 'R';
    return 'Z';
}

// Privado:
int Register::IdentifyId(
    const std::string& s
){
    if (s.size() < 2 || !std::isdigit(static_cast<unsigned char>(s[1]))) return -1;
    return std::stoi(s.substr(1));
}

// Público:
// Inverte o estado do registrador.
void Register::ToggleBusy() { busy = !busy; }

// Público:
// Retorna o produtor pendente mais recente (último RS_alocadas com tempo_fim == -1).
// É este que novas instruções devem aguardar em caso de WAW.
std::string Register::GetCurrentRS() const {
    for (int i = (int)allocated_rs.size() - 1; i >= 0; i--) {
        if (end_times[i] == -1) return allocated_rs[i];
    }
    return "";
}

// Público:
bool Register::HasPendingProducer() const {
    for (int i = 0; i < (int)end_times.size(); i++)
        if (end_times[i] == -1) return true;
    return false;
}

// Público:
// Retorna o ciclo_inicio do produtor pendente mais recente com esse nome.
// Usado pelo addIssue para montar o par (rs_id, ciclo_inicio) de Qj/Qk.
int Register::GetRSCycleStart(
    const std::string& rs_id
) const {
    for (int i = (int)allocated_rs.size() - 1; i >= 0; i--) {
        if (allocated_rs[i] == rs_id && end_times[i] == -1)
            return start_times[i];
    }
    return -1;
}

// Público:
// Para evitar ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes.
bool Register::IsDependencyResolved(
    const std::string& rs_id,
    int start_cycle
) const {
    for (int i = 0; i < (int)allocated_rs.size(); i++) {
        if (allocated_rs[i] == rs_id && start_times[i] == start_cycle) {
            return end_times[i] != -1;
        }
    }
    return false;
}

// Público:
std::vector<int> Register::GetAllocationTimes() const {
    std::vector<int> result;
    result.reserve(start_times.size() * 2);
    for (size_t i = 0; i < start_times.size(); i++) {
        result.push_back(start_times[i]);
        result.push_back(end_times[i]);
    }
    return result;
}

// Público:
// Registra o novo produtor. tempo_fim começa em -1 (pendente).
// Múltiplos produtores pendentes (WAW) ficam na sequência de RS_alocadas.
void Register::AllocateRS(
    const std::string& rs,
    int start
){
    busy = true;
    allocated_rs.push_back(rs);
    start_times.push_back(start);
    end_times.push_back(-1);
}

// Público:
// Usa o par (rs_id, ciclo_inicio) para identificar a entrada exata, evitando ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes (WAW).
void Register::DeallocateRS(
    const std::string& rs_id,
    int start_cycle,
    int end_cycle
){
    for (int i = 0; i < (int)allocated_rs.size(); i++) {
        if (allocated_rs[i] == rs_id && start_times[i] == start_cycle) {
            end_times[i] = end_cycle;
            break;
        }
    }
    busy = HasPendingProducer();
}
