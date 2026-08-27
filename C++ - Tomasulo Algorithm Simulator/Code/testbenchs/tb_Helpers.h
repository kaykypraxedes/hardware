/* tb_Helpers.h */
// Módulo auxiliar para todos os testbenchs
#ifndef TB_HELPERS_H
#define TB_HELPERS_H

#include "../headers/Components.h"
#include <cerrno>       // errno e EINTR.
#include <cstddef>
#include <fcntl.h>      // open() e O_WRONLY.
#include <iostream>
#include <ratio>
#include <signal.h>     // SIGABRT.
#include <string>
#include <sys/wait.h>   // waitpid(), WIFSIGNALED() e WTERMSIG().
#include <unistd.h>     // fork(), dup2(), close(), _exit() e descritores POSIX.
#include <vector>

// ─── HELPERS ──────────────────────────────────────────────────────

/**
 * @brief Executa uma operação isolada e confirma encerramento por SIGABRT.
 *
 * @details O filho redireciona apenas o stderr esperado para /dev/null. O pai
 * aguarda mesmo quando waitpid() é interrompido e aceita somente SIGABRT;
 * retorno normal ou falha da infraestrutura permanecem falhas observáveis.
 *
 * @tparam OPERATION Tipo invocável sem argumentos.
 *
 * @param operation Operação sem argumentos executada no processo filho.
 *
 * @return true quando a operação encerra o filho com SIGABRT.
 */
template <typename OPERATION>
static bool Aborts(
    const OPERATION& operation
) {
    // Evita duplicar saída ainda armazenada nos buffers após fork().
    std::cout.flush();
    std::cerr.flush();

    const pid_t child{fork()};
    if (child == 0) {
        // Silencia somente a mensagem esperada da operação inválida.
        const int null_fd{open("/dev/null", O_WRONLY)};
        if (null_fd < 0) _exit(1);
        if (dup2(null_fd, STDERR_FILENO) < 0) {
            close(null_fd);
            _exit(1);
        }
        if (close(null_fd) < 0) _exit(1);

        operation();
        _exit(0);
    }
    if (child < 0) return false;

    // Espera o filho sem transformar interrupção temporária em falha.
    int status{};
    pid_t waited{};
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);

    return waited == child && WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

static int passed = 0, failed = 0;

static void check(const std::string& test, bool condition) {
    if (condition) { std::cout << "  [OK]  " << test << "\n"; passed++; }
    else           { std::cout << "  [FALHOU] " << test << "\n"; failed++; }
}

// Verifica se um registrador (type, id) está presente na lista (o sombreamento
// de sub-registradores adiciona variantes mascaradas, então as asserções usam
// presença em vez de contagens exatas de slots).
[[maybe_unused]] static bool has_reg(const std::vector<processor::Register>& regs, char type, int id) {
    for (const auto& r : regs)
        if (r.GetType() == type && r.GetId() == id) return true;
    return false;
}

// Todos os ids da lista pertencem ao conjunto {ids} (ignora variantes mascaradas).
[[maybe_unused]] static bool only_ids(const std::vector<processor::Register>& regs, std::initializer_list<int> ids) {
    for (const auto& r : regs) {
        bool found = false;
        for (int id : ids)
            if (r.GetId() == id) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

// Nenhum registrador da lista tem o tipo dado (ex.: sem 'G' = sem CPSR/EFLAGS).
[[maybe_unused]] static bool no_type(const std::vector<processor::Register>& regs, char type) {
    for (const auto& r : regs)
        if (r.GetType() == type) return false;
    return true;
}

static size_t charCount(const std::string& s) {
    size_t n{};
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) n++;   // ignora bytes de continuação do UTF-8
    return n;
}

static void section(const std::string& name) {
    std::cout << "─── " << name << ' ';
    size_t n{charCount(name)};
    size_t max_size{101};
    size_t len{n < max_size ? max_size - n : 0};   // clamp contra underflow
    for (size_t i{}; i < len; i++) std::cout << "─";
    std::cout << '\n';
}

static void print_title(const std::string& title) {
    std::cout << "═════════════════════════════════════════════════════════════════════\n"; // 69
    std::cout << "═══ " << title << ' ';
    size_t n{charCount(title)};
    size_t max_size{69 - 5};
    size_t len{n < max_size ? max_size - n : 0};   // clamp contra underflow
    for (size_t i{}; i < len; i++) std::cout << "═";
    std::cout << '\n';
    std::cout << "═════════════════════════════════════════════════════════════════════\n\n";
}

#endif
