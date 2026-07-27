// ──────────────────────────────────────────────────────────
// Para rodar: ./build/executable < test-cases/inputs/<arquivo>.txt
// ──────────────────────────────────────────────────────────

#include "headers/Processor.h"
#include "headers/Instruction.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace processor {

static const int W_PC     = 6;
static const int W_INST   = 30;
static const int W_ISSUE  = 10;
static const int W_EX     = 16;
static const int W_MEM    = 16;
static const int W_WR     = 10;
static const int W_COMMIT = 10;

std::string CycleStr(
    int c
){
    return (c == 0 ? "--" : std::to_string(c));
}

std::string VecStr(
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

int TotalWidth(
    bool rob
){
    int total =
        W_PC +
        W_INST +
        W_ISSUE +
        W_EX +
        W_MEM +
        W_WR;

    if (rob)
        total += W_COMMIT;

    return total;
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
    PROCESSOR_TYPE       type          = PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB;
    int                  num_threads   = 1;
    MULTITHREADING_MODEL model         = MULTITHREADING_MODEL::FINE_GRAINED;
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
                (aux == 1 ? PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB : PROCESSOR_TYPE::TOMASULO_WITH_ROB));
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
            cfg.model = (aux == 2 ? MULTITHREADING_MODEL::SMT :
                (aux == 0 ? MULTITHREADING_MODEL::FINE_GRAINED : MULTITHREADING_MODEL::COARSE_GRAINED));
        }
        else if (key == "despacho")    { iss >> cfg.dispatch;       }
        else if (key == "ciclo_limite"){ iss >> cfg.cycle_limit;    }
        else if (key == "num_rs")      { cfg.num_rs       = ReadIntVector(iss, 6, 1);   }
        else if (key == "num_ufs")     { cfg.num_fus      = ReadIntVector(iss, 6, 1);   }
        else if (key == "latencias_ex"){ cfg.ex_latencies = ReadIntVector(iss, 10, 1);  }
        else if (key == "latencias_mem"){ cfg.mem_latencies = ReadIntVector(iss, 2, 1); }
        else if (key == "programa")    {
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

void PrintConfig(
    CONFIG cfg
){
    std::cout << "==============\n" <<
                 "CONFIGURAÇÕES:\n" <<
                 "==============\n\n" <<
        "- Tipo: " << (cfg.type == PROCESSOR_TYPE::IN_ORDER ? "IN_ORDER" :
            (cfg.type == PROCESSOR_TYPE::TOMASULO_WITHOUT_ROB ? "TOMASULO_SEM_ROB" : "TOMASULO_COM_ROB")) << '\n' <<
        "- Numero de Threads: " << cfg.num_threads << '\n' <<
        "- Modelo Multi-Threading: " << (cfg.model == MULTITHREADING_MODEL::FINE_GRAINED ? "GRANULACAO_FINA" :
            (cfg.model == MULTITHREADING_MODEL::COARSE_GRAINED ? "GRANULACAO_GROSSA" : "SMT")) << '\n' <<
        "- Previsor: " << (cfg.predictor == 0 ? "false" : "true") << '\n' <<
        "- Despacho: " << cfg.dispatch << '\n' <<
        "- Número de RSs: ";
        for(int i : cfg.num_rs){
            std::cout << i << ' ';
        }
        std::cout << "\n- Número de UFs: ";
        for(int i : cfg.num_fus){
            std::cout << i << ' ';
        }
        std::cout << "\n- Ciclo limite: " << cfg.cycle_limit;
        std::cout << "\n- Latências de EX: ";
        for(int i : cfg.ex_latencies){
            std::cout << i << ' ';
        }
        std::cout << "\n- Latências de MEM: ";
        for(int i : cfg.mem_latencies){
            std::cout << i << ' ';
        }
        std::cout << '\n';
}

void PrintTable(
    const std::vector<TABLE_ROW>& table,
    bool                            rob
) {
    std::cout << "=====================\n";
    std::cout << "TABELA DE RESULTADOS\n";
    std::cout << "=====================\n\n";

    std::cout << std::left
              << std::setw(W_PC)     << "PC"
              << std::setw(W_INST)   << "Instrucao"
              << std::setw(W_ISSUE)  << "Issue"
              << std::setw(W_EX)     << "EX"
              << std::setw(W_MEM)    << "MEM"
              << std::setw(W_WR)     << "WR";

    if (rob)
        std::cout << std::setw(W_COMMIT) << "Commit";

    std::cout << '\n';

    std::cout << std::string(TotalWidth(rob), '-') << '\n';

    for (const auto& l : table) {
        std::cout << std::left
                  << std::setw(W_PC)
                  << l.instruction.GetPC()

                  << std::setw(W_INST)
                  << l.instruction.GetInstructionString()

                  << std::setw(W_ISSUE)
                  << CycleStr(l.issue_cycle)

                  << std::setw(W_EX)
                  << VecStr(l.ex_cycles)

                  << std::setw(W_MEM)
                  << VecStr(l.mem_cycles)

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

template<typename T>
void PrintStructure(
    const std::string&    title,
    const std::vector<T>& structure
){
    std::cout << title << ":\n";

    for (int j{}; j < (int)structure.size(); j++) {

        auto tempos = structure[j].GetTimes();
        auto insts  = structure[j].GetInstructions();

        if (tempos.empty())
            continue;

        std::cout << std::left
                  << std::setw(20)
                  << (title + std::to_string(j));

        for (int i{1}; i < (int)tempos.size(); i += 2) {

            std::cout
                << insts[(i - 1) / 2]
                << " ("
                << tempos[i - 1]
                << '-'
                << tempos[i]
                << ") ";

            if (i + 1 < (int)tempos.size())
                std::cout << "| ";
        }

        std::cout << '\n';
    }

    std::cout << '\n';
}

template<typename T>
void PrintFU(
    const std::string&    title,
    const std::vector<T>& structure
) {
    std::cout << title << ":\n";

    for (int j{}; j < (int)structure.size(); j++) {

        auto tempos = structure[j].allocation_times;
        auto rs     = structure[j].allocated_rs;

        if (tempos.empty())
            continue;

        std::cout << std::left
                  << std::setw(24)
                  << (title + std::to_string(j));

        for (int i{1}; i < (int)tempos.size(); i += 2) {

            std::cout
                << rs[(i - 1) / 2]
                << " ("
                << tempos[i - 1]
                << '-'
                << tempos[i]
                << ") ";

            if (i + 1 < (int)tempos.size())
                std::cout << "| ";
        }

        std::cout << '\n';
    }

    std::cout << '\n';
}

void PrintRegisters(
    const std::string&          label,
    const std::vector<Register>& regs
) {
    for (int j{}; j < num_registers; j++) {
        auto tempos = regs[j].GetAllocationTimes();
        auto rsaloc = regs[j].GetAllocatedRS();
        if (tempos.empty()) continue;

        std::cout << std::setw(8) << (label + std::to_string(j));
        for (int i{1}; i < (int)tempos.size(); i += 2) {
            std::cout << rsaloc[(i - 1) / 2]
                      << " (" << tempos[i - 1] << '-' << tempos[i] << ") ";
            if (i + 1 < (int)tempos.size()) std::cout << "| ";
        }
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
        p.GetType() == processor::PROCESSOR_TYPE::TOMASULO_WITH_ROB
    );

    std::cout << "=====================\n";
    std::cout << "RESERVATION STATIONS\n";
    std::cout << "=====================\n\n";

    auto rs = p.GetThread(0).GetRS();

    processor::PrintStructure("load", rs.load);
    processor::PrintStructure("store", rs.store);
    processor::PrintStructure("int_basic", rs.int_basic);
    processor::PrintStructure("int_mult_div", rs.int_mult_div);
    processor::PrintStructure("float_basic", rs.float_basic);
    processor::PrintStructure("float_mult_div", rs.float_mult_div);

    std::cout << "=====================\n";
    std::cout << "CDB\n";
    std::cout << "=====================\n\n";

    auto cdb = p.GetThread(0).GetCDB();

    std::cout << "F:\n";
    processor::PrintRegisters("F", cdb.F);
    std::cout << "R:\n";
    processor::PrintRegisters("R", cdb.R);

    std::cout << "=====================\n";
    std::cout << "UNIDADES FUNCIONAIS\n";
    std::cout << "=====================\n\n";

    auto fu = p.GetThread(0).GetFU();

    processor::PrintFU("acessar_memoria", fu.memory_access);
    processor::PrintFU("ula_int_basico", fu.int_basic_alu);
    processor::PrintFU("ula_int_mult_div", fu.int_mult_div_alu);
    processor::PrintFU("ula_float_basico", fu.float_basic_alu);
    processor::PrintFU("ula_float_mult_div", fu.float_mult_div_alu);

    return 0;
}
