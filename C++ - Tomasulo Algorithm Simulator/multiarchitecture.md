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
| D5 | ~~**CDB vira classe por arquitetura**~~: `class CDB` com construtor `CDB(Architecture)` que monta o banco de registradores (32R/32F + flags quando a arquitetura tiver), expondo acesso central via `Get(const Register&)` — menos chance de erro nos módulos superiores | ✅ aprovada → **substituída pela D20** (CDB continua `struct` — agora vetor único de registradores físicos; acesso via `GetReg`) |
| D6 | **Flags modeladas como registradores**: `Register` passa a aceitar tipos `E` (EFLAGS) e `C` (CPSR); flags participam de dependências via CDB. **Revisada por D20/D21:** banco único `'G'` com `EFLAGS→('G',0)` e `CPSR→('G',1)` (nunca coexistem na mesma arquitetura) | ✅ aprovada (revisada) |
| D7 | **Nova chave `arquitetura` na config**: propagada `Main → Processor → Thread → InstructionFactory` | ✅ aprovada |
| D8 | **Default = MIPS32** para os test-cases atuais continuarem válidos | ✅ aprovada |
| D9 | ~~Renomear `INSTRUCTION_PHASE_TOMASULO` → `INSTRUCTION_PHASE`~~ | ❌ **revogada** — ver D15 |
| D10 | **Preservar normalização MIPS atual**: labels de branch permanecem em minúsculo (ex.: `BNEZ   R3, loop`), para não regenerar todos os `.expected` | ✅ aprovada |
| D11 | `std::abort()` em `InstructionFactory.h` requer `#include <cstdlib>` explícito | ✅ aprovada |
| D12 | **`Register` com tabela estática nome → (tipo, id)**: mapeamento configurado por arquitetura; a validação deixa de ser "primeira letra + sufixo numérico" e passa a ser "o registrador existe no banco da arquitetura". Elimina heurísticas e valores inválidos por construção | ✅ aprovada |
| D13 | **Ordem de execução da Fase 2**: Bloco A primeiro (swap do núcleo + shared_ptr + RS N-operandos + regressão MIPS), depois CDB/Register universal (pré-requisito das outras arquiteturas) | ✅ aprovada |
| D14 | **Módulo multi-arquitetura em pasta separada**: as subclasses por arquitetura ficam em `Code/Instruction/` (`.cpp`) + `Code/Instruction/headers/` (`.h`) — novas arquiteturas entram lá; **revisado na Fase 2/Bloco A**: a base `Instruction` e a `InstructionFactory` moram junto do núcleo (`Code/Instruction.cpp` + `Code/headers/Instruction.h` + `Code/headers/InstructionFactory.h`); os legados `headers/Instruction.h` + `Instruction.cpp` foram **sobrescritos pelo conteúdo novo** (não deletados) e os includes do projeto passam a apontar para os caminhos novos | ✅ aprovada (revisada na Fase 2: layout final definido pelo usuário) |
| D15 | **Manter o nome `INSTRUCTION_PHASE_TOMASULO`**: o header novo passa a definir esse nome (em vez de `INSTRUCTION_PHASE`); nada é renomeado no projeto. Facilita diferenciar do futuro `INSTRUCTION_PHASE_CLASSIC` (IF→WB) do `PROCESSOR_TYPE::IN_ORDER` | ✅ aprovada (substitui D9) |
| D16 | **`Register` com assinatura simplificada**: construtores `Register()` (vazio `'Z'`/id `-1`) e `Register(char type, int id)` — **sem parser de string/heurística de primeira letra**. A validação de nomes sai do Register: no parse, a tabela da própria subclasse aborta se o nome não existir (única validação de nome do sistema); no banco, `GetReg(cdb, reg)` (D20) aborta **só** se o id estiver fora (a classe não é validada — combinações classe/id erradas são bugs de construção pegos pelo parse). Remove o `num_registers` global. **Refina a D12** (a tabela não vive dentro do Register — ver D17) | ✅ aprovada |
| D17 | **Bancos de registradores por arquitetura via métodos estáticos das subclasses**: cada subclasse expõe `static CDB MakeCDB()` preenchendo o CDB **por faixas de ids via `fillCDB`** (a tabela `nome → (classe, id físico global)` é usada no parse, não no `MakeCDB`; MIPS/RISC-V gerados por loop, x86/ARM64 com faixas sobrepostas para os aliases). O dispatch por `ARCHITECTURE` fica na `InstructionFactory` (que já inclui as 4 subclasses). **Components/CDB não conhecem arquitetura nem subclasses** — a `Thread` recebe o CDB pronto de `InstructionFactory::MakeCDB(arch)` | ✅ aprovada |
| D18 | **Aliases compartilham o mesmo id físico global** (revisada com D20): `RAX/EAX/AX/AL/AH` têm **o mesmo id** com classes diferentes (`('L',0)/('R',0)/('W',0)/('B',0)`); como o CDB indexa por id (um slot = um registrador físico), a dependência entre aliases é detectada **sem nenhuma lógica** — sem arestas, sem varredura do lado da leitura, sem campo `aliases`. Como x86-64/AArch64 zeram os 32 bits superiores ao escrever um registrador de 32 bits, a escrita de `EAX` define `RAX` inteiro → o esquema "mais recente vence" é **exato** para `EAX/RAX` e `W/X`; para 8/16-bit (`AX/AL/AH`) a escrita é verdadeiramente parcial e a limitação documentada em §5.8 se aplica | ✅ aprovada (revisada) |
| D19 | **`MUL`/`IMUL`/`DIV` do x86 com 2 destinos**: no `SetAttributes` do x86, `MUL RBX` escreve `RDX:RAX` (produto de 128 bits) → destinos implícitos `{RAX, RDX}` + `EFLAGS`, aproveitando a máquina de multi-destinos (D4). Validação: conferir o mapeamento no `IdentifyType`/`NormalizeInstruction` do x86 | ✅ aprovada |
| D20 | **CDB como vetor único de registradores físicos** (substitui a D5): o `id` do `Register` é o **id físico global** (um slot = um registrador físico; aliases compartilham o id — ver D18). `struct CDB` = `{ std::vector<Register> registers; std::vector<CDB_BANK> print_banks; }`; `GetReg(cdb, reg)` = `registers[reg.GetId()]` com bounds check (id < 0 ou ≥ size → `abort`) e **nenhuma checagem de classe**. A classe (`B/W/R/L/F/S/V/G`) é metadado puro: parse, largura semântica e impressão. Layout físico universal: ids 0-31 inteiros, 32-63 ponto flutuante, 64-79 vetorial, 80 flags. O núcleo **nunca testa `'F'`/`'R'`** — os 4 pontos de acesso (`ReservationStations.cpp:167-169, 192-193, 231-233`, `Thread.cpp:432`) usam `GetReg`; a impressão (`Main.cpp:428-434`) itera `print_banks` na ordem F,R,S,L,V,W,B,G | ✅ aprovada |
| D21 | **Conjunto completo de registradores** (fidelidade máxima): `AX/AL/AH` incluídos no x86 (int16/int8 — escritas verdadeiramente parciais, limitação §5.8); `XMM0-15` → float128 (`'V'`); RISC-V assumido **rv64** (`x0-31` → int64, `f0-31` → float32); flags `EFLAGS`/`CPSR` → banco único `'G'`. MIPS intocado (`R0-31` → int32, `F0-31` → float32 — os 9 `.expected` preservados) | ✅ aprovada |

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

### 5.5 CDB como vetor único de registradores físicos (D5→D20; D6/D12 revisadas por D16/D17/D18/D21)
> **Revisão (D16/D17/D20):** a abordagem da D12 ("tabela dentro do Register + `SetArchitecture`") foi
> substituída, e a D5 (CDB classe) foi substituída pela D20. O Register não valida nomes nem conhece
> arquitetura; as tabelas vivem nas subclasses; o CDB é um **vetor único indexado pelo id físico global**;
> aliases compartilham o mesmo id (D18) — dependências cruzadas sem nenhuma lógica extra.

**Classes de registrador (D20) — o `type` é metadado (largura/semântica/impressão), não seletor de banco:**

| Código | Classe | Registradores (id físico global) |
|---|---|---|
| `'B'` | int8 | x86 `AL, AH, BL, BH, CL, CH, DL, DH, SIL, DIL, SPL, BPL, R8B…R15B` — ids 0-15 (compartilhados) |
| `'W'` | int16 | x86 `AX, BX, CX, DX, SI, DI, SP, BP, R8W…R15W` — ids 0-15 (compartilhados) |
| `'R'` | int32 | MIPS `R0-31` (ids 0-31); x86 `EAX…R15D` (ids 0-15); ARM64 `W0-30` (ids 0-30) |
| `'L'` | int64 | x86 `RAX…R15` (ids 0-15); ARM64 `X0-30` (ids 0-30); RISC-V rv64 `x0-31` (ids 0-31) |
| `'F'` | float32 | MIPS `F0-31` (ids 32-63); ARM64 `S0-31` (ids 32-63); RISC-V `f0-31` (ids 32-63) |
| `'S'` | float64 | ARM64 `D0-31` (ids 32-63, compartilhados com `S0-31`) |
| `'V'` | float128 | x86 `XMM0-15` (ids 64-79) |
| `'G'` | flags | x86 `EFLAGS`; ARM64 `CPSR` (id 80) |
| `'Z'` | inválido/vazio | `Register()` default (mantém `InvalidRegister`) |

**Layout físico global (id = slot em `cdb.registers`):** 0-31 inteiros · 32-63 ponto flutuante · 64-79 vetorial · 80 flags.

**Helpers do módulo de instrução (refatoração do Passo 2):**

| Helper | Onde vive | Papel |
|---|---|---|
| `fillCDB(CDB&, char classe, int base, int count)` | declarada em `Instruction.h` (seção HELPERS) — definida **1x** em `Instruction.cpp` | preenche slots vagos de `registers` (só escreve onde `GetId()==-1` → a 1ª faixa define a classe canônica); usada pelos 4 `MakeCDB` |
| `RegisterTable()` | **declarada `static`** em `Instruction.h` (seção ELEMENTO STATIC) — **definida em cada `.cpp` de subclasse** (static no escopo de arquivo = internal linkage: cada TU tem a sua cópia) | tabela `nome → (classe, id físico)` da arquitetura. **Convenção: toda nova subclasse DEVE definir a sua** — sem a definição, qualquer TU que a use falha no link ("undefined reference": linkage interna não enxerga definições de outras TUs). O `-Wunused-function` ("declared 'static' but never defined") nas TUs que só incluem o header (Instruction.cpp, Thread, Main, testbenchs) é esperado e silenciado com `#pragma GCC diagnostic push/ignored "-Wunused-function"/pop` em volta da declaração |
| `LookupRegister(name, context, table)` | declarada em `Instruction.h` (seção HELPERS) — definida **1x** em `Instruction.cpp` | única validação de nome do sistema (aborta com nome + instrução). **Recebe a tabela por parâmetro** (injeção de dependência): `SetAttributes` chama `LookupRegister(tokens[i], instruction_string, RegisterTable())`. Histórico: a versão que chamava `RegisterTable()` internamente quebrava o link (undefined reference em `Instruction.o` — a definição da subclasse é privada da própria TU) |

**`print_banks` escolhidos (ordem de impressão) e tamanhos de `registers`:**
- MIPS (64): `{{'F',32,32},{'R',0,32}}` · x86 (81): `{{'R',0,16},{'L',0,16},{'V',64,16},{'W',0,16},{'B',0,16},{'G',80,1}}` · ARM64 (81): `{{'F',32,32},{'R',0,31},{'S',32,32},{'G',80,1}}` · RISC-V (64): `{{'F',32,32},{'L',0,32}}`

- [x] `headers/Components.h` — `struct CDB` (D20):
  - [x] `std::vector<Register> registers;` — um slot por registrador físico (id = índice)
  - [x] `struct CDB_BANK { char classe; int base; int count; };` + `std::vector<CDB_BANK> print_banks;` — faixas de impressão na ordem F, R, S, L, V, W, B, G (F antes de R preserva o output MIPS; bancos vazios pulados)
  - [x] helpers `Register& GetReg(CDB&, const Register&)` + versão `const` — `registers[reg.GetId()]` com bounds check (id < 0 ou ≥ size → `std::abort()`); **sem checagem de classe**
- [x] `Register` — assinatura simplificada (D16):
  - [x] `Register()` (vazio) e `Register(char type, int id)`; **remover** o construtor de string, `ParseType`/`ParseId` e o `num_registers` global (`Components.h:14`, `Components.cpp:60`, `Thread.cpp:157`)
  - [x] **sem campo de aliases** — aliases são o mesmo id (D18)
- [x] Subclasses (`Code/Instruction/`):
  - [x] tabela estática `nome → (classe, id físico)` por arquitetura (D20/D21); **chaves em MAIÚSCULA** para MIPS/x86/ARM64 e **minúsculas** para RISC-V (casing garantido pela normalização)
  - [x] **validação de nome no parse**: `SetAttributes` faz o lookup na tabela da **própria subclasse** e aborta com mensagem (nome + instrução) se não existir — única validação de nome do sistema (`LookupRegister(tokens[i], instruction_string, RegisterTable())`, ver helpers acima)
  - [x] `static CDB MakeCDB()` por subclasse — preenche `registers` via `fillCDB` (dedup por id físico; classe canônica = 1ª faixa) e `print_banks`; MIPS/RISC-V por loop; x86/ARM64 com faixas sobrepostas para os aliases (D18)
  - [x] flags na mesma tabela: `EFLAGS→('G',80)`, `CPSR→('G',80)` (D6 com classe única de flags)
  - [ ] x86: `MUL`/`IMUL`/`DIV` com destinos implícitos `{RAX, RDX}` (D19); `MOV` continua decisão pendente (§7)
- [x] `InstructionFactory.h` — dispatch estático por arquitetura (D17): `static CDB MakeCDB(ARCHITECTURE)` delegando às subclasses
- [ ] Substituir acessos diretos `cdb.F/R[...]` pelo `GetReg` universal:
  - [ ] `Thread.cpp:155-156` — criação do banco via `InstructionFactory::MakeCDB(arch)`
  - [ ] `Thread.cpp:432-444` — `regs = (dest.GetType() == 'F') ? cdb.F : cdb.R` → `GetReg(cdb, dest)`
  - [ ] `ReservationStations.cpp:167-169, 192-193, 231-233` → `GetReg(cdb, ...)`
  - [ ] `Main.cpp:428-434` — impressão iterando `print_banks` (label = classe, índice exibido = id − base)
- [ ] `tb_ReservationStations.cpp` / `tb_Thread.cpp` — atualizar acesso direto `cdb.F[i]`/`cdb.R[i]` para a nova API; `tb_Components.cpp` — reescrever testes do construtor de string (linhas 20-76) + testes de `GetReg` (id fora do banco → abort); `tb_Instruction.cpp` — ids dos F regs ganham +32 (base 32)
- [ ] **Arquitetura da CDB**: `Thread` recebe `ARCHITECTURE` no construtor e monta o banco via `InstructionFactory` (ver §5.6)

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
- [x] Passo 2 fechado: os 5 `.cpp` compilam isolados `-Wall -Wextra` sem warnings (pragma no `-Wunused-function`) e o link parcial (Instruction + 4 subclasses + Components) resolve — só falta `main` (esperado, testado no `/tmp/opencode`)
- [ ] Teste manual: rodar um input `tipo 1` com `arquitetura 0` e conferir saída idêntica à anterior (fica pendente até o Bloco C — chave de config; o `simtest` já cobre a regressão do default)

### 5.8 Registradores: aliases e escritas parciais (D18/D21)
> Registro da discussão de design (WAW + escritas parciais). Decisões: D16/D17/D18/D19/D20/D21.

**Semântica de zero-extension (x86-64 e AArch64):**
- No x86-64, escrever um registrador de 32 bits **zera os 32 bits superiores** do de 64 bits (`ADD EAX, ECX` define `RAX` inteiro = `0x00000000_XXXXXXXX`). Idem no AArch64 para `W` sobre `X`.
- Consequência: a escrita de `EAX` é uma **escrita completa** — não é um caso parcial. O WAW entre `MUL RAX` (lento) e `ADD EAX` (rápido) tem leitor de `RAX` corretamente atendido pelo produtor mais novo; o WR atrasado do MUL é "ignorado" (renomeação), como num core real.
- **Escritas de 16/8 bits NÃO zeram**: `ADD AX, CX` preserva os 48 bits superiores de `RAX`; `ADD AL, CL` preserva 56 bits. Em ARM64, `S` sobre `D` deixa os 32 superiores *UNPREDICTABLE* (spec) — **decidido (D21):** modelar `S` como zero-extension (como a maioria das implementações) e documentar.

**Como o WAW é tratado (referência):**
- Tomasulo com ROB (máquinas modernas): WR por **conclusão** (fora de ordem); correção via renomeação + commit em ordem. WR do escritor novo antes do antigo é **correto**.
- Tomasulo clássico (H&P, sem renomeação): WAW travado no **issue** (stall se o destino estiver busy) — não no WR.
- Este simulador: não trava WAW no issue (modelo-ROB); `GetCurrentRS()` = pendente mais recente.

**Limitação conhecida (a documentar nos testes):**
1. **Pessimismo pré-existente** (também no MIPS): se o escritor novo termina antes do antigo (`ADD.D WR 8`, `DIV.D WR 10`), um leitor emitido entre os dois WRs espera o **pendente mais recente** — o antigo — mesmo o valor final já estando pronto. Conservador (nunca otimista), nunca erra valor; só atrasa. Exemplo real nos expecteds: `sim-uma_thread-sem_spec-05` (F4 `[1-10]`/`[2-11]`, sem leitor na janela).
2. **Escritas verdadeiramente parciais** (`AX/AL/AH`, D21): o valor final de `RAX` combina bits de **dois** produtores (o parcial novo + o completo anterior, ainda em voo). Com `Q[i]` de slot único, o leitor espera só o pendente mais recente → timing otimista se o escritor completo antigo terminar **depois** do parcial novo. Casos típicos são raros e podem ser mitigados com aliasing total ou multi-Q (renomeação parcial real); por ora fica documentado como limitação, igual aos cores com merge parcial.

**Exemplo de referência (MUL de 2 destinos, D19):**
```
MUL RAX, RBX   ; A1 — x86 real: RDX:RAX = RAX×RBX (2 destinos implícitos)
ADD EAX, ECX   ; A2 — zero-extension: RAX = 0x00000000_(EAX+ECX)
SUB RDX, RAX   ; B — RDX = (RAX×RBX)_alto − zeroext(EAX+ECX)
```
- No modelo simplificado (MUL com destino só RAX): `RDX = RDX_antigo − zeroext(EAX+ECX)`.

**Mesmo id físico por arquitetura (D18/D21):**

| Arquitetura | Grupo de alias (mesmo id, classes diferentes) | Observação |
|---|---|---|
| x86 | id 0-15: `RAX('L') = EAX('R') = AX('W') = AL/AH('B')`, idem `RBX/EBX/BX/BL/BH`…`R15/R15D/R15W/R15B`; ids 64-79: `XMM0-15('V')`; id 80: `EFLAGS('G')` | `AL` e `AH` são grupos parciais distintos do ponto de vista físico (não se sobrepõem) — no simulador compartilham o slot |
| ARM64 | id 0-30: `X0('L') = W0('R')`…`X30 = W30`; id 32-63: `D0('S') = S0('F')`…`D31 = S31`; id 80: `CPSR('G')` | `S` modelado como zero-extension (D21) |
| MIPS | ids 0-31 `R0-31('R')`, ids 32-63 `F0-31('F')` — sem aliases | largura única por classe |
| RISC-V | ids 0-31 `x0-31('L')`, ids 32-63 `f0-31('F')` — sem aliases | rv64 |

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
- [ ] Atualizar `testbenchs/tb_Thread.cpp` para CDB vetor único (`registers.size() == 64`) + construtor com arquitetura
- [ ] Novo `testbenchs/tb_InstructionFactory.cpp` (se não for coberto no tb_Instruction) — adicionar ao `TB_NAMES` do Makefile
- [ ] `tb_Components.cpp` — testes de `GetReg` (id fora do banco → abort; sem `GetAliases` — aliases são o mesmo id)
- [ ] Testes das classes novas: `AX/AL/AH` (int8/int16), `XMM0-15` (float128), `EFLAGS`/`CPSR` (flags), rv64 (`x0-31` int64) — com os ids físicos globais

### 6.2 Casos de simulação
- [ ] Criar `test-cases/inputs/sim-x86-01.txt` (arquitetura 1, com `ADD EAX, EBX` etc.)
- [ ] Criar `test-cases/inputs/sim-arm-01.txt` (arquitetura 2, com `ADDS`/`LDR`/`STR`)
- [ ] Criar `test-cases/inputs/sim-riscv-01.txt` (arquitetura 3, com `FADD.D`/`JAL`)
- [ ] Gerar `.expected` para os novos casos (`make simtest` gera referência quando ausente)
- [ ] Conferir dependências de flags (EFLAGS/CPSR) funcionando no CDB (tabela com EX atrasado)
- [ ] Caso de demonstração das limitações (§5.8): x86 com `MUL RAX` lento + `ADD EAX` rápido + leitor de `RAX` (zero-extension exato) e variante com `ADD AX`/`AL` (parcial — slot único de `Q`)
- [ ] Caso com `MUL RAX, RBX` verificando os 2 destinos (`RDX` e `RAX`) no CDB (D19)

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

- **`Register` só entende `R`/`F`**: nomenclatura de ARM64 (`X0`/`D0`...), RISC-V (`x1`/`f1`...) e x86 (`EAX`...) aborta no parse hoje — **resolvido por D16/D17** (Register simplificado + tabelas nas subclasses); é **pré-requisito** para testar as arquiteturas além de MIPS (verificado na Fase 1).
- **CDB vetor único (D20)**: `GetReg` = `registers[reg.GetId()]` com bounds check; **nenhuma checagem de classe** — a tabela do parse é a fonte da verdade (combinação classe/id errada é bug de construção, pego no parse com abort). A impressão itera `print_banks` na ordem fixa `F, R, S, L, V, W, B, G` pulando vazios para preservar os 9 `.expected` MIPS (F antes de R).
- **Aliases (D18)**: compartilham o **mesmo id físico** → mesmo slot; nada a varrer no lado da leitura (a resolução de WR já é por `rs_id` — nada muda em `BroadcastOnCDBAndRS`). Atenção ao casing das chaves (MAIÚSCULO para MIPS/x86/ARM, minúsculo para RISC-V). `Register` copiado em `V.assign`/`V[i] = src` — sem efeito extra (ids globais, cópias baratas).
- **Convenção `RegisterTable()`**: declarada `static` em `Instruction.h` (seção ELEMENTO STATIC) e **definida em cada `.cpp` de subclasse** (internal linkage — a declaração no header não cria um símbolo único; cada TU tem a sua). **Nova subclasse sem a definição → "undefined reference" no link.** O warning `-Wunused-function` nas TUs que só incluem o header é esperado e silenciado por pragma em volta da declaração — **não remover a declaração do header** achando que é lixo: ela é o protótipo da convenção.
- **WAW com escritas parciais**: o "mais recente pendente vence" é **exato** para `EAX/RAX` e `W/X` (zero-extension = escrita completa, D18/§5.8); o pessimismo do leitor entre dois WRs é pré-existente e conservador; `AX/AL/AH` incluídos (D21) têm a limitação de escrita parcial documentada (slot único de `Q`); `S` modelado como zero-extension — ver §5.8.
- **x86 `MUL`/`IMUL`/`DIV`**: 2 destinos implícitos (`RDX:RAX` + `EFLAGS`, D19) — conferir o mapeamento no `IdentifyType`/`NormalizeInstruction` do x86.
- **Cópias de `Thread`**: `std::vector<Thread> threads` usa move na inicialização; com `shared_ptr` no `rob`/tabela/RS, cópias acidentais continuam seguras. O `struct CDB` com `std::vector<Register>` permanece copiável (D20 — sem classe, sem ponteiro).
- **`Instruction` com `position == -1`**: a RS constrói `current_instruction` default; com `shared_ptr`, usar `nullptr` como "sem instrução" e ajustar guardas (`IsBusy()` já evita acesso).
- **X86 `MOV`**: hoje é sempre `LOAD` — decisão pendente: detectar memória `[...]` no operando 2 para diferenciar `MOV reg,reg` (INT_BASIC) de `MOV reg,[mem]` (LOAD). Opcode artificial `MOV_STORE` deve ser revisado.
- **ARM64 `LDR X0, [X1, #8]`**: `SplitInstruction` já trata `[`, `]` e `#`; validar `SetAttributes` com deslocamento (tokens extras).
- **RISC-V lowercases tudo** na normalização: output em minúsculo é esperado (diferente do MIPS que é UPPERCASE).
- **Latências**: `base_ex_latencies`/`base_mem_latencies` estáticos permanecem iguais aos atuais (compatibilidade com config `latencias_ex`/`latencias_mem`).
- **Compatibilidade de config**: toda chave nova precisa ser opcional (default = comportamento atual) para não quebrar os inputs existentes.
- **`INSTRUCTION_PHASE_TOMASULO` único**: com a D15, `Code/headers/Instruction.h` define esse nome e o legado homônimo foi sobrescrito pelo conteúdo novo — não há mais duas definições (o swap do header foi atômico com a substituição do legado).
