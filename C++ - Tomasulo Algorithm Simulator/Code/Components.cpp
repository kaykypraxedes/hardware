/* Components.cpp */
#include "headers/Components.h"

// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "header.h".
 */

namespace processor {

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

// ─── CONSTRUTOR ───────────────────────────────────────────────────

// Público:
Register::Register(
    const char type,
    const int  id,
    const int  mask
) : type(type), id(id), mask(mask) {}


// ==================================================================
// === CLASSE =======================================================
// ==================================================================

// ─── CONSTRUTOR ───────────────────────────────────────────────────
// Público:
RegisterStatusTable::RegisterStatusTable(
    const std::vector<Register>& references
) {
    for (const Register& reference : references) AddReference(reference);
}

// ─── GETTERS ─────────────────────────────────────────────────────────────
// Público:
REGISTER_STATUS_VIEW RegisterStatusTable::FindStatus(
    const Register& reference
) const {
    return MakeView(FindEntry(reference));
}

// Público:
bool RegisterStatusTable::IsProducerResolved(
    const Register& reference,
    const int       producer_position
) const {
    const STATUS_ENTRY& entry{FindEntry(reference)};
    for (const PRODUCER_RECORD& producer : entry.producers)
        if (producer.position == producer_position) return !producer.pending;
    return false;
}

// Público:
bool RegisterStatusTable::IsBusy(
    const Register& reference
) const {
    const STATUS_ENTRY& entry{FindEntry(reference)};
    for (const PRODUCER_RECORD& producer : entry.producers)
        if (producer.pending) return true;
    return false;
}

// Público:
std::vector<Register> RegisterStatusTable::GetReferences() const {
    std::vector<Register> references;
    references.reserve(entries.size());
    for (const STATUS_ENTRY& entry : entries) references.push_back(entry.reference);
    return references;
}

// Público:
std::vector<REGISTER_STATUS_VIEW> RegisterStatusTable::GetStatuses() const {
    std::vector<REGISTER_STATUS_VIEW> statuses;
    statuses.reserve(entries.size());
    for (const STATUS_ENTRY& entry : entries) statuses.push_back(MakeView(entry));
    return statuses;
}

// Público:
std::size_t RegisterStatusTable::Size() const { return entries.size(); }

// Privado:
RegisterStatusTable::STATUS_ENTRY& RegisterStatusTable::FindEntry(
    const Register& reference
) {
    for (STATUS_ENTRY& entry : entries)
        if (SameReference(entry.reference, reference)) return entry;

    std::cerr <<
        "[ERRO] Status de registrador não encontrado:\n"
        "- Tipo: " << reference.GetType() << '\n' <<
        "- Id: " << reference.GetId() << '\n' <<
        "- Máscara: " << reference.GetMask() << '\n';
    std::abort();
}

// Privado:
const RegisterStatusTable::STATUS_ENTRY& RegisterStatusTable::FindEntry(
    const Register& reference
) const {
    for (const STATUS_ENTRY& entry : entries)
        if (SameReference(entry.reference, reference)) return entry;

    std::cerr <<
        "[ERRO] Status de registrador não encontrado:\n"
        "- Tipo: " << reference.GetType() << '\n' <<
        "- Id: " << reference.GetId() << '\n' <<
        "- Máscara: " << reference.GetMask() << '\n';
    std::abort();
}

// ─── DEMAIS MÉTODOS ────────────────────────────────────────────────────────────
// Público:
void RegisterStatusTable::AddReference(
    const Register& reference
) {
    for (const STATUS_ENTRY& entry : entries) {
        if (SameReference(entry.reference, reference)) {
            std::cerr << "[ERRO] Referência de registrador duplicada no layout.\n";
            std::abort();
        }
    }
    entries.push_back({reference, {}, {}});
}

// Público:
void RegisterStatusTable::AllocateProducer(
    const Register&    reference,
    const int          producer_position,
    const std::string& rs_id,
    const int          cycle
) {
    if (producer_position < 0) {
        std::cerr << "[ERRO] Posição inválida de produtor: " << producer_position << '\n';
        std::abort();
    }

    STATUS_ENTRY& entry{FindEntry(reference)};
    for (const PRODUCER_RECORD& producer : entry.producers) {
        if (producer.position == producer_position) {
            std::cerr << "[ERRO] Produtor já registrado: " << producer_position << '\n';
            std::abort();
        }
    }

    entry.producers.push_back({producer_position, true});
    entry.trace.push_back({producer_position, rs_id, cycle, -1});
}

// Público:
bool RegisterStatusTable::DeallocateProducer(
    const Register& reference,
    const int       producer_position,
    const int       cycle
) {
    STATUS_ENTRY& entry{FindEntry(reference)};
    for (PRODUCER_RECORD& producer : entry.producers) {
        if (producer.position != producer_position || !producer.pending) continue;

        producer.pending = false;
        for (TRACE_RECORD& trace : entry.trace) {
            if (trace.producer_position == producer_position && trace.end_cycle == -1) {
                trace.end_cycle = cycle;
                return true;
            }
        }
        std::cerr << "[ERRO] Trace de produtor funcional não encontrado.\n";
        std::abort();
    }
    return false;
}

// Público:
int RegisterStatusTable::FindLatestProducerBefore(
    const Register& reference,
    const int       consumer_position
) const {
    const STATUS_ENTRY& entry{FindEntry(reference)};
    int latest_position{-1};
    for (const PRODUCER_RECORD& producer : entry.producers) {
        if (producer.position < consumer_position && producer.position > latest_position)
            latest_position = producer.position;
    }
    return latest_position;
}

// Privado:
bool RegisterStatusTable::SameReference(
    const Register& left,
    const Register& right
) {
    return left.GetType() == right.GetType() &&
           left.GetId()   == right.GetId() &&
           left.GetMask() == right.GetMask();
}

// Privado:
REGISTER_STATUS_VIEW RegisterStatusTable::MakeView(
    const STATUS_ENTRY& entry
) {
    REGISTER_STATUS_VIEW view;
    view.reference = entry.reference;
    for (const PRODUCER_RECORD& producer : entry.producers) {
        view.producer_positions.push_back(producer.position);
        if (producer.pending) view.busy = true;
    }
    for (const TRACE_RECORD& trace : entry.trace) {
        view.allocated_rs.push_back(trace.rs_id);
        view.allocation_times.push_back(trace.start_cycle);
        view.allocation_times.push_back(trace.end_cycle);
    }
    return view;
}


// ==================================================================
// === CDB_BROADCAST ================================================
// ==================================================================

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
bool CDB_BROADCAST::CompleteProducer(
    RegisterStatusTable& register_status,
    const int            cycle
) const {
    return register_status.DeallocateProducer(
        destination,
        producer_position,
        cycle
    );
}


// ==================================================================
// === FuncionalUnits ===============================================
// ==================================================================

// ─── CONSTRUTORES ─────────────────────────────────────────────────
// Público:
FuncionalUnits::FuncionalUnits(
    const FUNCTIONAL_UNIT_CAPACITIES& capacities
) {
    // Cada campo nomeado materializa somente seu grupo físico.
    memory_access.resize(capacities.memory_access);
    int_basic_alu.resize(capacities.int_basic);
    int_mult_div_alu.resize(capacities.int_mult_div);
    float_basic_alu.resize(capacities.float_basic);
    float_mult_div_alu.resize(capacities.float_mult_div);
}

// ─── GETTERS ──────────────────────────────────────────────────────
// Público:
const std::vector<FU>& FuncionalUnits::GetMemoryAccessUnits() const {
    return memory_access;
}

// Público:
const std::vector<FU>& FuncionalUnits::GetIntBasicUnits() const {
    return int_basic_alu;
}

// Público:
const std::vector<FU>& FuncionalUnits::GetIntMultDivUnits() const {
    return int_mult_div_alu;
}

// Público:
const std::vector<FU>& FuncionalUnits::GetFloatBasicUnits() const {
    return float_basic_alu;
}

// Público:
const std::vector<FU>& FuncionalUnits::GetFloatMultDivUnits() const {
    return float_mult_div_alu;
}

// Privado:
std::vector<FU>& FuncionalUnits::GetGroup(
    const FUNCTIONAL_UNIT_GROUP group
) {
    switch (group) {
        case FUNCTIONAL_UNIT_GROUP::MEMORY_ACCESS: return memory_access;
        case FUNCTIONAL_UNIT_GROUP::INT_BASIC: return int_basic_alu;
        case FUNCTIONAL_UNIT_GROUP::INT_MULT_DIV: return int_mult_div_alu;
        case FUNCTIONAL_UNIT_GROUP::FLOAT_BASIC: return float_basic_alu;
        case FUNCTIONAL_UNIT_GROUP::FLOAT_MULT_DIV: return float_mult_div_alu;
    }

    std::cerr << "[ERRO] Grupo de FU inválido.\n";
    std::abort();
}

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────
// Público:
int FuncionalUnits::Allocate(
    const FUNCTIONAL_UNIT_GROUP group,
    const std::string&          rs_id,
    const int                   cycle
) {
    if (HasAllocation(rs_id)) {
        std::cerr << "[ERRO] RS já possui uma FU alocada: " << rs_id << '\n';
        std::abort();
    }

    std::vector<FU>& units{GetGroup(group)};
    for (std::size_t position{}; position < units.size(); position++) {
        FU& unit{units[position]};
        if (unit.busy) continue;

        unit.busy = true;
        unit.current_rs = rs_id;
        unit.allocated_rs.push_back(rs_id);
        unit.allocation_times.push_back(cycle);
        return static_cast<int>(position);
    }
    return -1;
}

// Público:
void FuncionalUnits::Release(
    const FUNCTIONAL_UNIT_GROUP group,
    const int                   position,
    const std::string&          rs_id,
    const int                   cycle
) {
    std::vector<FU>& units{GetGroup(group)};
    if (position < 0 || position >= static_cast<int>(units.size())) {
        std::cerr <<
            "[ERRO] Posição inválida de FU: " << position << '\n' <<
            "- RS: " << rs_id << '\n';
        std::abort();
    }

    FU& unit{units[position]};
    if (!unit.busy || unit.current_rs != rs_id) {
        std::cerr <<
            "[ERRO] Associação RS/FU inválida na liberação.\n" <<
            "- RS esperada: " << unit.current_rs << '\n' <<
            "- RS informada: " << rs_id << '\n';
        std::abort();
    }

    unit.busy = false;
    unit.current_rs.clear();
    unit.allocation_times.push_back(cycle);
}

// Privado:
bool FuncionalUnits::HasAllocation(
    const std::string& rs_id
) const {
    const std::vector<const std::vector<FU>*> groups{
        &memory_access,
        &int_basic_alu,
        &int_mult_div_alu,
        &float_basic_alu,
        &float_mult_div_alu
    };
    for (const std::vector<FU>* group : groups)
        for (const FU& unit : *group)
            if (unit.busy && unit.current_rs == rs_id) return true;
    return false;
}

// ─── HELPERS ──────────────────────────────────────────────────────

// Valida quantidade e não-negatividade do formato posicional externo.
static void ValidateExternalCapacities(
    const std::vector<int>& values,
    const std::size_t       expected_size,
    const std::string&      component
) {
    if (values.size() != expected_size) {
        std::cerr <<
            "[ERRO] Quantidade inválida de " << component << ": " << values.size() << '\n';
        std::abort();
    }
    for (const int value : values) {
        if (value < 0) {
            std::cerr << "[ERRO] Capacidade de " << component << " não pode ser negativa.\n";
            std::abort();
        }
    }
}

RESERVATION_STATION_CAPACITIES MakeReservationStationCapacities(
    const std::vector<int>& values
) {
    ValidateExternalCapacities(values, 6, "RSs");
    return {values[0], values[1], values[2], values[3], values[4], values[5]};
}

FUNCTIONAL_UNIT_CAPACITIES MakeFunctionalUnitCapacities(
    const std::vector<int>& values
) {
    ValidateExternalCapacities(values, 6, "FUs");
    return {values[0], values[1], values[2], values[3], values[4]};
}

int GetExternalWriteResultWidth(
    const std::vector<int>& values
) {
    ValidateExternalCapacities(values, 6, "FUs");
    if (values[5] == 0) {
        std::cerr << "[ERRO] Largura de WR deve ser positiva.\n";
        std::abort();
    }
    return values[5];
}

} // namespace processor
