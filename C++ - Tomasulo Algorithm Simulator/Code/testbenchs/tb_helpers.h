#ifndef TB_HELPERS_H
#define TB_HELPERS_H

#include <cstddef>
#include <iostream>
#include <ratio>
#include <string>

static int passou = 0, falhou = 0;

static void check(const std::string& teste, bool condicao) {
    if (condicao) { std::cout << "  [OK]  " << teste << "\n"; passou++; }
    else          { std::cout << "  [FALHOU] " << teste << "\n"; falhou++; }
}

static size_t caracteres(const std::string& s) {
    size_t n{};
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) n++;   // ignora bytes de continuação do UTF-8
    return n;
}

static void secao(const std::string& nome) {
    std::cout << "─── " << nome << ' ';
    size_t n{caracteres(nome)};
    size_t max_size{101};
    size_t len{n < max_size ? max_size - n : 0};   // clamp contra underflow
    for (size_t i{}; i < len; i++) std::cout << "─";
    std::cout << '\n';
}

static void print_title(const std::string& title) {
    std::cout << "═════════════════════════════════════════════════════════════════════\n"; // 69
    std::cout << "═══ " << title << ' ';
    size_t n{caracteres(title)};
    size_t max_size{69 - 5};
    size_t len{n < max_size ? max_size - n : 0};   // clamp contra underflow
    for (size_t i{}; i < len; i++) std::cout << "═";
    std::cout << '\n';
    std::cout << "═════════════════════════════════════════════════════════════════════\n\n";
}

#endif
