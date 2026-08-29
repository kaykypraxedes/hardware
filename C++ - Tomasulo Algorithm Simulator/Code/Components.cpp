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

// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────

} // namespace processor
