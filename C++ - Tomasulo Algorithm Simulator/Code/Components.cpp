/* Components.cpp */
#include "headers/Components.h"
#include <cstdlib>

namespace processor {

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
char Register::GetType() const { return type; }

// Público:
int  Register::GetId()   const { return id;   }

// Público:
bool Register::GetBusy() const { return busy; }

// Público:
const std::vector<std::string>& Register::GetAllocatedRS() const { return allocated_rs; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Register::Register(
    std::string register_string
)
{
    if (!ParseType(register_string) || !ParseId(register_string)){
        std::cerr << "[ERRO] Register string inválida: " << register_string << "\n";
        std::abort();
    }
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Privado:
bool Register::ParseType(
    const std::string& s
){
    // Casos válidos:
    // 1. Input vazio
    if (s.empty()) { type = 'Z'; return true; }
    // 2. Input[0] == 'F' || Input[0] == 'R'
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    if (c == 'F' || c == 'R') { type = c; return true; }
    // Casos inválidos:
    // - Input[0] = "1", "A", "#", etc.
    return false;
}

// Privado:
bool Register::ParseId(
    const std::string& s
){
    // Casos válidos:
    // 1. Input vazio
    if (s.empty()) { id = -1; return true; }
    try{
        // 2. Input[2-n] está no intervalo dos registradores
        int id_aux{std::stoi(s.substr(1))};
        if(id_aux >= 0 && id_aux < num_registers) { id = id_aux; return true; }
    } catch (...) { return false; } // Casos não suportados pelo std::stoi ou pelo std::string::substr
    // Casos inválidos:
    // - Input[2-n] = "-1", "1236", "#", etc.
    return false;
}

// Público:
// Inverte o estado do registrador.
void Register::ToggleBusy() { busy = !busy; }

// Público:
// Retorna o produtor pendente mais recente (último allocated_rs com end_times == -1).
// É este que novas instruções devem aguardar em caso de WAW.
std::string Register::GetCurrentRS() const {
    for (int i = static_cast<int>(allocated_rs.size() - 1); i >= 0; i--) {
        if (end_times[i] == -1) return allocated_rs[i];
    }
    return "";
}

// Privado:
bool Register::HasPendingProducer() const {
    for (size_t i = 0; i < end_times.size(); i++)
        if (end_times[i] == -1) return true;
    return false;
}

// Público:
// Retorna o start_cycle do produtor pendente mais recente com esse nome.
// Usado pelo AddIssue para montar o par (rs_id, start_cycle) de Qj/Qk.
int Register::GetRSCycleStart(
    const std::string& rs_id
) const {
    for (int i = static_cast<int>(allocated_rs.size()) - 1; i >= 0; i--) {
        if (allocated_rs[i] == rs_id && end_times[i] == -1)
            return start_times[i];
    }
    return -1;
}

// Público:
// Para evitar ambiguidade quando o mesmo RS reutiliza múltiplas vezes o mesmo registrador.
bool Register::IsDependencyResolved(
    const std::string& rs_id,
    int start_cycle
) const {
    for (size_t i = 0; i < allocated_rs.size(); i++) {
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
// Registra o novo produtor.
// - end_times começa em -1 (para que start_time e end_time tenham o mesmo len, evitando IndexOutOfBounds).
// Múltiplos produtores pendentes (WAW) ficam na sequência de allocated_rs.
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
// Usa o par (rs_id, start_cycle) para identificar a entrada exata, evitando ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes (WAW).
void Register::DeallocateRS(
    const std::string& rs_id,
    int start_cycle,
    int end_cycle
){
    for (size_t i = 0; i < allocated_rs.size(); i++) {
        if (allocated_rs[i] == rs_id && start_times[i] == start_cycle) {
            end_times[i] = end_cycle;
            break;
        }
    }
    busy = HasPendingProducer();
}

} // namespace processor
