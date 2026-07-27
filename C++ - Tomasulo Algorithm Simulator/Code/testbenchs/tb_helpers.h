#ifndef TB_HELPERS_H
#define TB_HELPERS_H

#include <iostream>
#include <string>

static int passou = 0, falhou = 0;

static void check(const std::string& teste, bool condicao) {
    if (condicao) { std::cout << "  [OK]  " << teste << "\n"; passou++; }
    else          { std::cout << "  [FALHOU] " << teste << "\n"; falhou++; }
}

static void secao(const std::string& nome) {
    std::cout << "\n══ " << nome << " ══\n";
}

#endif
