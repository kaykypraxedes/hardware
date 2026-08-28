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

// ==================================================================
// === CLASSE =======================================================
// ==================================================================

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
int Register::GetCurrentProducer() const {
    for (int i{static_cast<int>(producer_positions.size()) - 1}; i >= 0; i--) {
        if (end_times[i] == -1) return producer_positions[i];
    }
    return -1;
}

// Público:
const std::vector<std::string>& Register::GetAllocatedRS() const { return allocated_rs; }

// Público:
const std::vector<int>& Register::GetProducerPositions() const { return producer_positions; }

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
bool Register::IsDependencyResolved(
    const int producer_position
) const {
    for (size_t i{}; i < producer_positions.size(); i++)
        if (producer_positions[i] == producer_position)
            return end_times[i] != -1;
    return false;
}

// Público:
void Register::AllocateProducer(
    const int          producer_position,
    const std::string& rs_id,
    const int          cycle
){
    if (producer_position < 0) {
        std::cerr << "[ERRO] Posição inválida de produtor: " << producer_position << '\n';
        std::abort();
    }
    for (const int registered_position : producer_positions) {
        if (registered_position == producer_position) {
            std::cerr << "[ERRO] Produtor já registrado: " << producer_position << '\n';
            std::abort();
        }
    }

    busy = true;
    producer_positions.push_back(producer_position);
    allocated_rs.push_back(rs_id);
    start_times.push_back(cycle);
    // Começa em -1 para que "start_times" e "end_times" tenham o mesmo tamanho.
    // - Evita erros como "IndexOutOfBounds".
    end_times.push_back(-1);
}

// Público:
bool Register::DeallocateProducer(
    const int producer_position,
    const int cycle
){
    for (size_t i{}; i < producer_positions.size(); i++) {
        if (producer_positions[i] == producer_position && end_times[i] == -1) {
            end_times[i] = cycle;
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
