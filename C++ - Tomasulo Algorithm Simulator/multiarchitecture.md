# MULTIARCHITECTURE — Simulador Tomasulo Multi-Arquitetura

> Documento de acompanhamento da adaptação do simulador para suportar múltiplas
> arquiteturas (MIPS32, x86-Intel, ARM64, RISC-V).
> Marque os itens com `[x]` conforme forem concluídos.

---

## 1. Objetivo

Adaptar o simulador atual (monolítico, apenas MIPS) para ser **multi-arquitetura**,
utilizando o módulo reformulado em `Code/` (base) + `Code/Instruction/` (arquiteturas):

- `Instruction` base **abstrata** com Template Method (`Parse()` chama
  `SplitInstruction` → `IdentifyType` → `NormalizeInstruction` → `SetAttributes` → `SetLatencies`).
- Subclasses por arquitetura: `InstructionMips32`, `InstructionX86Intel`,
  `InstructionArm64`, `InstructionRiscV`.
- `InstructionFactory::ParseTrace()` gera `std::vector<std::unique_ptr<Instruction>>`
  genérico a partir das linhas de assembly e da arquitetura escolhida.

---

## 2. Contexto atual (código legado)

| Arquivo | Papel hoje |
|---|---|
| `Code/headers/Instruction.h` + `Code/Instruction.cpp` | Classe `Instruction` monolítica com parse de MIPS embutido; enums `INSTRUCTION_TYPE` e `INSTRUCTION_PHASE_TOMASULO` |
| `Code/headers/ReservationStations.h` + `Code/ReservationStations.cpp` | RS hardwired em **1 destino + J/K** (`Vj/Vk`, `Qj/Qk`, `GetDestRegister/GetJ/GetK`) |
| `Code/headers/Thread.h` + `Code/Thread.cpp` | Guarda `Instruction` **por valor** em `TABLE_ROW::instruction`, `rob` e `instruction_table`; monta tabela com `Instruction(i++, instr)` |
| `Code/headers/Components.h` + `Code/Components.cpp` | `Register` (só aceita `R`/`F`/vazio em `ParseType`); `struct CDB { std::vector<Register> R; std::vector<Register> F; }` |
| `Code/Main.cpp` | Lê config; `Instruction::base_ex_latencies/mem` são setados globalmente; imprime CDB acessando `cdb.F`/`cdb.R` |
| `Code/Makefile` | `SRCS = $(wildcard ./*.cpp)` — só compila arquivos na raiz de `Code/` (não compila `Instruction/`) |
| `Code/testbenchs/tb_Instruction.cpp` | Testes baseados no construtor antigo `Instruction(pos, string)` |

---

## 3. Decisões de projeto (registro)

| # | Decisão | Status |
|---|---|---|
| D1 | **Herança + Template Method**: `Instruction` abstrata com 4 métodos virtuais puros; parse delegado às subclasses | ✅ aprovada |
| D2 | **Factory**: `InstructionFactory::ParseTrace(lines, arch)` cria o array genérico com `unique_ptr` | ✅ aprovada |
| D3 | **Armazenamento via `std::shared_ptr`**: `TABLE_ROW::instruction`, `rob` e `ReservationStation::current_instruction` passam a guardar `shared_ptr<Instruction>` (instrução abstrata não pode ser por valor; um único objeto compartilhado entre tabela/RS/ROB evita clone). A **Factory continua retornando `unique_ptr`** — a Thread converte para `shared_ptr` ao guardar na tabela (`shared_ptr<Instruction>(std::move(p))`, 1 conversão por instrução) | ✅ aprovada (refinada: shared_ptr confirmado em vez de unique_ptr + ponteiro cru — elimina invariante oculta de "tabela imutável" e risco de dangling) |
| D4 | **RS generalizada para N operandos**: substituir `Vj/Vk/Qj/Qk` por vetores construídos a partir de `GetSourceRegisters()`; alocação no CDB itera `GetDestRegisters()` (x86 pode ter 2 destinos: reg + EFLAGS) | ✅ aprovada |
| D5 | **CDB vira classe por arquitetura**: `class CDB` com construtor `CDB(Architecture)` que monta o banco de registradores (32R/32F + flags quando a arquitetura tiver), expondo acesso central via `Get(const Register&)` — menos chance de erro nos módulos superiores | ✅ aprovada |
| D6 | **Flags modeladas como registradores**: `Register` passa a aceitar tipos `E` (EFLAGS) e `C` (CPSR); flags participam de dependências via CDB (resolvido via tabela estática — ver D12) | ✅ aprovada |
| D7 | **Nova chave `arquitetura` na config**: propagada `Main → Processor → Thread → InstructionFactory` | ✅ aprovada |
| D8 | **Default = MIPS32** para os test-cases atuais continuarem válidos | ✅ aprovada |
| D9 | ~~Renomear `INSTRUCTION_PHASE_TOMASULO` → `INSTRUCTION_PHASE`~~ | ❌ **revogada** — ver D15 |
| D10 | **Preservar normalização MIPS atual**: labels de branch permanecem em minúsculo (ex.: `BNEZ   R3, loop`), para não regenerar todos os `.expected` | ✅ aprovada |
| D11 | `std::abort()` em `InstructionFactory.h` requer `#include <cstdlib>` explícito | ✅ aprovada |
| D12 | **`Register` com tabela estática nome → (tipo, id)**: mapeamento configurado por arquitetura; a validação deixa de ser "primeira letra + sufixo numérico" e passa a ser "o registrador existe no banco da arquitetura". Elimina heurísticas e valores inválidos por construção | ✅ aprovada |
| D13 | **Ordem de execução da Fase 2**: Bloco A primeiro (swap do núcleo + shared_ptr + RS N-operandos + regressão MIPS), depois CDB classe + Register (pré-requisito das outras arquiteturas) | ✅ aprovada |
| D14 | **Módulo multi-arquitetura em pasta separada**: as subclasses por arquitetura ficam em `Code/Instruction/` (`.cpp`) + `Code/Instruction/headers/` (`.h`) — novas arquiteturas entram lá; **revisado na Fase 2/Bloco A**: a base `Instruction` e a `InstructionFactory` moram junto do núcleo (`Code/Instruction.cpp` + `Code/headers/Instruction.h` + `Code/headers/InstructionFactory.h`); os legados `headers/Instruction.h` + `Instruction.cpp` foram **sobrescritos pelo conteúdo novo** (não deletados) e os includes do projeto passam a apontar para os caminhos novos | ✅ aprovada (revisada na Fase 2: layout final definido pelo usuário) |
| D15 | **Manter o nome `INSTRUCTION_PHASE_TOMASULO`**: o header novo passa a definir esse nome (em vez de `INSTRUCTION_PHASE`); nada é renomeado no projeto. Facilita diferenciar do futuro `INSTRUCTION_PHASE_CLASSIC` (IF→WB) do `PROCESSOR_TYPE::IN_ORDER` | ✅ aprovada (substitui D9) |

---

## 4. Fase 1 — Correção do módulo novo (`Code/Instruction/`)

> **Nota de layout:** nesta fase o módulo inteiro (base + factory + arquiteturas) ainda
> vivia em `Code/Instruction/`; na Fase 2/Bloco A o usuário definiu o layout final —
> base e factory movidos para `Code/` + `Code/headers/` (ver D14 revisada e §5.1).
> Os caminhos abaixo se referem ao layout da época.

> O módulo **não compila** hoje. Corrigir antes de qualquer integração.

### 4.1 Restos de `[cite: N]` (código colado de IA)
- [x] `Instruction/Instruction.cpp` — remover `[cite: 2]` do fim das linhas (constructor, `Parse`, getters/setters, `SetLatencies` e arrays estáticos)
- [x] `Instruction/InstructionMips32.cpp` — remover `[cite: 2]` do fim das linhas (arrays de opcodes, `IdentifyType`, `SplitInstruction`, `NormalizeInstruction`, `SetAttributes`)
- [x] Conferir que `InstructionArm64.cpp`, `InstructionRiscV.cpp` e `InstructionX86Intel.cpp` **não** têm esses resíduos (já compilam)

### 4.2 Includes
- [x] Estrutura de pastas definida pelo usuário: `Code/Instruction/` (`.cpp`) + `Code/Instruction/headers/` (`.h`) — os `#include "headers/X.h"` dos `.cpp` agora resolvem corretamente
- [x] `Instruction/headers/Instruction.h` inclui `"Components.h"` — compilação exige `-I Code/headers` (registrado; no Makefile integrado da Fase 2 isso precisa ser resolvido)

### 4.3 Headers faltantes
- [x] `Instruction/headers/InstructionFactory.h` — adicionado `#include <cstdlib>` (usa `std::abort()`)

### 4.4 Comportamento MIPS (D10)
- [x] `InstructionMips32::NormalizeInstruction` — labels de branch em **minúsculo** restaurados (helper `IsRegister()` + mutação in-place dos tokens); verificado com `BNEZ R3, foo` → `BNEZ   R3, foo`
- [x] Bug extra encontrado e corrigido: labels eram minúsculados numa **cópia** (`token`) e `SetAttributes` lia `tokens` original em UPPERCASE, causando `Register("FOO")` → abort. Corrigido mutando `tokens[i]` in-place

### 4.5 Verificação da Fase 1
- [x] `g++ -std=c++17 -fsyntax-only -Wall -Wextra -I Code/headers -I Code/Instruction/headers` em todos os 5 `.cpp` → **zero erros/warnings**
- [x] Mini-main funcional em `/tmp/opencode` com `InstructionFactory::ParseTrace` → MIPS (8 instruções) com tipos/latências/vetores corretos e output idêntico ao legado; exit 0

> **Achado da Fase 1 (bloqueia Fase 2):** `Register::ParseType` só aceita `R`/`F`/vazio.
> Além de `EFLAGS`/`CPSR` (D6), os registradores das outras arquiteturas **também abortam**:
> ARM64 (`X0`, `W0`, `D0`, `S0`, `V0`...), RISC-V (`x1`, `f1`...), x86 (`EAX`, `EBX`...).
> A generalização do `Register` (D6) precisa cobrir a nomenclatura de TODAS as arquiteturas,
> não só as flags. Ver item 5.5.

---

## 5. Fase 2 — Integração do módulo e adaptação do núcleo

### 5.1 Layout de arquivos (D14)
**Layout final (definido pelo usuário):** a base e a factory ficam junto do núcleo; `Code/Instruction/` recebe **apenas** as subclasses por arquitetura.
- [x] `Code/headers/Instruction.h` + `Code/Instruction.cpp` — base `Instruction` (abstrata, Template Method) — sobrescreveram os legados homônimos (não foram deletados)
- [x] `Code/headers/InstructionFactory.h` — `InstructionFactory` + enum `Architecture` + `#include <cstdlib>` (D11)
- [x] `Code/Instruction/headers/Instruction{Mips32,X86Intel,Arm64,RiscV}.h` — subclasses (incluem `"../../headers/Instruction.h"`)
- [x] `Code/Instruction/Instruction{Mips32,X86Intel,Arm64,RiscV}.cpp` — implementações por arquitetura (incluem `"headers/X.h"`)
- [x] Makefile: `SRCS = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/Instruction/*.cpp)` — com o layout final o wildcard pega só as 4 arquiteturas; `CXXFLAGS += -I$(SRCDIR)/headers` mantido (defensivo — todos os includes atuais já resolvem por caminho relativo); regra de compilação ganhou `@mkdir -p $(dir $@)` para criar `build/Instruction/`
- [x] Legados `headers/Instruction.h` e `Instruction.cpp` **sobrescritos** pelo conteúdo novo (substituídos)
- [x] Atualizar includes do projeto para os caminhos novos:
  - [x] `Main.cpp:3-4`, `Thread.cpp:3` → `"headers/Instruction.h"` / `"headers/InstructionFactory.h"`
  - [x] `headers/Processor.h`, `headers/ReservationStations.h`, `headers/Thread.h` → `"Instruction.h"` (mesma pasta; RS deixou de incluir diretamente, vem via `ReservationStations.h`)
  - [x] `testbenchs/tb_*.cpp` → `"../headers/Instruction.h"` + `"../headers/InstructionFactory.h"` (tb_Instruction também inclui `"../Instruction/headers/InstructionMips32.h"`)

### 5.2 Enum de fase (D15)
- [x] `headers/Instruction.h` — renomear o enum `INSTRUCTION_PHASE` do header novo para `INSTRUCTION_PHASE_TOMASULO` (única mudança; nada mais no projeto muda de nome)
- [x] Confirmar que `ReservationStations.h/.cpp`, `Thread.cpp` e `tb_ReservationStations.cpp` seguem usando `INSTRUCTION_PHASE_TOMASULO` (já usam — zero renomes)
- [x] Registrar no TODO.md: criar `INSTRUCTION_PHASE_CLASSIC` (IF→WB) quando implementar `PROCESSOR_TYPE::IN_ORDER`

### 5.3 Armazenamento com `shared_ptr` (D3)
- [x] `headers/Thread.h` — `TABLE_ROW::instruction` vira `std::shared_ptr<Instruction>`
- [x] `headers/Thread.h` — `std::vector<Instruction> rob` → `std::vector<std::shared_ptr<Instruction>>`
- [x] `headers/ReservationStations.h` — `Instruction current_instruction` → `std::shared_ptr<Instruction>` (default `nullptr`; ver risco no §7)
- [x] Getters mantêm a assinatura antiga (`const Instruction& GetCurrentInstruction() const`), desreferenciando o ponteiro — minimiza mudanças nos consumidores
- [x] `Thread::Issue()` — `rob.push_back(instruction)` vira `rob.push_back(instruction_table[pos].instruction)` (compartilha o mesmo objeto)
- [x] `ReservationStation::SetupNewIssue()` — recebe `const std::shared_ptr<Instruction>&`
- [x] `Thread.cpp` linha 131 — montar a tabela via `InstructionFactory::ParseTrace(assembly, Architecture::MIPS_32)`, convertendo `unique_ptr` → `shared_ptr` ao guardar (`std::shared_ptr<Instruction>(std::move(p))`)
- [x] Verificar se `Thread` é copiada em algum fluxo (`Processor::threads.push_back` usa move; `shared_ptr` torna cópia segura de qualquer forma)

### 5.4 Generalizar RS para N operandos (D4)
- [x] `ReservationStations.h` — substituir `Vj`, `Vk`, `Qj`, `Qk` por:
  - `std::vector<Register> V;`
  - `std::vector<std::pair<std::string, int>> Q;`
- [x] `ReservationStation::AddIssue()` — construir `V`/`Q` com `for (const Register& src : instruction.GetSourceRegisters())`
- [x] `ReadSourceOperand(char, Register, CDB)` → `ReadSourceOperand(size_t idx, const Register&, CDB&)` (loop sobre fontes; `InvalidRegister` pula)
- [x] `CheckDependency(char, CDB)` → `CheckDependency(size_t idx, CDB&)` (loop)
- [x] `AllocateDestInCDB(const Register&, ...)` → loop sobre `GetDestRegisters()` (x86: dest + EFLAGS = 2 alocações)
- [x] `Thread::BroadcastOnRSAndCDB(const Register&, ...)` → mantida com 1 `Register`; `WriteResultOnComponents` chama em loop sobre `GetDestRegisters()` (escolha de implementação: broadcast um destino por vez)
- [x] `Thread::WriteResultOnComponents()` — iterar destinos na desalocação do CDB
- [x] `GetQj()/GetQk()` (usados em prints/testes) — mantidos como aliases de `Q[0]/Q[1]` (retorno por valor; `""` se o operando não existir)
- [x] Revisar `ReleaseRS`, `ReleaseRSStoreWithROB`, `ResolveDependency` para a nova API (ResolveDependency itera todos os `Q[i]`; STORE continua com dado em `Q[0]` e endereço em `Q.back()`, igual ao modelo antigo)

### 5.5 CDB como classe (D5/D6)
- [ ] `headers/Components.h` — `struct CDB` → `class CDB` com:
  - [ ] membros privados: `std::vector<Register> int_regs; std::vector<Register> float_regs; std::vector<Register> flags_regs;`
  - [ ] `explicit CDB(Architecture arch)` — cria o banco conforme a arquitetura (32 R + 32 F sempre; flags se x86/ARM)
  - [ ] `Register& Get(const Register& reg)` / `const Register& Get(const Register& reg) const` — resolve tipo (`R`/`F`/`E`/`C`) e id; aborta se fora do banco
  - [ ] getters para impressão: `const std::vector<Register>& GetRegisters(char type) const` (ex.: `GetRegisters('R')`)
- [ ] `Register` — **tabela estática de nomes por arquitetura (D12)**, substituindo a heurística atual (`ParseType` por primeira letra + `ParseId` por sufixo numérico):
  - [ ] tabela nome → (tipo, id): `EAX→('R',0)`, `EBX→('R',1)`, `XMM0→('F',0)`, `EFLAGS→('E',0)`, `CPSR→('C',0)`, `X0→('R',0)` (ARM), `x1→('R',1)`, `f2→('F',2)` (RISC-V), etc.
  - [ ] configuração por arquitetura (construtor do CDB ou `Register::SetArchitecture`); validação vira "o nome existe na tabela da arquitetura" — sem heurísticas, sem ids inválidos por construção
  - [ ] flags `E`/`C` entram na mesma tabela (D6)
  - [ ] manter compatibilidade MIPS (`R0..R31`, `F0..F31` — forma atual também passa pela tabela)
- [ ] Substituir acessos diretos:
  - [ ] `Thread.cpp:155-156` — criação do banco (movida para o construtor da CDB)
  - [ ] `Thread.cpp:425` — `regs = (dest.GetType() == 'F') ? cdb.F : cdb.R` → `cdb.Get(dest)` (e iterar destinos)
  - [ ] `ReservationStations.cpp:160-161, 182-183, 223-224` → `cdb.Get(reg)`
  - [ ] `Main.cpp:429-437` — impressão via `GetRegisters('R')/GetRegisters('F')` + novo grupo "flags" quando existir
- [ ] `tb_ReservationStations.cpp` / `tb_Thread.cpp` — atualizar acesso direto `cdb.F[i]`/`cdb.R[i]` para a nova API
- [ ] **Arquitetura da CDB**: decidir onde ela "vive" — `Thread` recebe `Architecture` no construtor e passa ao construtor do CDB

### 5.6 Fio da arquitetura (D7/D8)
- [ ] `Main.cpp` — nova chave `arquitetura` no `ReadConfig()` (0=MIPS32, 1=X86_INTEL, 2=ARM_64, 3=RISC_V; default 0)
- [ ] `Main.cpp` — campo `Architecture arch` em `CONFIG`; imprimir no `PrintConfig()`
- [ ] `Main.cpp` — propagar `cfg.arch` para `Processor`
- [ ] `headers/Processor.h` / `Processor.cpp` — construtor e `InitializeThreads()` recebem `Architecture`
- [ ] `headers/Thread.h` / `Thread.cpp` — construtor recebe `Architecture`; `InitializeComponents()` cria o CDB por arquitetura; a tabela é montada com `InstructionFactory::ParseTrace(assembly, arch)`
- [ ] `Thread.cpp:131` — remover loop manual de `Instruction(i++, instr)` (substituído pela Factory)
- [ ] `tb_Thread.cpp` — atualizar chamadas do construtor da Thread (novo parâmetro)

### 5.7 Verificação da Fase 2
- [x] `make` compila sem erros nem warnings novos (`-Wall -Wextra`)
- [x] `make test` → todos os testbenchs passam (420/420)
- [x] `make simtest` → todos os casos atuais (MIPS default) seguem OK (9/9) **sem** regenerar `.expected`
- [x] Re-verificação pós-reorganização de arquivos (Bloco A): `make rebuild` (artefatos antigos de `build/` foram descartados) + `make test` + `make simtest` → 420/420 e 9/9
- [ ] Teste manual: rodar um input `tipo 1` com `arquitetura 0` e conferir saída idêntica à anterior (fica pendente até o Bloco C — chave de config; o `simtest` já cobre a regressão do default)

---

## 6. Fase 3 — Testes multi-arquitetura

### 6.1 Testbenchs
- [ ] Reescrever `testbenchs/tb_Instruction.cpp`:
  - [ ] usar `InstructionFactory::ParseTrace` com as 4 arquiteturas
  - [ ] novos getters `GetDestRegisters()`/`GetSourceRegisters()` (vetores)
  - [ ] casos MIPS idênticos aos atuais (garante regressão)
  - [ ] casos x86: `MOV` load/store, `ADD` com EFLAGS como 2º destino
  - [ ] casos ARM64: `LDR`/`STR`, `ADDS` com CPSR
  - [ ] casos RISC-V: `LW`/`SW`, `FADD.D`, `JAL`
- [ ] Atualizar `testbenchs/tb_ReservationStations.cpp` para RS com N operandos
- [ ] Atualizar `testbenchs/tb_Thread.cpp` para CDB classe + construtor com arquitetura
- [ ] Novo `testbenchs/tb_InstructionFactory.cpp` (se não for coberto no tb_Instruction) — adicionar ao `TB_NAMES` do Makefile

### 6.2 Casos de simulação
- [ ] Criar `test-cases/inputs/sim-x86-01.txt` (arquitetura 1, com `ADD EAX, EBX` etc.)
- [ ] Criar `test-cases/inputs/sim-arm-01.txt` (arquitetura 2, com `ADDS`/`LDR`/`STR`)
- [ ] Criar `test-cases/inputs/sim-riscv-01.txt` (arquitetura 3, com `FADD.D`/`JAL`)
- [ ] Gerar `.expected` para os novos casos (`make simtest` gera referência quando ausente)
- [ ] Conferir dependências de flags (EFLAGS/CPSR) funcionando no CDB (tabela com EX atrasado)

### 6.3 Documentação
- [ ] Atualizar `VERIFICATION.md` com os resultados dos novos testes
- [ ] Atualizar `README.md`/`README-portuguese.md` (nova chave `arquitetura` e opcodes suportados)
- [ ] Atualizar `FULL_DOCUMENTATION-portuguese.md` se necessário
- [ ] Atualizar `TODO.md` (marcar como feito ou mover itens relacionados)

### 6.4 Comandos de verificação (atalho)
```bash
make rebuild          # compila tudo do zero
make test             # testbenchs unitários
make simtest          # casos de simulação vs .expected
make simtest-update   # regenera .expected (SOMENTE quando a mudança é intencional)
./build/executable < test-cases/inputs/sim-uma_thread-sem_spec-01.txt
```

---

## 7. Riscos e observações

- **`Register` só entende `R`/`F`**: além das flags (D6/D12), a nomenclatura de ARM64 (`X0`/`D0`...), RISC-V (`x1`/`f1`...) e x86 (`EAX`...) aborta no parse hoje — a tabela estática do `Register`/CDB é **pré-requisito** para testar as arquiteturas além de MIPS (verificado na Fase 1).
- **Cópias de `Thread`**: `std::vector<Thread> threads` usa move na inicialização; com `shared_ptr` no `rob`/tabela/RS, cópias acidentais continuam seguras. A CDB classe (Fase 2, Bloco B) também precisa ser copiável (ou usar ponteiro).
- **`Instruction` com `position == -1`**: a RS constrói `current_instruction` default; com `shared_ptr`, usar `nullptr` como "sem instrução" e ajustar guardas (`IsBusy()` já evita acesso).
- **X86 `MOV`**: hoje é sempre `LOAD` — decisão pendente: detectar memória `[...]` no operando 2 para diferenciar `MOV reg,reg` (INT_BASIC) de `MOV reg,[mem]` (LOAD). Opcode artificial `MOV_STORE` deve ser revisado.
- **ARM64 `LDR X0, [X1, #8]`**: `SplitInstruction` já trata `[`, `]` e `#`; validar `SetAttributes` com deslocamento (tokens extras).
- **RISC-V lowercases tudo** na normalização: output em minúsculo é esperado (diferente do MIPS que é UPPERCASE).
- **Latências**: `base_ex_latencies`/`base_mem_latencies` estáticos permanecem iguais aos atuais (compatibilidade com config `latencias_ex`/`latencias_mem`).
- **Compatibilidade de config**: toda chave nova precisa ser opcional (default = comportamento atual) para não quebrar os inputs existentes.
- **`INSTRUCTION_PHASE_TOMASULO` único**: com a D15, `Code/headers/Instruction.h` define esse nome e o legado homônimo foi sobrescrito pelo conteúdo novo — não há mais duas definições (o swap do header foi atômico com a substituição do legado).
