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
num_rs      6 valores       load store int_basico int_mult_div float_basico float_mult_div
num_ufs     6 valores       acessar_memoria ula_int_basico ula_int_mult_div ula_float_basico ula_float_mult_div wr (commit = despacho)
latencias_ex  10 valores    NAO_EXISTE LOAD STORE BRANCH INT_BASICO INT_MUL INT_DIV FLOAT_BASICO FLOAT_MUL FLOAT_DIV
latencias_mem 2 valores     LOAD STORE
programa     instrucoes... END_PROG
```

## Pipeline (s/ ROB): `Issue → EX → MEM (LOAD/STORE) → WB → fim`
## Pipeline (c/ ROB):  `Issue → EX → MEM (LOAD/STORE) → WB → Commit`

## Hierarquia de Classes

```
_codigo/
├── Main.cpp                     (leitura + impressão)
├── Processador.cpp/.h           (orquestração multiciclo)
├── Thread.cpp/.h                (Issue, ExMem, Wr, Commit)
├── ReservationStations.cpp/.h   (addIssue, dep tracking, contagem)
├── Componentes.cpp/.h           (Registrador, CDB, UF)
├── Instrucao.cpp/.h             (parsing, tipo, latências)
└── testbenchs/                  (testes unitários)
```

## `Instrucao` — Parsing e Tipos
|      Tipo      |                             Exemplos                            | latEX default | latMEM default |
|----------------|-----------------------------------------------------------------|---------------|----------------|
| `LOAD`         | L.D, LW, LB, LH, LBU, LHU, L.S, LD, LWU, LL                     |        1      |        1       |
| `STORE`        | S.D, SW, SB, SH, S.S, SD, SC                                    |        1      |        1       |
| `INT_BASICO`   | ADD, SUB, ADDI, AND, OR, XOR, NOR, SLT, SLL, SRL, SRA, LUI, ... |        1      |        -       |
| `INT_MUL`      | MULT, MULTU, MUL, DMULT, DMULTU                                 |        4      |        -       |
| `INT_DIV`      | DIV, DIVU, DDIV, DDIVU                                          |        10     |        -       |
| `FLOAT_BASICO` | ADD.D, ADD.S, SUB.D, SUB.S                                      |        9      |        -       |
| `FLOAT_MUL`    | MUL.D, MUL.S                                                    |        14     |        -       |
| `FLOAT_DIV`    | DIV.D, DIV.S                                                    |        40     |        -       |
| `BRANCH`       | BEQ, BNE, BNEZ, BEQZ, BGTZ, BLTZ, J, JR, JAL, JALR, ...         |        1      |        -       |

- `identificaTipo()` mapeia mnemônico → `TipoInstrucao`.
- `defineAtributos()`: LOAD → dest=R1, K=R2; STORE → J=R1, K=R2; ADD → dest=R1, J=R2, K=R3; BRANCH → J e/ou K se forem registradores.

## `Registrador` — Rastreamento de Dependências
- Atributos: `tipo` ('R'/'F'/'Z'), `id`, `busy`, `RS_alocadas[]`, `tempo_inicio[]`, `tempo_fim[]`.
- `getRSatual()`: produtor pendente mais recente (último com `tempo_fim == -1`), usado para WAW.
- `alocarRS(rs_id, inicio)`: marca busy, adiciona produtor com `tempo_fim = -1`.
- `desalocarRS(rs_id, ciclo_inicio, ciclo_fim)`: usa **hash (rs_id, ciclo_inicio)** para desambiguar reuso do mesmo RS.
- `dependenciaResolvida(rs_id, ciclo_inicio)`: true se aquele produtor específico já terminou.

## `CDB` (Common Data Bus) — Struct
```cpp
struct CDB { vector<Registrador> R[32]; vector<Registrador> F[32]; };
```
Centraliza o tracking de **quem produz cada registrador**.

## `UF` / `UnidadesFuncionais`
Grupos: `acessar_memoria`, `ula_int_basico`, `ula_int_mult_div`, `ula_float_basico`, `ula_float_mult_div`. Cada UF: `{busy, RS_atual, tempo_alocacao[], RS_alocadas[]}`. `wr` e `commit` são inteiros (largura). `commit` = `largura_de_despacho` (implicitamente, não configurável via `num_ufs`).

## `RS` (Reservation Station) — Lógica Central
### `addIssue(instrucao, cdb, ciclo)`
1. Lê **Qj/Qk do CDB antes** de marcar o destino (WAR).
2. Se tag vazia → `Vj/Vk` = operando; senão → `Qj/Qk = {rs_id, ciclo_inicio}`.
3. Marca destino no CDB (`alocarRS`).
4. Trata WAR: se o único produtor pendente é a própria instrução → operando disponível.

### `atualizarDependencias(cdb, uf, ciclo)`
- Verifica `dependenciaResolvida()` no CDB para Qj/Qk.
- Quando pronto: aloca UF (`procuraUFlivre`), inicia contagem, avança fase (EX ou MEM).
- LOAD/STORE: EX → `ula_int_basico` (cálculo endereço); MEM → `acessar_memoria`.

### `atualizaContagem(uf, ciclo)`
- Decrementa contagem regressiva. Ao chegar 0: libera UF, avança fase.
- LOAD/STORE: EX→MEM (contagem volta a -1, aguarda nova alocação de MEM).
- Demais + MEM: →WB.

### `resolverDependencia(rs_id, valor)`
Broadcast: quando produtor finaliza WR, resolve Qj/Qk das dependentes.

## `Thread` — Pipeline por Thread
### Construtores
```cpp
Thread(assembly, tem_rob, num_rs={}, num_ufs={}, largura_despacho=1, capacidade_rob=30)
Thread(instrucoes_troca, assembly, tem_rob, num_rs={}, num_ufs={}, largura_despacho=1, capacidade_rob=30)
```
- `num_ufs`: 6 elementos `{acessar_mem, ula_int_basico, ula_int_mult_div, ula_float_basico, ula_float_mult_div, wr}`
- `uf.commit` é sempre = `largura_despacho` (commit width = dispatch width)
- `capacidade_rob` só usado quando `tem_rob=true`; senão = 1 (sem ROB)

### Atributos
`PC`, `tabela_de_instrucoes[]`, `rs`, `cdb`, `uf`, `rob[]`, `buffer_WB[]`, `buffer_WB_pendente[]`, `estado` (LIBERADA/ESPERA/BLOQUEADA), `pc_branch_nao_resolvido`.

### `Issue(ciclo)`
- Seleciona grupo RS conforme tipo, tenta `addIssue()` na primeira livre.
- Sucesso: `ciclo_issue = ciclo`, PC++, se BRANCH s/ ROB → `pc_branch_nao_resolvido`.
- Se RS cheia ou thread bloqueada → false.

### `ExMem(ciclo)`
- Delega para `iniciarFaseExOuMem(ciclo)`.

### `iniciarFaseExOuMem(ciclo)`
- `coletarCandidatasParaAvancar()`: coleta todas RS ocupadas.
- `ordenarCandidatasPorPC()`: ordena por PC (prioridade à mais antiga).
- `tentarAvancarRS()`: chama `atualizarDependencias()`, registra início de EX/MEM.
- Instruções após `pc_branch_nao_resolvido` não iniciam EX.
- STORE c/ ROB em MEM é ignorada (commit fará escrita).

### `Wr(ciclo)`
- Flush `buffer_WB_pendente` → `buffer_WB`.
- `realizarWriteResult(ciclo)`.
- `detectarTransicoesDeFase(ciclo)`.

### `realizarWriteResult(ciclo)` — Write Result (antigo Write-Back)
- Ordena `buffer_WB` por PC, processa `uf.wr` por ciclo.
- STORE c/ ROB: `liberarRSPorRegistrador()` + `liberarRSPorPC()`, sem WR nem CDB.
- LOAD/demais: `ciclo_WR = ciclo`, **`broadcastCDB()`** (desaloca produtor + resolve dependentes), libera RS.
- BRANCH: `liberarRSPorPC()`, não ocupa vaga WR.

### `broadcastCDB(pc, dest, ciclo)`
- Percorre todos 6 grupos RS, resolve dependências de Qj/Qk que apontam para `(id, ciclo_inicio)`.
- Desaloca produtor do CDB via `desalocarRS`.

### `liberarRSPorRegistrador(dest, ciclo)`
- Libera a RS do registrador `dest` no CDB.

### `liberarRSPorPC(grupo, pc, ciclo)`
- Libera a RS associada ao PC dentro de um grupo específico.

### `detectarTransicoesDeFase(ciclo)`
- `coletarEventosDeTransicao()`: coleta eventos de fim de fase via `atualizaContagem()`.
- `processarTransicao()`: ordena por PC, fecha `ciclo_EX`/`ciclo_MEM`, move para `buffer_WB_pendente`.

### `Commit(ciclo)` — Apenas com ROB
- Se `!tem_rob`, retorna (thread sem ROB não commita).
- Processa `rob[]` em ordem via `ponteiro_commit`.
- STORE: simula latência MEM, depois commita.
- BRANCH: pronto quando EX terminou (`ciclo_EX.size() == 2`).
- Demais: `ciclo_WR != 0 && ciclo_WR < ciclo`.
- BRANCH s/ previsor trava commit no mesmo ciclo.

## `Processador` — Orquestração
### `executarCiclo()`
```cpp
executarExMemWr();  // ExMem → Wr → Commit (todas threads)
executarIssue();    // despacho (ate largura_de_despacho)
ciclo_atual++;
```
### `executarExMemWr()`
Para cada thread: `ExMem()` → `Wr()` → `Commit()` (Commit unificado: s/ ROB retorna imediatamente). Retorna true se todas finalizaram.

### `executarIssue()` — Política de Despacho
- GRANULAÇÃO_FINA: thread mantém prioridade até falhar ou encontrar BRANCH.
- SMT: round-robin a cada despacho bem-sucedido.
- BRANCH s/ previsor: interrompe despacho no mesmo ciclo.

## Multithreading
|          Modelo         |                                    Comportamento                                   |
|-------------------------|------------------------------------------------------------------------------------|
| `GRANULACAO_FINA` (0)   | Thread mantém prioridade até esgotar despacho. Alterna no próximo ciclo se falhou. |
| `SMT` (2)               | Round-robin entre threads a cada instrução despachada.                             |
| `GRANULACAO_GROSSA` (1) | Troca de contexto baseada em `instrucoes_troca`.                                   |

## Dependências
- **RAW**: RS espera broadcast do WR para resolver Qj/Qk.
- **WAW**: CDB suporta múltiplos produtores pendentes. Desalocação por hash `(rs_id, ciclo_inicio)`.
- **WAR**: Prevenido lendo Qj/Qk **antes** de marcar destino em `addIssue()`.

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
├── _codigo/
│   ├── Main.cpp                     # Leitura config + simulação + impressão
│   ├── Processador.cpp/.h           # Orquestração multiciclo
│   ├── Thread.cpp/.h                # Pipeline por thread
│   ├── ReservationStations.cpp/.h   # RS individual
│   ├── Componentes.cpp/.h           # Registrador, CDB, UF
│   ├── Instrucao.cpp/.h             # Parsing e tipos de instrução
│   └── testbenchs/                  # Testes unitários (fontes)
│       ├── tb_Componentes.cpp
│       ├── tb_Instrucao.cpp
│       ├── tb_ReservationStations.cpp
│       ├── tb_Thread.cpp
│       └── tb_Processador.cpp
├── test_cases/
│   ├── inputs/                      # Arquivos de configuração .txt
│   └── expected/                    # Saídas de referência .expected
├── build/                           # Objetos .o e binários dos testbenchs
├── executavel                       # Binário principal
├── Makefile
└── context.md
```
