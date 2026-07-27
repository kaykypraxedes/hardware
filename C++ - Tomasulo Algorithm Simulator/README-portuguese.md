# Simulador do Algoritmo de Tomasulo

Simulador ciclo-a-ciclo de processadores Out-of-Order baseados no **Algoritmo de Tomasulo**, escrito em C++.

**Desenvolvedor:** Kayky Moreira Praxedes

---

## O que é o Algoritmo de Tomasulo?

O Algoritmo de Tomasulo é uma técnica de hardware para escalonamento dinâmico de instruções que permite execução fora de ordem (Out-of-Order) sem criar dependências falsas. Diferente de processadores In-Order, que executam instruções estritamente na ordem do programa, o Tomasulo usa **renomeação de registradores** e **estações de reserva** para rastrear dependências reais (RAW) e ignorar dependências de nome (WAR, WAW).

### Propriedades fundamentais
- Renomeação de registradores elimina WAW e WAR
- Estações de reserva desacoplam issue de execução
- Common Data Bus (CDB) faz broadcast de resultados para todas as unidades

---

## Funcionalidades

- **[In-Order / Tomasulo s/ ROB / c/ ROB]** — Três modos de execução selecionáveis por parâmetro: pipeline In-Order, Tomasulo sem ROB (Reorder Buffer), e Tomasulo com ROB (commit em ordem).
- **[Multithreading]** — Suporte a três modelos: granulação fina (alterna a cada ciclo), granulação grossa (troca a cada N instruções) e SMT (issue simultâneo em round-robin).
- **[Superscalar]** — Largura de despacho configurável, permitindo emitir múltiplas instruções por ciclo.
- **[Previsor de desvios]** — Previsor liga/desliga por parâmetro; desvios não resolvidos travam o despacho.
- **[Latências customizáveis]** — Latências de execução e memória configuráveis por tipo de instrução via entrada padrão.

---

## Estrutura do Projeto

```
Code/
├── Makefile
├── Main.cpp                     # Leitura da config + simulação + impressão
├── Processor.cpp                # Orquestração multiciclo
├── Thread.cpp                   # Pipeline por thread (Issue, EX, MEM, WB, Commit)
├── ReservationStations.cpp      # Estação de reserva individual
├── Components.cpp               # Register, CDB, FU
├── Instruction.cpp              # Instruction (parsing e tipos)
├── headers/                     # Arquivos .h de cabeçalho
├── testbenchs/                  # Testbenchs individuais para cada .cpp
└── test-cases/
    ├── inputs/                  # Arquivos de config .txt
    └── expected/                # Saídas de referência .expected
```

---

## Módulos

### `Instruction`

Faz o parsing da instrução a partir do mnemônico, identifica o tipo (LOAD, STORE, INT_BASIC, INT_MUL, INT_DIV, FLOAT_BASIC, FLOAT_MUL, FLOAT_DIV, BRANCH) e extrai seus operandos (registradores destino, fonte e imediatos) com latências default por tipo.

### `Components`

Define as três estruturas fundamentais do simulador: **Register** (rastreia busy e produtores pendentes via CDB), **Common Data Bus (CDB)** (centraliza o tracking de quem produz cada registrador) e **Functional Unit (FU)** (controla busy, alocação e contagem regressiva de latência).

### `ReservationStations`

Implementa a lógica central do algoritmo: `AddIssue()` aloca a estação resolvendo WAR (lendo Qj/Qk antes de marcar destino), `UpdateDependencies()` gerencia a transição entre fases da pipeline (EX, MEM) e `ResolveDependency()` faz o broadcast do resultado liberando instruções dependentes.

### `Thread`

Gerencia o pipeline completo de uma thread: Issue (despacho para estação de reserva), ExMem (execução e acesso à memória), Wr (broadcast no CDB) e Commit (apenas no modo com ROB, garante finalização em ordem). Suporta troca de contexto entre threads conforme o modelo de multithreading.

### `Processor`

Orquestra a execução multiciclo: a cada ciclo executa `ExMem -> Wr -> Commit` em todas as threads e depois `Issue` (despacho), aplicando a política de escalonamento escolhida (round-robin, prioridade fixa ou troca por contagem).

---

## Testes

1. **Testbenchs unitários** — Validam Components, Instruction, ReservationStations, Thread e Processor individualmente.
2. **Simulação comparada** — A saída do simulador é comparada com arquivos `.expected` para detectar regressões.

Compile com `make` e execute os testes com `make test`.

---

## Uso / Configuração

```bash
make
cd Code && ./executable < arquivo_de_entrada.txt
```

O arquivo de entrada define o tipo de processador, número de threads, largura de despacho, quantidade de estações de reserva e unidades funcionais, latências e o programa assembly. Consulte `Code/test-cases/inputs/` para exemplos completos.
