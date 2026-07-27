# Simulador do Algoritmo de Tomasulo (v14-final)

## Visão Geral
Simulador ciclo-a-ciclo de processadores baseados no **Algoritmo de Tomasulo** (c/ e s/ ROB) e **In-Order**, escrito em **C++**. Suporta **multithreading** (granulação fina, grossa, SMT), **superscalar**, **previsor de desvios** e latências customizáveis via stdin.

## Entrada (stdin)
```
tipo        0|1|2          (In-Order | Tomasulo s/ ROB | Tomasulo c/ ROB)
previsor    0|1
num_threads N
modelo      0|1|2          (gran. fina | gran. grossa | SMT)
despacho    N              (largura de despacho)
num_rs      6 valores       load store int_basic int_mult_div float_basic float_mult_div
num_fu      6 valores       memory_access int_basic_alu int_mult_div_alu float_basic_alu float_mult_div_alu wr (commit = despacho)
latencias_ex  10 valores    NONEXISTENT LOAD STORE BRANCH INT_BASIC INT_MUL INT_DIV FLOAT_BASIC FLOAT_MUL FLOAT_DIV
latencias_mem 2 valores     LOAD STORE
programa     instrucoes... END_PROG
```

## Pipeline (s/ ROB): `Issue → EX → MEM (LOAD/STORE) → WB → fim`
## Pipeline (c/ ROB):  `Issue → EX → MEM (LOAD/STORE) → WB → Commit`

## Hierarquia de Classes

```
Code/
├── Makefile
├── Main.cpp                     (leitura + simulação + impressão)
├── Processor.cpp                (Processor, orquestração multiciclo)
├── Thread.cpp                   (Issue, ExMem, Wr, Commit)
├── ReservationStations.cpp      (AddIssue, dep tracking, contagem)
├── Components.cpp               (Register, CDB, FU)
├── Instruction.cpp              (Instruction, parsing, tipo, latências)
├── headers/                     (arquivos .h de cabeçalho)
└── testbenchs/                  (testes unitários)
```

## `Instruction` — Parsing e Tipos
|      Tipo      |                             Exemplos                            | latEX default | latMEM default |
|----------------|-----------------------------------------------------------------|---------------|----------------|
| `LOAD`         | L.D, LW, LB, LH, LBU, LHU, L.S, LD, LWU, LL                     |        1      |        1       |
| `STORE`        | S.D, SW, SB, SH, S.S, SD, SC                                    |        1      |        1       |
| `INT_BASIC`    | ADD, SUB, ADDI, AND, OR, XOR, NOR, SLT, SLL, SRL, SRA, LUI, ... |        1      |        -       |
| `INT_MUL`      | MULT, MULTU, MUL, DMULT, DMULTU                                 |        4      |        -       |
| `INT_DIV`      | DIV, DIVU, DDIV, DDIVU                                          |        10     |        -       |
| `FLOAT_BASIC`  | ADD.D, ADD.S, SUB.D, SUB.S                                      |        9      |        -       |
| `FLOAT_MUL`    | MUL.D, MUL.S                                                    |        14     |        -       |
| `FLOAT_DIV`    | DIV.D, DIV.S                                                    |        40     |        -       |
| `BRANCH`       | BEQ, BNE, BNEZ, BEQZ, BGTZ, BLTZ, J, JR, JAL, JALR, ...         |        1      |        -       |

- `IdentifyType()` mapeia mnemônico → `INSTRUCTION_TYPE`.
- `SetAttributes()`: LOAD → dest=R1, K=R2; STORE → J=R1, K=R2; ADD → dest=R1, J=R2, K=R3; BRANCH → J e/ou K se forem registradores.

## `Register` — Rastreamento de Dependências
- Atributos: `type` ('R'/'F'/'Z'), `id`, `busy`, `allocated_rs[]`, `start_times[]`, `end_times[]`.
- `GetCurrentRS()`: produtor pendente mais recente (último com `end_times == -1`), usado para WAW.
- `AllocateRS(rs_id, start)`: marca busy, adiciona produtor com `end_times = -1`.
- `DeallocateRS(rs_id, start_cycle, end_cycle)`: usa **hash (rs_id, start_cycle)** para desambiguar reuso do mesmo RS.
- `IsDependencyResolved(rs_id, start_cycle)`: true se aquele produtor específico já terminou.

## `CDB` (Common Data Bus) — Struct
```cpp
struct CDB { vector<Register> R[32]; vector<Register> F[32]; };
```
Centraliza o tracking de **quem produz cada registrador**.

## `FU` / `FUNCTIONAL_UNITS`
Grupos: `memory_access`, `int_basic_alu`, `int_mult_div_alu`, `float_basic_alu`, `float_mult_div_alu`. Cada FU: `{busy, current_rs, allocation_times[], allocated_rs[]}`. `wr` e `commit` são inteiros (largura). `commit` = `largura_de_despacho` (implicitamente, não configurável via `num_fu`).

## `ReservationStation` — Lógica Central
### `AddIssue(instrucao, cdb, ciclo)`
1. Lê **Qj/Qk do CDB antes** de marcar o destino (WAR).
2. Se tag vazia → `Vj/Vk` = operando; senão → `Qj/Qk = {rs_id, ciclo_inicio}`.
3. Marca destino no CDB (`AllocateRS`).
4. Trata WAR: se o único produtor pendente é a própria instrução → operando disponível.

### `UpdateDependencies(cdb, fu, ciclo)`
- Verifica `IsDependencyResolved()` no CDB para Qj/Qk.
- Quando pronto: aloca FU (`FindFreeFU`/`AllocateFreeFU`), inicia contagem, avança fase (EX ou MEM).
- LOAD/STORE: EX → `int_basic_alu` (cálculo endereço); MEM → `memory_access`.

### `UpdateCountdown(fu, ciclo)`
- Decrementa contagem regressiva. Ao chegar 0: libera FU, avança fase.
- LOAD/STORE: EX→MEM (contagem volta a -1, aguarda nova alocação de MEM).
- Demais + MEM: →WB.

### `ResolveDependency(rs_id, valor)`
Broadcast: quando produtor finaliza WR, resolve Qj/Qk das dependentes.

## `Thread` — Pipeline por Thread
### Construtores
```cpp
Thread(assembly, has_rob, num_rs={}, num_fu={}, dispatch_width=1, rob_capacity=30)
Thread(switch_instructions, assembly, has_rob, num_rs={}, num_fu={}, dispatch_width=1, rob_capacity=30)
```
- `num_fu`: 6 elementos `{memory_access, int_basic_alu, int_mult_div_alu, float_basic_alu, float_mult_div_alu, wr}`
- `fu.commit` é sempre = `dispatch_width` (commit width = dispatch width)
- `rob_capacity` só usado quando `has_rob=true`; senão = 1 (sem ROB)

### Atributos
`PC`, `instruction_table[]`, `rs`, `cdb`, `fu`, `rob[]`, `wb_buffer[]`, `pending_wb_buffer[]`, `state` (FREE/WAITING/BLOCKED), `unresolved_branch_pc`.

### `Issue(ciclo)`
- Seleciona grupo RS conforme tipo, tenta `AddIssue()` na primeira livre.
- Sucesso: `ciclo_issue = ciclo`, PC++, se BRANCH s/ ROB → `unresolved_branch_pc`.
- Se RS cheia ou thread bloqueada → false.

### `ExMem(ciclo)`
- Delega para `StartExOrMemPhase(ciclo)`.

### `StartExOrMemPhase(ciclo)`
- `CollectCandidatesToAdvance()`: coleta todas RS ocupadas.
- `SortCandidatesByPC()`: ordena por PC (prioridade à mais antiga).
- `TryAdvanceRS()`: chama `UpdateDependencies()`, registra início de EX/MEM.
- Instruções após `unresolved_branch_pc` não iniciam EX.
- STORE c/ ROB em MEM é ignorada (commit fará escrita).

### `Wr(ciclo)`
- Flush `pending_wb_buffer` → `wb_buffer`.
- `PerformWriteResult(ciclo)`.
- `DetectPhaseTransitions(ciclo)`.

### `PerformWriteResult(ciclo)` — Write Result (antigo Write-Back)
- Ordena `wb_buffer` por PC, processa `fu.wr` por ciclo.
- STORE c/ ROB: `ReleaseRSByRegister()` + `ReleaseRSByPC()`, sem WR nem CDB.
- LOAD/demais: `ciclo_WR = ciclo`, **`BroadcastCDB()`** (desaloca produtor + resolve dependentes), libera RS.
- BRANCH: `ReleaseRSByPC()`, não ocupa vaga WR.

### `BroadcastCDB(pc, dest, ciclo)`
- Percorre todos 6 grupos RS, resolve dependências de Qj/Qk que apontam para `(id, ciclo_inicio)`.
- Desaloca produtor do CDB via `DeallocateRS`.

### `ReleaseRSByRegister(dest, ciclo)`
- Libera a RS do registrador `dest` no CDB.

### `ReleaseRSByPC(grupo, pc, ciclo)`
- Libera a RS associada ao PC dentro de um grupo específico.

### `DetectPhaseTransitions(ciclo)`
- `CollectTransitionEvents()`: coleta eventos de fim de fase via `UpdateCountdown()`.
- `ProcessTransition()`: ordena por PC, fecha `ciclo_EX`/`ciclo_MEM`, move para `pending_wb_buffer`.

### `Commit(ciclo)` — Apenas com ROB
- Se `!has_rob`, retorna (thread sem ROB não commita).
- Processa `rob[]` em ordem via `commit_pointer`.
- STORE: simula latência MEM, depois commita.
- BRANCH: pronto quando EX terminou (`ciclo_EX.size() == 2`).
- Demais: `ciclo_WR != 0 && ciclo_WR < ciclo`.
- BRANCH s/ previsor trava commit no mesmo ciclo.

> **Nota:** `ReleaseRSByRegister()`, `ReleaseRSByPC()`, `SortCandidatesByPC()` e `SortEventsByPC()` foram refatoradas de métodos privados da `Thread` para funções `static` em `Thread.cpp`, removendo-as da interface da classe. O comportamento descrito nesta seção permanece inalterado.

## `Processor` — Orquestração
### `ExecuteCycle()`
```cpp
ExecuteExMemWr();  // ExMem → Wr → Commit (todas threads)
ExecuteIssue();    // despacho (ate dispatch_width)
current_cycle++;
```
### `ExecuteExMemWr()`
Para cada thread: `ExMem()` → `Wr()` → `Commit()` (Commit unificado: s/ ROB retorna imediatamente). Retorna true se todas finalizaram.

### `ExecuteIssue()` — Política de Despacho
- FINE_GRAINED: thread mantém prioridade até falhar ou encontrar BRANCH.
- SMT: round-robin a cada despacho bem-sucedido.
- BRANCH s/ previsor: interrompe despacho no mesmo ciclo.

## Multithreading
|          Modelo         |                                    Comportamento                                   |
|-------------------------|------------------------------------------------------------------------------------|
| `FINE_GRAINED` (0)      | Thread mantém prioridade até esgotar despacho. Alterna no próximo ciclo se falhou. |
| `SMT` (2)               | Round-robin entre threads a cada instrução despachada.                             |
| `COARSE_GRAINED` (1)    | Troca de contexto baseada em `switch_instructions`.                                |

## Dependências
- **RAW**: RS espera broadcast do WR para resolver Qj/Qk.
- **WAW**: CDB suporta múltiplos produtores pendentes. Desalocação por hash `(rs_id, ciclo_inicio)`.
- **WAR**: Prevenido lendo Qj/Qk **antes** de marcar destino em `AddIssue()`.

## Makefile Targets

|          Comando        |                         Descrição                         |
|-------------------------|-----------------------------------------------------------|
| `make`                  | Compila executável + testbenchs                           |
| `make test`             | Executa testbenchs (exibe apenas falhas)                  |
| `make simtest`          | Compara saída com `.expected` (exibe apenas diferenças)   |
| `make simtest-completo` | Mostra output completo de testbenchs + casos de simulação |
| `make simtest-update`   | Sobrescreve `.expected` com saída atual                   |
| `make clean`            | Remove build/ e executável                                |
| `make rebuild`          | clean + all                                               |

## Estrutura de Arquivos
```
├── Code/
│   ├── Makefile
│   ├── Main.cpp                     # Leitura config + simulação + impressão
│   ├── Processor.cpp                # Processor (orquestração multiciclo)
│   ├── Thread.cpp                   # Pipeline por thread
│   ├── ReservationStations.cpp      # ReservationStation individual
│   ├── Components.cpp               # Register, CDB, FU
│   ├── Instruction.cpp              # Instruction (parsing, tipo, latências)
│   ├── headers/                     # Arquivos .h de cabeçalho
│   │   ├── Components.h
│   │   ├── Instruction.h
│   │   ├── Processor.h
│   │   ├── ReservationStations.h
│   │   ├── SortUtils.h
│   │   └── Thread.h
│   ├── testbenchs/                  # Testes unitários (fontes)
│   │   ├── tb_Components.cpp
│   │   ├── tb_Instruction.cpp
│   │   ├── tb_ReservationStations.cpp
│   │   ├── tb_Thread.cpp
│   │   └── tb_Processor.cpp
│   ├── test-cases/
│   │   ├── inputs/                  # Arquivos de configuração .txt
│   │   └── expected/                # Saídas de referência .expected
│   └── build/                       # Build: .o, .d, executável, testbenchs
├── README.md
├── README-portuguese.md
├── FULL_DOCUMENTATION-portuguese.md
└── TODO.md
```
