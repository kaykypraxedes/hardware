// ──────────────────────────────────────────────────────────
// Para rodar: ./executable < test-cases/inputs/<arquivo>.txt
// ──────────────────────────────────────────────────────────

#include "headers/Processador.h"
#include "headers/Instrucao.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

constexpr int LIMITE_CICLOS = 10000;

constexpr int W_PC       = 6;
constexpr int W_INST     = 30;
constexpr int W_ISSUE    = 10;
constexpr int W_EX       = 16;
constexpr int W_MEM      = 16;
constexpr int W_WR       = 10;
constexpr int W_COMMIT   = 10;

// Helpers:

std::string ciclo_str(
    int c
){
    return (c == 0 ? "--" : std::to_string(c));
}

std::string vec_str(
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

int largura_total(
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

// Lê N inteiros de uma linha e devolve como vector<int>.
// Se a linha tiver menos de N valores, completa com 'padrao'.
std::vector<int> lerVetorInt(
    std::istringstream& iss,
    int                 n,
    int                 padrao
){
    std::vector<int> v(n, padrao);
    for (int i = 0; i < n; i++) {
        if (!(iss >> v[i])) break;
    }
    return v;
}

// Leitura das configuracoes via stdin
struct Config {
    TipoProcessador tipo            = TipoProcessador::TOMASULO_SEM_ROB;
    int num_threads                 = 1;
    ModeloMultithreading modelo     = ModeloMultithreading::GRANULACAO_FINA;
    bool previsor                   = false;
    int despacho                    = 2;
    // num_rs / num_ufs seguem a ordem:
    //   num_rs:  load, store, int_basico, int_mult_div, float_basico, float_mult_div
    //   num_ufs: acessar_memoria, ula_int_basico, ula_int_mult_div, ula_float_basico, ula_float_mult_div, wr
    std::vector<int> num_rs          = {5, 5, 5, 4, 3, 2};
    std::vector<int> num_ufs         = {1, 1, 1, 1, 1, 2};
    // latencias_EX seguem a ordem do enum TipoInstrucao:
    //   NAO_EXISTE, LOAD, STORE, BRANCH, INT_BASICO, INT_MUL, INT_DIV, FLOAT_BASICO, FLOAT_MUL, FLOAT_DIV
    std::vector<int> latencias_EX    = Instrucao::latencias_basicas_EX;
    // latencias_MEM seguem a ordem: LOAD, STORE
    std::vector<int> latencias_MEM   = Instrucao::latencias_basicas_MEM;
    std::vector<std::string> prog;
};

Config lerConfig() {
    Config cfg;
    std::string linha;

    while (std::getline(std::cin, linha)) {

        // Ignora comentarios e linhas vazias
        if (linha.empty() || linha[0] == '#')
            continue;

        std::istringstream iss(linha);
        std::string chave;
        iss >> chave;

        if (chave == "tipo"){
            int aux;
            iss >> aux;
            cfg.tipo = (aux == 0 ? TipoProcessador::IN_ORDER :
                (aux == 1 ? TipoProcessador::TOMASULO_SEM_ROB : TipoProcessador::TOMASULO_COM_ROB));
        }
        else if (chave == "previsor"){
            int aux;
            iss >> aux;
            cfg.previsor = (aux == 0 ? false : true);
        }
        else if (chave == "num_threads") { iss >> cfg.num_threads; }
        else if (chave == "modelo"){
            int aux;
            iss >> aux;
            cfg.modelo = (aux == 2 ? ModeloMultithreading::SMT :
                (aux == 0 ? ModeloMultithreading::GRANULACAO_FINA : ModeloMultithreading::GRANULACAO_GROSSA));
        }
        else if (chave == "despacho")    { iss >> cfg.despacho;    }
        else if (chave == "num_rs")      { cfg.num_rs        = lerVetorInt(iss, 6, 1); }
        else if (chave == "num_ufs")     { cfg.num_ufs       = lerVetorInt(iss, 6, 1); }
        else if (chave == "latencias_ex"){ cfg.latencias_EX  = lerVetorInt(iss, 10, 1); }
        else if (chave == "latencias_mem"){ cfg.latencias_MEM = lerVetorInt(iss, 2, 1); }
        else if (chave == "programa")    {
            // Le instrucoes ate encontrar END_PROG
            while (std::getline(std::cin, linha)) {
                if (linha == "END_PROG") break;
                if (!linha.empty() && linha[0] != '#')
                    cfg.prog.push_back(linha);
            }
        }
        // Ignora a secao CODIGO_FONTE e tudo abaixo
        else if (chave == "CODIGO_FONTE") { break; }
    }

    return cfg;
}

void imprimirConfig(
    Config cfg
){
    std::cout << "==============\n" <<
                 "CONFIGURAÇÕES:\n" <<
                 "==============\n\n" <<
        "- Tipo: " << (cfg.tipo == TipoProcessador::IN_ORDER ? "IN_ORDER" :
            (cfg.tipo == TipoProcessador::TOMASULO_SEM_ROB ? "TOMASULO_SEM_ROB" : "TOMASULO_COM_ROB")) << '\n' <<
        "- Numero de Threads: " << cfg.num_threads << '\n' <<
        "- Modelo Multi-Threading: " << (cfg.modelo == ModeloMultithreading::GRANULACAO_FINA ? "GRANULACAO_FINA" :
            (cfg.modelo == ModeloMultithreading::GRANULACAO_GROSSA ? "GRANULACAO_GROSSA" : "SMT")) << '\n' <<
        "- Previsor: " << (cfg.previsor == 0 ? "false" : "true") << '\n' <<
        "- Despacho: " << cfg.despacho << '\n' <<
        "- Número de RSs: ";
        for(int i : cfg.num_rs){
            std::cout << i << ' ';
        }
        std::cout << "\n- Número de UFs: ";
        for(int i : cfg.num_ufs){
            std::cout << i << ' ';
        }
        std::cout << "\n- Latêcias de EX: ";
        for(int i : cfg.latencias_EX){
            std::cout << i << ' ';
        }
        std::cout << "\n- Latêcias de MEM: ";
        for(int i : cfg.latencias_MEM){
            std::cout << i << ' ';
        }
        std::cout << '\n';
}

// Impressao da tabela principal
void imprimirTabela(
    const std::vector<LinhaTabela>& tabela,
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

    std::cout << std::string(largura_total(rob), '-') << '\n';

    for (const auto& l : tabela) {
        std::cout << std::left
                  << std::setw(W_PC)
                  << l.instrucao.getPC()

                  << std::setw(W_INST)
                  << l.instrucao.getInstrucaoString()

                  << std::setw(W_ISSUE)
                  << ciclo_str(l.ciclo_issue)

                  << std::setw(W_EX)
                  << vec_str(l.ciclo_EX)

                  << std::setw(W_MEM)
                  << vec_str(l.ciclo_MEM)

                  << std::setw(W_WR)
                  << ciclo_str(l.ciclo_WR);

        if (rob) {
            std::cout << std::setw(W_COMMIT)
                      << ciclo_str(l.ciclo_commit);
        }

        std::cout << '\n';
    }

    std::cout << '\n';
}

// Impressao generica de estruturas temporais:
template<typename T>
void imprimirEstrutura(
    const std::string&    titulo,
    const std::vector<T>& estrutura
){
    std::cout << titulo << ":\n";

    for (int j{}; j < (int)estrutura.size(); j++) {

        auto tempos = estrutura[j].getTempos();
        auto insts  = estrutura[j].getInstrucoes();

        if (tempos.empty())
            continue;

        std::cout << std::left
                  << std::setw(20)
                  << (titulo + std::to_string(j));

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

// Impressao generica de unidades funcionais:
template<typename T>
void imprimirUF(
    const std::string&    titulo,
    const std::vector<T>& estrutura
) {
    std::cout << titulo << ":\n";

    for (int j{}; j < (int)estrutura.size(); j++) {

        auto tempos = estrutura[j].tempo_alocacao;
        auto rs     = estrutura[j].RS_alocadas;

        if (tempos.empty())
            continue;

        std::cout << std::left
                  << std::setw(24)
                  << (titulo + std::to_string(j));

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

int main() {

    Config cfg = lerConfig();

    imprimirConfig(cfg);

    // As tabelas de latência são elementos static de Instrucao e precisam
    // ser definidas ANTES de criar o Processador, pois cada Instrucao já
    // calcula suas latências no próprio construtor.
    Instrucao::latencias_basicas_EX  = cfg.latencias_EX;
    Instrucao::latencias_basicas_MEM = cfg.latencias_MEM;

    Processador p(
        cfg.num_threads,
        cfg.despacho,
        cfg.previsor,
        cfg.tipo,
        cfg.modelo,
        cfg.prog,
        cfg.num_rs,
        cfg.num_ufs
    );

    std::cout << "\nSimulando...\n";

    int resultado{};

    for (int c{}; c < LIMITE_CICLOS && !resultado; c++) { // Máximo de ciclos = LIMITE_CICLOS
        resultado = p.executarCiclo();
    }

    std::cout
        << (resultado
            ? "Concluido!\n"
            : "Limite de " + std::to_string(LIMITE_CICLOS) + " ciclos atingido.\n");

    std::cout << '\n';

    // ─────────────────────────────────────────────────
    // TABELA PRINCIPAL
    // ─────────────────────────────────────────────────

    imprimirTabela(
        p.getTabelaThread(0),
        p.getTipo() == TipoProcessador::TOMASULO_COM_ROB
    );

    // ─────────────────────────────────────────────────
    // RESERVATION STATIONS
    // ─────────────────────────────────────────────────

    std::cout << "=====================\n";
    std::cout << "RESERVATION STATIONS\n";
    std::cout << "=====================\n\n";

    auto rs = p.getThread(0).getRS();

    imprimirEstrutura("load", rs.load);
    imprimirEstrutura("store", rs.store);
    imprimirEstrutura("int_basico", rs.int_basico);
    imprimirEstrutura("int_mult_div", rs.int_mult_div);
    imprimirEstrutura("float_basico", rs.float_basico);
    imprimirEstrutura("float_mult_div", rs.float_mult_div);

    // ─────────────────────────────────────────────────
    // CDB
    // ─────────────────────────────────────────────────

    std::cout << "=====================\n";
    std::cout << "CDB\n";
    std::cout << "=====================\n\n";

    auto cdb = p.getThread(0).getCDB();

    std::cout << "F:\n";

    for (int j{}; j < NUM_REGISTRADORES; j++) {

        auto tempos = cdb.F[j].getTempoAlocacao();
        auto rsaloc = cdb.F[j].getRSalocadas();

        if (tempos.empty())
            continue;

        std::cout << std::setw(8)
                  << ("F" + std::to_string(j));

        for (int i{1}; i < (int)tempos.size(); i += 2) {

            std::cout
                << rsaloc[(i - 1) / 2]
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

    std::cout << "\nR:\n";

    for (int j{}; j < NUM_REGISTRADORES; j++) {

        auto tempos = cdb.R[j].getTempoAlocacao();
        auto rsaloc = cdb.R[j].getRSalocadas();

        if (tempos.empty())
            continue;

        std::cout << std::setw(8)
                  << ("R" + std::to_string(j));

        for (int i{1}; i < (int)tempos.size(); i += 2) {

            std::cout
                << rsaloc[(i - 1) / 2]
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

    // ─────────────────────────────────────────────────
    // UNIDADES FUNCIONAIS
    // ─────────────────────────────────────────────────

    std::cout << "=====================\n";
    std::cout << "UNIDADES FUNCIONAIS\n";
    std::cout << "=====================\n\n";

    auto uf = p.getThread(0).getUF();

    imprimirUF("acessar_memoria", uf.acessar_memoria);
    imprimirUF("ula_int_basico", uf.ula_int_basico);
    imprimirUF("ula_int_mult_div", uf.ula_int_mult_div);
    imprimirUF("ula_float_basico", uf.ula_float_basico);
    imprimirUF("ula_float_mult_div", uf.ula_float_mult_div);

    return 0;
}
