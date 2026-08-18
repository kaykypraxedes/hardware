/* Components.cpp */
#include "headers/Components.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados no header.
 */

namespace processor {

// ─── HELPER ───────────────────────────────────────────────────────
Register& GetReg(
    CDB&            cdb,
    const Register& reg
){
    for (Register& slot : cdb.registers) {
        if (slot.GetType() == reg.GetType() &&
            slot.GetId()   == reg.GetId()   &&
            slot.GetMask() == reg.GetMask())
            return slot;
    }
    // Não encontrou o registrador.
    std::cerr <<
        "[ERRO] Slot não encontrado:\n"
        "- Tipo de registrador: " << reg.GetType() << '\n' <<
        "- Id: " << reg.GetId() << '\n' <<
        "- Máscara: " << reg.GetMask() << '\n';
    std::abort();
}

// ──────────────────────────────────────────────────────────────────
// ─── CLASSE ───────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
char Register::GetType() const { return type; }

// Público:
int  Register::GetId()   const { return id; }

// Público:
int  Register::GetMask() const { return mask; }

// Público:
bool Register::GetBusy() const { return busy; }

// Público:
std::vector<int> Register::GetAllocationTimes() const {
    std::vector<int> result;
    result.reserve(start_times.size() * 2);
    for (size_t i{}; i < start_times.size(); i++) {
        result.push_back(start_times[i]);
        result.push_back(end_times[i]);
    }
    return result;
}

// Público:
std::string Register::GetCurrentRS() const {
    for (int i{static_cast<int>(allocated_rs.size()) - 1}; i >= 0; i--) {
        if (end_times[i] == -1) return allocated_rs[i];
    }
    return "";
}

// Público:
const std::vector<std::string>& Register::GetAllocatedRS() const { return allocated_rs; }

// ─── CONSTRUTOR ───────────────────────────────────────────────────

// Público:
Register::Register(
    const char type,
    const int  id,
    const int  mask
) : type(type), id(id), mask(mask) {}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
void Register::ToggleBusy() { busy = !busy; }

// Público:
int Register::GetRSCycleStart(
    const std::string& rs_id
) const {
    for (int i{static_cast<int>(allocated_rs.size()) - 1}; i >= 0; i--) {
        if (end_times[i] == -1 && allocated_rs[i] == rs_id)
            return start_times[i];
    }
    return -1;
}

// Público:
bool Register::IsDependencyResolved(
    const std::string& rs_id,
    const int          start_cycle
) const {
    for (size_t i{}; i < allocated_rs.size(); i++) {
        if (start_times[i] == start_cycle && allocated_rs[i] == rs_id) {
            return end_times[i] != -1;
        }
    }
    return false;
}

// Público:
void Register::AllocateRS( // Retorno "void" pois sempre funciona.
    const std::string& rs_id,
    const int          start_cycle
){
    busy = true;
    allocated_rs.push_back(rs_id);
    start_times.push_back(start_cycle);
    // Começa em -1 para que "start_times" e "end_times" tenham o mesmo tamanho.
    // - Evita erros como "IndexOutOfBounds".
    end_times.push_back(-1);
}

// Público:
bool Register::DeallocateRS( // Retorno "bool" pois o "for" pode não achar correspondente.
    const std::string& rs_id,
    const int          start_cycle,
    const int          end_cycle
){
    for (size_t i{}; i < allocated_rs.size(); i++) {
        if (allocated_rs[i] == rs_id && start_times[i] == start_cycle) {
            end_times[i] = end_cycle;
            busy = HasPendingProducer();
            return true;
        }
    }
    return false;
}

// Privado:
bool Register::HasPendingProducer() const {
    for (size_t i{}; i < end_times.size(); i++)
        if (end_times[i] == -1) return true;
    return false;
}

} // namespace processor
