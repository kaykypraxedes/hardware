# PLANO — CDB por variantes (id, mask) no x86

> Documento de referência para implementação.
> Status: **x86 IMPLEMENTADO e verificado** (rebuild 0 warnings, testbenchs 800/800, simtest 9/9,
> smoke x86 com al/ah independentes). **ARM64 pendente** — ver seção 8.

---

## 1. Objetivo

Eliminar as dependências falsas entre registradores sobrepostos do x86 (ex.: `al`/`ah`) e permitir
rastreamento correto de registradores parciais (ex.: leitura de `rax` esperando produtores de
`al` e `ah` separadamente), usando **máscaras de bits** como identidade dos registradores físicos.

Analogia: o CDB hoje é um armário em que `al`, `ah`, `ax`, `eax`, `rax` compartilham **a mesma
gaveta** (um slot por id). A máscara é a etiqueta ("al mexe só no primeiro byte"), mas as gavetas
continuam uma por família — a etiqueta nunca separa as gavetas. O plano torna **uma gaveta por
(id, máscara)**.

---

## 2. Estado atual (o que já foi feito)

| Item | Status | Local |
|---|---|---|
| `Register` com `mask` (construtor 3-arg, `GetMask()`) | Feito | `Code/headers/Components.h:17-26, 56` |
| `RegisterTable` x86 com máscaras (L=0xFF, R=0x0F, W=0x03, B=0x01/0x02, V/G=0xFF) | Feito | `Code/Instruction/InstructionX86Intel.cpp:126-175` |
| `fillCDB(cdb, classe, base, count, mask = 255)` | Feito | `Code/headers/Instruction.h:109-115` (default aplicado; param renomeado p/ `reg_class`) |
| `GetMaskedRegisters(const Register&)` sem CDB | Feito e usado | `Code/Instruction/InstructionX86Intel.cpp:80-102` |
| `MakeCDB` x86 por variante contígua (85 slots) + `print_banks` de 7 bancos | Feito | `Code/Instruction/InstructionX86Intel.cpp:121-140` |
| `GetReg` por (id, mask) (busca linear, abort se não achar) | Feito | `Code/Components.cpp:124-146` |
| `PushWithMasked` com dedupe em destinos E fontes | Feito | `Code/Instruction/InstructionX86Intel.cpp:104-119, 252-297` |
| Testbenchs x86 (2.x/7.1-7.7 + 7.8 CDB) | Feito | `Code/testbenchs/tb_InstructionX86Intel.cpp` |
| Verificação | Feito | rebuild 0 warnings; testbenchs 800/800; simtest 9/9; smoke x86 al/ah paralelos + RAW por máscara corretos |

---

## 3. Problemas a resolver

1. **Gaveta única por id**: `GetReg` → `cdb.registers[reg.GetId()]` (`Code/Components.cpp:131`).
   `al`/`ah`/`ax`/`eax`/`rax` são o mesmo slot, uma cadeia de produtores só → `mov ah, y`
   pendente bloqueia leitores de `al` (dependência falsa). A máscara nunca entra no circuito de
   dependência.
2. **Duplicatas na concatenação como está**: `mov al, x` → dests `{al, ax, eax, rax}` → 4
   alocações do **mesmo slot 0** (mesmo RS, 4× na mesma cadeia). O `DeallocateRS`
   (`Code/Components.cpp:105-118`) fecha **só a primeira** ocorrência; as demais ficam pendentes
   para sempre → slot "ocupado" eternamente → deadlock. (O lado RS — `ResolveDependency`,
   `Code/ReservationStations.cpp:361-369` — limpa todos os Q que casam com o `rs_id`; o lado CDB
   não. Falha latente: hoje nenhuma instrução duplica alocação.)
3. **`GetMaskedRegisters` pede CDB**: `SetAttributes` roda no parse (`Code/Instruction.cpp:83`),
   sem CDB (criado depois pelo `MakeCDB()`). O helper deve iterar a `RegisterTable()`.

---

## 4. Decisões de design

- **Não criar novas classes de tipo** para `ah`/`bh`/`ch`/`dh`. A classe 'B' continua sendo a
  classe arquitetural; a **máscara é o diferenciador**. Dois slots coexistem com
  `('B', 0, 0x01)` e `('B', 0, 0x02)`.
- **Encoding**: `id` = família (0-15), `mask` = variante. Interferência = mesmo id + máscaras
  sobrepostas (`(mask & mask) != 0`). O check `GetType() != reg.GetType()` exclui o próprio
  registrador (e é inócuo para al/ah: máscaras não sobrepõem).
- **Expansão em destinos E fontes** (não só destinos): leitura de `rax` precisa de Q nas
  variantes `al` E `ah` separadamente (caso `mov al, x` + `mov ah, y` pendentes — sem expansão
  de fontes, só o produtor mais novo da variante rax seria esperado e o resultado ficaria
  incorreto).
- **Layout do vetor**: contíguo por variante, para a impressão do CDB (Main.cpp:448-455, itera
  faixas contíguas) continuar funcionando sem mudança de código (só `print_banks` muda).
- **Escopo**: x86 apenas. ARM64 (`w0/x0`, `s0/d0`) pode adotar a mesma mecânica depois.
- **Outras arquiteturas não mudam**: Simplified/MIPS32/RISC-V/ARM64 continuam com máscara
  uniforme 255; a busca do `GetReg` por (id, 255) encontra o slot único.

---

## 5. Passos de implementação

### Passo 1 — Desbloquear o build: `fillCDB` com default

`Code/headers/Instruction.h:109` — declarar o parâmetro com default:

```cpp
void fillCDB(
    CDB& cdb,
    char class,
    int base,
    int count,
    int mask = 255
);
```

Assim as chamadas 4-arg existentes (Simplified, MIPS32, RISC-V, ARM64 e `'B'` em
InstructionX86Intel.cpp:118) continuam compilando com 255.

### Passo 2 — `MakeCDB` do x86: layout por variante

Substituir `resize(81)` + `fillCDB` (`Code/Instruction/InstructionX86Intel.cpp:108-123`) por
preenchimento explícito por variante:

```cpp
// Monta o CDB com os registradores físicos:
CDB InstructionX86Intel::MakeCDB() {
    // Layout por variante (um slot por (id, mask)), contíguo por variante:
    // - ids 0-15:  famílias int (L/R/W/B), máscaras 0xFF/0x0F/0x03/0x01 (0x02 p/ ah-bh-ch-dh).
    // - ids 64-79: 'V' (xmm), 0xFF.
    // - id 80:     'G' (EFLAGS), 0xFF.
    CDB cdb;
    for (int fam = 0; fam < 16; fam++) {
        cdb.registers.push_back(Register('L', fam, 0xFF));
        cdb.registers.push_back(Register('R', fam, 0x0F));
        cdb.registers.push_back(Register('W', fam, 0x03));
        cdb.registers.push_back(Register('B', fam, 0x01));
        if (fam < 4) cdb.registers.push_back(Register('B', fam, 0x02)); // ah/bh/ch/dh
    }
    for (int i = 0; i < 16; i++) cdb.registers.push_back(Register('V', 64 + i, 0xFF));
    cdb.registers.push_back(Register('G', 80, 0xFF));
    cdb.print_banks = {{'L', 0, 16}, {'R', 16, 16}, {'W', 32, 16},
                       {'B', 48, 16}, {'B', 64, 4}, {'V', 68, 16}, {'G', 84, 1}};
    return cdb;
}
```

- A impressão (Main.cpp:448-455) mostra a posição dentro do grupo → os dois bancos 'B' imprimem
  `B0..B15` (baixos) e `B0..B3` (altos). Sem refs x86, sem risco.
- Atualizar o comentário em `Code/headers/Components.h:74`
  ("Aliases compartilham o mesmo id -> mesmo slot") → agora: um slot por (id, mask).
- (Opcional) helper genérico `fillVariant(cdb, classe, id_base, count, mask)` com `push_back`
  — reutilizável para ARM64 no futuro.

### Passo 3 — `GetReg` por (id, mask)

`Code/Components.cpp:122-141` — trocar a indexação por busca linear:

```cpp
// ─── HELPERS ──────────────────────────────────────────────────────
// Público:
// Pesquisa o slot pelo par (id físico, máscara); a classe não é validada aqui.
const Register& GetReg(
    const CDB&      cdb,
    const Register& reg
){
    for (const Register& slot : cdb.registers) {
        if (slot.GetId() == reg.GetId() && slot.GetMask() == reg.GetMask())
            return slot;
    }
    std::cerr << "[ERRO] Slot não encontrado: " << reg.GetType() << reg.GetId()
              << " (mask 0x" << std::hex << reg.GetMask() << ")\n";
    std::abort();
}
```

- Chamadores transparentes: `Thread.cpp:430`, `ReservationStations.cpp:167, 190, 228`.
- tb_Components/tb_ReservationStations usam `Register('R', i)` (mask default 255) contra slots
  Simplified (255) → casam sem mudança.

### Passo 4 — `GetMaskedRegisters` sem CDB

`Code/Instruction/InstructionX86Intel.cpp:84-105` — mudar a assinatura e a fonte de iteração:

```cpp
// Recebe o registrador alvo para procurar dependências de hardware:
// - ax (16 bits) contém al (8 bits de baixo) e ah (8 bits de cima), por exemplo.
// - Quando ax é usado, ele bloqueia al e ah;
// - Quando ah é usado, al ainda pode ser usado (ax não).
// - Simplificação: registrador grande = número binário; registradores menores = pedaços dele.
// - Mesmo id + máscaras sobrepostas = mesmo espaço de hardware (bloqueiam-se mutuamente).
static std::vector<Register> GetMaskedRegisters(
    const Register& target_reg
){
    std::vector<Register> blocked_regs;
    for (const auto& [name, reg] : RegisterTable()) {
        if (target_reg.GetType() != reg.GetType() && // Tipo igual = o próprio registrador.
            target_reg.GetId()  == reg.GetId()     &&
            ((target_reg.GetMask() & reg.GetMask()) != 0)) // A mágica acontece aqui.
            blocked_regs.push_back(reg);
    }
    return blocked_regs;
}
```

A tabela não tem duplicatas de (type, id, mask) ✓.

### Passo 5 — `SetAttributes` do x86: concatenação em destinos E fontes

`Code/Instruction/InstructionX86Intel.cpp:252-291`. Helper local:

```cpp
// Empurra o registrador e todas as suas variantes sobrepostas (com dedupe).
static void PushWithMasked(
    std::vector<Register>& list,
    const Register&        reg
){
    for (const Register& r : GetMaskedRegisters(reg))
        if (std::find(list.begin(), list.end(), r) == list.end())
            list.push_back(r);
    if (std::find(list.begin(), list.end(), reg) == list.end())
        list.push_back(reg);
}
```

> Nota: `Register` não tem `operator==` — o dedupe precisa comparar
> `(GetType(), GetId(), GetMask())`. (Considerar adicionar `operator==` ao `Register`.)

Aplicação no `SetAttributes`:

- **BRANCH**: inalterado (só EFLAGS, sem variantes).
- **LOAD** (hoje L262-267): dest = `PushWithMasked(dest_registers, LookupRegister(tokens[1], ...))`;
  fonte = `PushWithMasked(source_registers, base)`.
- **STORE** (hoje L268-274): fontes = `PushWithMasked(..., LookupRegister(tokens[2], ...))` +
  `PushWithMasked(..., base)`.
- **Genérico** (hoje L275-290):
  - dest = `PushWithMasked(dest_registers, LookupRegister(tokens[1], ...))`;
  - se `!IsMOVCopy(tokens)`: push EFLAGS `Register('G', 80, 0xFF)` como dest e
    `PushWithMasked(source_registers, LookupRegister(tokens[1], ...))` como fonte (dest-as-source);
  - op2 (se `tokens.size() > 2`): `PushWithMasked(source_registers, op2)`.

Exemplos de resultado:
- `mov al, bl` → dests `{al, ax, eax, rax}`; fontes `{bl, bx, ebx, rbx}`.
- `mov ah, bh` → dests `{ah, ax, eax, rax}` — **`al` não é bloqueado** (quirk 7.5 morre).
- `add rax, rbx` → dests `{rax, eax, ax, al, ah}` + EFLAGS; fontes `{rax, eax, ax, al, ah, rbx, ebx, bx, bl, bh}`.
- EFLAGS e xmm sem variantes → comportamento inalterado.
- `add eax, eax` → fontes com duplicatas removidas pelo dedupe.

### Passo 6 — Testbenchs (`Code/testbenchs/tb_InstructionX86Intel.cpp`)

Atualizar na execução, linha a linha (contagens/índices mudam):

- **2.x / 7.1**: ex.: `mov eax, ebx` → dests `{eax, rax}` (2, sem EFLAGS); fontes `{ebx, rbx}` (2).
- **7.4**: checks de (type, id) continuam válidos (rax→'L' 0, eax→'R' 0, ax→'W' 0, al→'B' 0);
  verificar índices de fontes de `add r8d, r9d`.
- **7.5**: **inverter** — de "AL/AH colidem no id (limitação)" para "al ('B',0,0x01) e
  ah ('B',0,0x02) são variantes independentes" (checar máscaras distintas).
- **7.6 movzx**: `movzx` é INT_BASIC (não MOVS) → quirk EFLAGS/self-dest permanece; índices das
  fontes expandidas mudam.
- **7.7 movsd**: inalterado (xmm sem variantes).
- **Novos checks** (nível CDB, novo bloco):
  - `GetReg(cdb, al) != GetReg(cdb, ah)` (slots distintos);
  - alocação de `mov al` não deixa o slot de `ah` busy;
  - leitura de `rax` com `mov al` + `mov ah` pendentes gera Q para os dois produtores;
  - sem duplicatas: cadeia do slot com 1 entrada por alocação (cobre o risco do `DeallocateRS`).

Demais testbenchs (Components, ReservationStations, Simplified, MIPS32, RISC-V, ARM64):
**sem mudança**.

### Passo 7 — Verificação

```bash
make rebuild   # 0 warnings
make test      # esperado: 786+ atualizações OK
make simtest   # 9/9, SEM regenerar referências (Simplified intocado)
```

- Smoke manual x86: cenário al/ah/ax/rax (dependências parciais) + movsd store/copy (regressão).

---

## 6. Riscos e cuidados

- **Build quebrado no estado atual**: `fillCDB` sem default + chamadas 4-arg → Passo 1 primeiro.
- **Guarda do `fillCDB` antiga**: no layout atual, a guarda `GetId()==-1` faz R/W/B serem
  ignorados (só o L preenche) — o novo `MakeCDB` (push explícito) elimina isso.
- **Dedupe obrigatório** em dests/fontes para não inflar V/Q (ex.: `add eax, eax`).
- **Impressão do CDB x86 muda** (variantes) — sem refs x86, sem risco.
- **`GetReg` linear** (~90 slots × chamadas por ciclo) — irrelevante em performance.

---

## 7. Decisões pendentes

1. **ARM64** (`w0/x0`, `s0/d0`): adiado de propósito — ver seção 8 (o que falta + ideia proposta).
2. **`fillCDB`**: **resolvido** — default `255` aplicado no header (`Code/headers/Instruction.h:114`).

---

## 8. Próximo passo: ARM64 (`w0/x0`, `s0/d0`)

### 8.1 O que falta

Diferente do x86, o ARM64 **não tem um bug de dependência falsa** no estado atual: `w0` é a
metade baixa de `x0` e, na arquitetura real, **escrever `w0` sempre altera `x0`** (zero-extend);
`d0`/`s0` idem. Ou seja, `w0` e `x0` sempre interferem → o slot compartilhado atual (máscara
uniforme 255) é **semanticamente correto**.

O que falta é **fidelidade de largura e de impressão**:

1. **Impressão duplicada do CDB**: o slot de `x0` (`'L'`) é impresso 2× — como banco `L` e como
   banco `R` — porque `print_banks` repete as faixas (`{{'L',0,31},{'R',0,31},{'S',32,32},
   {'F',32,32},{'G',80,1}}`). O modelo "1 slot por variante" elimina a duplicação (como o x86
   agora).
2. **Largura explícita**: a máscara documenta o trecho de bits coberto (`x0` = 64 bits,
   `w0` = 32 bits) e deixa o modelo pronto para futuras variações.
3. **`MakeCDB` do ARM64** ainda usa `resize(81)` + `fillCDB` com a guarda `GetId()==-1`
   (`Code/Instruction/InstructionArm64.cpp:58-72`): na prática só `L` e `S` preenchem
   (`R`/`F` são engolidos pela guarda) — funciona via `GetReg` sem checagem de classe, mas não é
   honesto e não suporta variantes.

### 8.2 Ideia proposta (mesmo mecanismo do x86)

1. **`RegisterTable` ARM64** (`InstructionArm64.cpp:75-100`) com máscaras:
   ```cpp
   t.emplace("x" + std::to_string(i), Register('L', i,          0xFF)); // 64 bits.
   t.emplace("w" + std::to_string(i), Register('R', i,          0x0F)); // 32 bits (metade baixa).
   t.emplace("d" + std::to_string(i), Register('S', 32 + i,     0xFF)); // 64 bits.
   t.emplace("s" + std::to_string(i), Register('F', 32 + i,     0x0F)); // 32 bits (metade baixa).
   t.emplace("cpsr", Register('G', 80, 0xFF));
   ```
   - Só 2 larguras por família → sem "dois pedaços disjuntos" (não há equivalente a `al`/`ah`).

2. **`MakeCDB` por variante contígua** (mesmo padrão do x86, com `push_back`):
   ```cpp
   for (int i = 0; i < 31; i++) cdb.registers.push_back(Register('L', i, 0xFF));  // x0-30.
   for (int i = 0; i < 31; i++) cdb.registers.push_back(Register('R', i, 0x0F));  // w0-30.
   for (int i = 0; i < 32; i++) cdb.registers.push_back(Register('S', 32 + i, 0xFF)); // d0-31.
   for (int i = 0; i < 32; i++) cdb.registers.push_back(Register('F', 32 + i, 0x0F)); // s0-31.
   cdb.registers.push_back(Register('G', 80, 0xFF));
   // 127 slots: L 0-30, R 31-61, S 62-93, F 94-125, G 126.
   cdb.print_banks = {{'L', 0, 31}, {'R', 31, 31}, {'S', 62, 32}, {'F', 94, 32}, {'G', 126, 1}};
   ```

3. **`GetMaskedRegisters` + `PushWithMasked` locais** em `InstructionArm64.cpp` (cópia do padrão
   x86, iterando a `RegisterTable()` do ARM64) e aplicação no `SetAttributes` (destinos E fontes;
   BRANCH inalterado — só lê CPSR).

4. **Semântica preservada**: como `w0 ⊂ x0` (máscaras sobrepõem), a expansão faz `mov w0, #1`
   alocar **`{w0, x0}`** juntos — exatamente o comportamento atual (slot compartilhado). Nenhuma
   dependência muda; o ganho é impressão correta e largura explícita.

5. **Testbench**: atualizar `tb_InstructionArm64.cpp` (seção 7.1 "Aliasing de largura" + novos
   checks de CDB: slots `w0`/`x0` distintos, `s0`/`d0` distintos, 127 slots, 5 bancos) e verificar
   com `make rebuild` (0 warnings) + `make test` + `make simtest` (9/9, sem regenerar refs —
   ARM64 não tem refs).

> Prioridade: **baixa** (cosmético/fidelidade). Fazer quando quiserem evoluir o ARM64.
