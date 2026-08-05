/* Components.cpp */
#include "headers/Components.h"

namespace processor {

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
char Register::GetType() const { return type; }

// Público:
int  Register::GetId()   const { return id; }

// Público:
bool Register::GetBusy() const { return busy; }

// Público:
const std::vector<std::string>& Register::GetAllocatedRS() const { return allocated_rs; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
Register::Register(
    const char type,
    const int  id
) : type(type), id(id) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

// Público:
// Inverte o estado do registrador.
void Register::ToggleBusy() { busy = !busy; }

// Público:
// Retorna o produtor pendente mais recente (último allocated_rs com end_times == -1).
// - É este que novas instruções devem aguardar em caso de WAW.
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
// - Usado pelo AddIssue para montar o par (rs_id, start_cycle) de Qj/Qk.
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
    const int          start_cycle
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
// - Múltiplos produtores pendentes (WAW) ficam na sequência de allocated_rs.
void Register::AllocateRS( // Retorno "void" pois sempre funciona.
    const std::string& rs,
    const int          start
){
    busy = true;
    allocated_rs.push_back(rs);
    start_times.push_back(start);
    // Começa em -1 para que start_time e end_time tenham o mesmo len.
    // - Evita erros como IndexOutOfBounds.
    end_times.push_back(-1);
}

// Público:
// Usa o par (rs_id, start_cycle) para identificar a entrada exata.
// - Evita ambiguidade quando o mesmo RS foi reutilizado múltiplas vezes (WAW).
bool Register::DeallocateRS( // O for pode não achar correspondente (por isso bool).
    const std::string& rs_id,
    const int          start_cycle,
    const int          end_cycle
){
    for (size_t i = 0; i < allocated_rs.size(); i++) {
        if (allocated_rs[i] == rs_id && start_times[i] == start_cycle) {
            end_times[i] = end_cycle;
            busy = HasPendingProducer();
            return true;
        }
    }
    return false;
}

// ─── HELPERS ──────────────────────────────────────────────────────
// Público:
const Register& GetReg(
    const CDB&      cdb,
    const Register& reg
){
    // Pesquisa apenas pelo id físico global; a classe não é validada aqui.
    if (reg.GetId() < 0 || static_cast<size_t>(reg.GetId()) >= cdb.registers.size()) {
        std::cerr << "[ERRO] Registrador fora do banco: " << reg.GetType() << reg.GetId() << "\n";
        std::abort();
    }
    return cdb.registers[reg.GetId()];
}

// Público:
Register& GetReg(
    CDB&            cdb,
    const Register& reg
){
    return const_cast<Register&>(GetReg(static_cast<const CDB&>(cdb), reg));
}

} // namespace processor
