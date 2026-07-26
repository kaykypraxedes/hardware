# Tomasulo Algorithm Simulator

Cycle-accurate simulator of Out-of-Order processors based on the **Tomasulo Algorithm**, written in C++.

**Developer:** Kayky Moreira Praxedes

---

## What is the Tomasulo Algorithm?

The Tomasulo Algorithm is a hardware technique for dynamic instruction scheduling that enables out-of-order execution without creating false dependencies. Unlike In-Order processors, which execute instructions strictly in program order, Tomasulo uses **register renaming** and **reservation stations** to track real dependencies (RAW) and ignore name dependencies (WAR, WAW).

### Fundamental properties
- Register renaming eliminates WAW and WAR hazards
- Reservation stations decouple instruction issue from execution
- Common Data Bus (CDB) broadcasts results to all units

---

## Features

- **[In-Order / Tomasulo w/o ROB / w/ ROB]** — Three selectable execution modes: In-Order pipeline, Tomasulo without ROB (Reorder Buffer), and Tomasulo with ROB (in-order commit).
- **[Multithreading]** — Three models: fine-grained (switches every cycle), coarse-grained (switches every N instructions), and SMT (simultaneous issue in round-robin).
- **[Superscalar]** — Configurable dispatch width, allowing multiple instructions per cycle.
- **[Branch Predictor]** — Toggleable predictor; unresolved branches stall dispatch.
- **[Customizable Latencies]** — Execution and memory latencies configurable per instruction type via standard input.

---

## Project Structure

```
Code/
├── Main.cpp                     # Config reading + simulation + output
├── Processador.cpp/.h           # Multi-cycle orchestration
├── Thread.cpp/.h                # Per-thread pipeline (Issue, EX, MEM, WB, Commit)
├── ReservationStations.cpp/.h   # Individual reservation station
├── Componentes.cpp/.h           # Register, CDB, Functional Unit
├── Instrucao.cpp/.h             # Instruction parsing and types
└── test-cases/
    ├── inputs/                  # .txt config files
    └── expected/                # Reference .expected outputs
```

---

## Modules

### `Instrucao`

Parses instructions from mnemonics, identifies the type (LOAD, STORE, INT_BASIC, INT_MUL, INT_DIV, FLOAT_BASIC, FLOAT_MUL, FLOAT_DIV, BRANCH), and extracts operands (destination, source registers, and immediates) with default latencies per type.

### `Componentes`

Defines the three core structures: **Register** (tracks busy and pending producers via CDB), **Common Data Bus (CDB)** (centralized producer tracking per register), and **Functional Unit (UF)** (manages busy state, allocation, and latency countdown).

### `ReservationStations`

Implements the algorithm's core logic: `addIssue()` allocates the station while handling WAR (reads Qj/Qk before marking the destination), `atualizarDependencias()` manages pipeline phase transitions (EX, MEM), and `resolverDependencia()` broadcasts results to free dependent instructions.

### `Thread`

Manages a thread's full pipeline: Issue (dispatch to reservation station), ExMem (execution and memory access), Wr (CDB broadcast), and Commit (ROB mode only, ensures in-order completion). Supports context switching according to the multithreading model.

### `Processador`

Orchestrates multi-cycle execution: each cycle runs `ExMem -> Wr -> Commit` across all threads, then `Issue` (dispatch), applying the chosen scheduling policy (round-robin, fixed priority, or count-based switching).

---

## Tests

1. **Unit testbenchs** — Validate Componentes, Instrucao, ReservationStations, Thread, and Processador individually.
2. **Comparison simulation** — The simulator's output is compared against `.expected` files to detect regressions.

Compile with `make` and run tests with `make test`.

---

## Usage / Configuration

```bash
make
cd Code && ./executable < input_file.txt
```

The input file defines the processor type, thread count, dispatch width, number of reservation stations and functional units, latencies, and the assembly program. See `Code/test-cases/inputs/` for complete examples.
