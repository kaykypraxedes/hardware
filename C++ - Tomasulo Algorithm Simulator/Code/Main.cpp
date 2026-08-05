/* Main.cpp */
#include "headers/Processor.h"
#include "headers/Instruction.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace processor {

static const int W_POSITION = 12;
static const int W_INST     = 30;
static const int W_ISSUE    = 12;
static const int W_EX       = 16;
static const int W_MEM      = 16;
static const int W_WR       = 12;
static const int W_COMMIT   = 12;

// Ordem dos componentes usada tanto nos vetores de config (num_rs/num_fus)
// quanto nos grupos de RS/FU impressos no final.
static const std::vector<std::string> COMPONENT_LABELS = {
    "load",
    "store",
    "int_basic",
    "int_mult_div",
    "float_basic",
    "float_mult_div"
};

// Ordem assumida para latencias_mem (load, store). Ajustar se necessário.
static const std::vector<std::string> MEM_LABELS = { "load", "store" };
static const std::vector<std::string> EX_LABELS = {
    "invalid",
    "load",
    "store",
    "branch",
    "int_basic",
    "int_mult",
    "int_div",
    "float_basic",
    "float_mult",
    "float_div"
};

std::string CycleStr(
    int c
){
    return (c <= 0 ? "--" : std::to_string(c));
}

std::string FormatCycles(
    const std::vector<int>& v
){
    if (v.empty()) return "--";

    std::ostringstream oss;

    for (int i{}; i < (int)v.size(); i++) {
        if (i) oss << '-';

        oss << std::setw(2)
            << std::setfill('0')
            << v[i];
    }

    return oss.str();
}

int SeparatorWidth(
    bool rob
){
    int total =
        W_POSITION +
        W_INST +
        W_ISSUE +
        W_EX +
        W_MEM +
        W_WR;

    if (rob)
        total += W_COMMIT;

    return total - 7;
}

std::vector<int> ReadIntVector(
    std::istringstream& iss,
    int                 n,
    int                 default_val
){
    std::vector<int> v(n, default_val);
    for (int i = 0; i < n; i++) {
        if (!(iss >> v[i])) break;
    }
    return v;
}

struct CONFIG {
    PROCESSOR_TYPE       type          = PROCESSOR_TYPE::TOMASULO_CLASSIC;
    int                  num_threads   = 1;
    MULTITHREADING_MODEL model         = MULTITHREADING_MODEL::NONE;
    bool                 predictor     = false;
    int                  dispatch      = 2;
    std::vector<int>     num_rs        = {5, 5, 5, 4, 3, 2};
    std::vector<int>     num_fus       = {1, 1, 1, 1, 1, 2};
    std::vector<int>     ex_latencies  = Instruction::base_ex_latencies;
    std::vector<int>     mem_latencies = Instruction::base_mem_latencies;
    int                  cycle_limit   = 10000;
    std::vector<std::string> prog;
};

CONFIG ReadConfig() {
    CONFIG cfg;
    std::string line;

    while (std::getline(std::cin, line)) {

        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (key == "tipo"){
            int aux;
            iss >> aux;
            cfg.type = (aux == 0 ? PROCESSOR_TYPE::IN_ORDER :
                (aux == 1 ? PROCESSOR_TYPE::TOMASULO_CLASSIC : PROCESSOR_TYPE::TOMASULO_ESPECULATIVE));
        }
        else if (key == "previsor"){
            int aux;
            iss >> aux;
            cfg.predictor = (aux == 0 ? false : true);
        }
        else if (key == "num_threads") { iss >> cfg.num_threads; }
        else if (key == "modelo"){
            int aux;
            iss >> aux;
            cfg.model = (aux == 3 ? MULTITHREADING_MODEL::SMT    :
                (aux == 2 ? MULTITHREADING_MODEL::COARSE_GRAINED :
                (aux == 1 ? MULTITHREADING_MODEL::FINE_GRAINED   : MULTITHREADING_MODEL::NONE)));
        }
        else if (key == "despacho")     { iss >> cfg.dispatch;       }
        else if (key == "ciclo_limite") { iss >> cfg.cycle_limit;    }
        else if (key == "num_rs")       { cfg.num_rs       = ReadIntVector(iss, 6, 1);   }
        else if (key == "num_ufs")      { cfg.num_fus      = ReadIntVector(iss, 6, 1);   }
        else if (key == "latencias_ex") { cfg.ex_latencies = ReadIntVector(iss, 10, 1);  }
        else if (key == "latencias_mem"){ cfg.mem_latencies = ReadIntVector(iss, 2, 1); }
        else if (key == "programa")     {
            while (std::getline(std::cin, line)) {
                if (line == "END_PROG") break;
                if (!line.empty() && line[0] != '#')
                    cfg.prog.push_back(line);
            }
        }
        else if (key == "CODIGO_FONTE") { break; }
    }

    return cfg;
}

// Imprime um vetor de inteiros como "rotulo=valor rotulo=valor ...".
// Se faltar rótulo para alguma posição, imprime só o valor.
void PrintLabeledVector(
    const std::vector<int>&         values,
    const std::vector<std::string>& labels
){
    for (size_t i{}; i < values.size(); i++) {
        if (i < labels.size())
            std::cout << labels[i] << '=' << values[i] << ' ';
        else
            std::cout << values[i] << ' ';
    }
}

void PrintConfig(
    CONFIG cfg
){
    std::cout << "══════════════════════════════════════════════════════════\n" <<
                 "═══ CONFIGURAÇÕES ════════════════════════════════════════\n" <<
                 "══════════════════════════════════════════════════════════\n\n" <<
        "- Tipo: " << (cfg.type == PROCESSOR_TYPE::IN_ORDER ? "IN_ORDER" :
            (cfg.type == PROCESSOR_TYPE::TOMASULO_CLASSIC ? "TOMASULO_CLASSIC" : "TOMASULO_ESPECULATIVE")) << '\n' <<
        "- Numero de Threads: " << cfg.num_threads << '\n' <<
        "- Modelo Multi-Threading: " <<
            (cfg.model == MULTITHREADING_MODEL::FINE_GRAINED ? "FINE_GRAINED" :
            (cfg.model == MULTITHREADING_MODEL::COARSE_GRAINED ? "COARSE_GRAINED" :
            (cfg.model == MULTITHREADING_MODEL::SMT ? "SMT" : "NONE"))) << '\n' <<
        "- Previsor: " << (cfg.predictor == 0 ? "false" : "true") << '\n' <<
        "- Despacho: " << cfg.dispatch << '\n';

    std::cout << "- Número de RSs: ";
    PrintLabeledVector(cfg.num_rs, COMPONENT_LABELS);

    std::cout << "\n- Número de UFs: ";
    PrintLabeledVector(cfg.num_fus, COMPONENT_LABELS);

    std::cout << "\n- Ciclo limite: " << cfg.cycle_limit;

    std::cout << "\n- Latências de EX: ";
    PrintLabeledVector(cfg.ex_latencies, EX_LABELS);

    std::cout << "\n- Latências de MEM: ";
    PrintLabeledVector(cfg.mem_latencies, MEM_LABELS);

    std::cout << '\n';
}

void PrintTable(
    const std::vector<TABLE_ROW>& table,
    bool                          rob
) {
    std::cout << "══════════════════════════════════════════════════════════\n";
    std::cout << "═══ TABELA DE RESULTADOS ═════════════════════════════════\n";
    std::cout << "══════════════════════════════════════════════════════════\n\n";

    std::cout << std::left
              << std::setw(W_POSITION) << "Posição"
              << std::setw(W_INST)     << "Instrução"
              << std::setw(W_ISSUE)    << "Issue"
              << std::setw(W_EX)       << "EX"
              << std::setw(W_MEM)      << "MEM"
              << std::setw(W_WR)       << "WR";

    if (rob)
        std::cout << std::setw(W_COMMIT) << "Commit";

    std::cout << '\n';

    for (int i{}; i < SeparatorWidth(rob); i++) std::cout << "─";
    std::cout << '\n';

    for (const auto& l : table) {
        std::cout << std::left
                  << std::setw(W_POSITION - 2)
                  << l.instruction->GetPosition()

                  << std::setw(W_INST - 2)
                  << l.instruction->GetInstructionString()

                  << std::setw(W_ISSUE)
                  << CycleStr(l.issue_cycle)

                  << std::setw(W_EX)
                  << FormatCycles(l.ex_cycles)

                  << std::setw(W_MEM)
                  << FormatCycles(l.mem_cycles)

                  << std::setw(W_WR)
                  << CycleStr(l.wr_cycle);

        if (rob) {
            std::cout << std::setw(W_COMMIT)
                      << CycleStr(l.commit_cycle);
        }

        std::cout << '\n';
    }

    std::cout << '\n';
}

// ──────────────────────────────────────────────────────────
// Impressão unificada de "grupos de componentes" (RS, FU e CDB).
//
// Um grupo (ex.: "load") é composto por N unidades (load0, load1, ...).
// Cada unidade tem uma linha do tempo de pares (rótulo, início-fim).
//
// get_times/get_labels são extractors (lambdas) que sabem como pegar,
// de cada unidade, o vetor de tempos e o vetor de rótulos — isso permite
// reaproveitar a mesma função para RS (métodos GetTimes/GetInstructions),
// FU (membros allocation_times/allocated_rs) e Registradores da CDB
// (métodos GetAllocationTimes/GetAllocatedRS), sem duplicar a lógica de
// impressão em três funções quase idênticas.
// ──────────────────────────────────────────────────────────

template<typename LabelT>
struct TimelineEntry {
    LabelT label;
    int    start;
    int    end;
};

template<typename LabelT>
std::vector<TimelineEntry<LabelT>> BuildTimeline(
    const std::vector<int>&    times,
    const std::vector<LabelT>& labels
){
    std::vector<TimelineEntry<LabelT>> entries;

    for (int i{1}; i < (int)times.size(); i += 2) {
        entries.push_back({ labels[(i - 1) / 2], times[i - 1], times[i] });
    }

    return entries;
}

template<typename LabelT>
void PrintTimeline(
    const std::vector<TimelineEntry<LabelT>>& entries,
    int                                       indent
){
    std::string pad(indent, ' ');
    static const int TIME_COL = 26; // coluna onde [ini-fim] começa

    for (const auto& e : entries) {
        std::cout << pad
                  << std::left << std::setw(TIME_COL - indent - 4) << e.label
                  << " - [" << e.start << '-' << e.end << "]\n";
    }
}

template<typename T, typename TimesFn, typename LabelsFn>
void PrintComponentGroup(
    const std::string&    group_title,
    const std::vector<T>& components,
    TimesFn               get_times,
    LabelsFn              get_labels
){
    bool has_content = false;
    for (const auto& c : components) {
        if (!get_times(c).empty()) { has_content = true; break; }
    }

    // Componente sem nenhuma utilização (ex.: int_mult_div vazio) não é impresso.
    if (!has_content) return;
    std::cout << "─── " << group_title << ' ';
    size_t rest{53 - group_title.length()};
    for (size_t i{}; i < rest; i++) std::cout << "─";
    std::cout << '\n';

    for (int j{}; j < (int)components.size(); j++) {
        auto times  = get_times(components[j]);
        auto labels = get_labels(components[j]);

        if (times.empty()) continue;

        std::cout << "- " << group_title << j << ":\n";
        PrintTimeline(BuildTimeline(times, labels), 4);
        std::cout << '\n';
    }

    std::cout << '\n';
}

} // namespace processor

int main() {

    processor::CONFIG cfg = processor::ReadConfig();

    processor::PrintConfig(cfg);

    processor::Instruction::base_ex_latencies  = cfg.ex_latencies;
    processor::Instruction::base_mem_latencies = cfg.mem_latencies;

    processor::Processor p(
        cfg.num_threads,
        cfg.dispatch,
        cfg.predictor,
        cfg.type,
        cfg.model,
        cfg.prog,
        cfg.num_rs,
        cfg.num_fus
    );

    std::cout << "\nSimulando...\n";

    int result{};

    for (int c{}; c < cfg.cycle_limit && !result; c++) {
        result = p.ExecuteCycle();
    }

    std::cout
        << (result
            ? "Concluido!\n"
            : "Limite de " + std::to_string(cfg.cycle_limit) + " ciclos atingido.\n");

    std::cout << '\n';

    PrintTable(
        p.GetThreadTable(0),
        p.GetType() == processor::PROCESSOR_TYPE::TOMASULO_ESPECULATIVE
    );

    std::cout << "══════════════════════════════════════════════════════════\n";
    std::cout << "═══ RESERVATION STATIONS (RS) ════════════════════════════\n";
    std::cout << "══════════════════════════════════════════════════════════\n\n";

    auto rs = p.GetThread(0).GetRS();

    processor::PrintComponentGroup("load", rs.load,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    processor::PrintComponentGroup("store", rs.store,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    processor::PrintComponentGroup("int_basic", rs.int_basic,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    processor::PrintComponentGroup("int_mult_div", rs.int_mult_div,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    processor::PrintComponentGroup("float_basic", rs.float_basic,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    processor::PrintComponentGroup("float_mult_div", rs.float_mult_div,
        [](const auto& c) { return c.GetTimes(); },
        [](const auto& c) { return c.GetInstructions(); });

    std::cout << "══════════════════════════════════════════════════════════\n";
    std::cout << "═══ COMMOM DATA BUS (CDB) ════════════════════════════════\n";
    std::cout << "══════════════════════════════════════════════════════════\n\n";

    auto cdb = p.GetThread(0).GetCDB();

    processor::PrintComponentGroup("F", cdb.F,
        [](const auto& r) { return r.GetAllocationTimes(); },
        [](const auto& r) { return r.GetAllocatedRS(); });

    processor::PrintComponentGroup("R", cdb.R,
        [](const auto& r) { return r.GetAllocationTimes(); },
        [](const auto& r) { return r.GetAllocatedRS(); });

    std::cout << "══════════════════════════════════════════════════════════\n";
    std::cout << "═══ FUNCIONAL UNITYS (FU) ════════════════════════════════\n";
    std::cout << "══════════════════════════════════════════════════════════\n\n";

    auto fu = p.GetThread(0).GetFU();

    processor::PrintComponentGroup("memory_access", fu.memory_access,
        [](const auto& c) { return c.allocation_times; },
        [](const auto& c) { return c.allocated_rs; });

    processor::PrintComponentGroup("int_basic_alu", fu.int_basic_alu,
        [](const auto& c) { return c.allocation_times; },
        [](const auto& c) { return c.allocated_rs; });

    processor::PrintComponentGroup("int_mult_div_alu", fu.int_mult_div_alu,
        [](const auto& c) { return c.allocation_times; },
        [](const auto& c) { return c.allocated_rs; });

    processor::PrintComponentGroup("float_basic_alu", fu.float_basic_alu,
        [](const auto& c) { return c.allocation_times; },
        [](const auto& c) { return c.allocated_rs; });

    processor::PrintComponentGroup("float_mult_div_alu", fu.float_mult_div_alu,
        [](const auto& c) { return c.allocation_times; },
        [](const auto& c) { return c.allocated_rs; });

    return 0;
}
