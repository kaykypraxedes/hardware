# Checklist de fixes — Parser de Instruções (multi-arquitetura)

> Fixes 1 (`piece.clear()` em `PushOperandSources`) e 2 (`tokens.empty()` em `Instruction::Parse`) já aplicados.
> Este documento cobre a partir do fix 3, mais uma segunda varredura completa em busca de casos que passaram despercebidos na primeira análise. Itens novos (não mencionados na conversa anterior) estão marcados com **[NOVO]**.
> **Status atual:** seções 1-7 finalizadas e verificadas (rebuild 0 warnings, testbenchs 860/860 OK, simtest 9/9). **Refactor extra:** `IsRegister` unificado — helper compartilhado `bool IsRegister(const std::string&, const table&)` em Instruction.h/Instruction.cpp (mesmo padrão do `LookupRegister`); as 4 arquiteturas delegam a ele. ARM64 mantém `IsZeroRegister` nos call sites de acesso direto ao `LookupRegister` (LOAD/STORE/ldp/stp). Falsos positivos eliminados: labels como `X99` (ARM64) e `R99` (Simplified) não são mais confundidos com registrador (case preservado, sem abort).

## Como usar
- Cada item tem checkbox, prioridade, arquivo, e uma explicação com o trecho relevante.
- Prioridades: 🔴 crítico (crash ou corrompe rastreamento de dependência em código comum) · 🟠 alto (afeta corretude em casos comuns) · 🟡 médio (impreciso, mas não quebra) · ⚪ nitpick/observação.
- Marquei como validado empiricamente (rodei o trecho isolado em C++) quando fiz isso.

---

## 1. `Components.h` / `Components.cpp`

### [x] 1.1 `Register::GetCurrentRS()` — subtração antes do cast (unsigned underflow) **[NOVO]**
🟡 Médio · `Components.cpp`

```cpp
std::string Register::GetCurrentRS() const {
    for (int i = static_cast<int>(allocated_rs.size() - 1); i >= 0; i--) {
```

Se `allocated_rs` estiver vazio, `allocated_rs.size()` é `0` (tipo `size_t`, sem sinal). `0 - 1` primeiro faz o **underflow em `size_t`** (vira um número enorme), e só depois disso o resultado é convertido para `int`. Antes do C++20 esse tipo de truncamento é *implementation-defined* (não totalmente UB, mas não garantido pelo padrão); na prática, em GCC/Clang/x86-64 hoje isso "dá certo por acidente" (o padrão de bits vira `-1` ao truncar), mas é frágil e depende de comportamento não garantido pela norma.

Compare com `GetRSCycleStart`, que faz o cast **antes** de subtrair — e está correto:
```cpp
int Register::GetRSCycleStart(const std::string& rs_id) const {
    for (int i = static_cast<int>(allocated_rs.size()) - 1; i >= 0; i--) {
```

**Fix:** aplicar o mesmo padrão em `GetCurrentRS`:
```cpp
for (int i = static_cast<int>(allocated_rs.size()) - 1; i >= 0; i--) {
```

### [x] 1.2 `GetReg` ignora `type` na comparação (observação, não bug ativo)
⚪ Observação · `Components.cpp` — **resolvido**: `GetReg` agora compara `(GetType(), GetId(), GetMask())` (`Code/Components.cpp:132-136`), ou seja, a classe também é validada.

```cpp
const Register& GetReg(const CDB& cdb, const Register& reg){
    for (const Register& slot : cdb.registers) {
        if (slot.GetId() == reg.GetId() && slot.GetMask() == reg.GetMask())
            return slot;
```

Funciona hoje porque nenhuma arquitetura tem duas classes de registrador com `(id, mask)` idênticos no mesmo `CDB`. É um invariante implícito entre cada `MakeCDB()` e este helper — não há nada quebrado agora, mas se algum dia uma arquitetura nova (ou uma faixa nova) colidir em `(id, mask)`, o bug será silencioso (retorna o slot errado, não crasha). Sugestão: comparar `type` também, ou pelo menos deixar um comentário/assert documentando a suposição.

---

## 2. `InstructionX86Intel.cpp`

### [x] 2.1 Instruções que **não escrevem** no primeiro operando são tratadas como se escrevessem
🟠 Alto · `SetAttributes`

O `else` genérico assume sempre "operando 1 = destino (+ fonte se não for cópia)":
```cpp
} else {
    PushWithMasked(dest_registers, LookupRegister(tokens[1], ...));
    if (!IsMOVCopy(tokens)) {
        PushWithMasked(dest_registers, Register('G', 80));
        PushWithMasked(source_registers, LookupRegister(tokens[1], ...));
    }
    if (tokens.size() > 2) PushOperandSources(source_registers, tokens[2], ...);
}
```

Isso é **falso** para várias instruções que caem no mesmo `else` hoje:

- **`cmp`, `test`** (`INT_BASIC`): comparam e só setam flags — não escrevem no operando 1.
- **`comiss`, `ucomiss`** (`FLOAT_BASIC`): o par float de `cmp`/`test` — mesmo problema. **[NOVO]**

Isso cria um produtor-fantasma para o registrador comparado, gerando falso hazard WAW/RAW para quem ler esse registrador depois.

**Fix**, seguindo o padrão que o próprio `InstructionArm64.cpp` já usa para `cmp/cmn/tst/fcmp`:
```cpp
} else if (tokens[0] == "cmp" || tokens[0] == "test" ||
           tokens[0] == "comiss" || tokens[0] == "ucomiss") {
    PushWithMasked(dest_registers, Register('G', 80));
    PushOperandSources(source_registers, tokens[1], instruction_string);
    if (tokens.size() > 2)
        PushOperandSources(source_registers, tokens[2], instruction_string);
}
```

### [x] 2.2 `div` / `idiv` / `mul` (forma de 1 operando): destino real nunca é rastreado **[NOVO — bug adicional na mesma família do 2.1]**
🔴 Crítico · `SetAttributes`

Em x86, `div`/`idiv` **só existem** na forma de 1 operando (`idiv ebx`), e `mul` (não-`imul`) também é sempre 1 operando. Nessa forma:
- o operando explícito (`ebx`) é **fonte** (divisor/multiplicador) — nunca destino;
- o destino real é o par implícito `edx:eax` (ou `dx:ax`, ou `ah:al` para o operando de 8 bits) — **não rastreado em lugar nenhum do código**.

Como `div`/`idiv`/`mul` caem no mesmo `else` genérico do item 2.1, o código atual:
1. marca `ebx` (que é fonte) como **destino** — hazard WAW falso;
2. também adiciona `ebx` como fonte (correto, mas por acidente);
3. **nunca** cria uma dependência em `eax`/`edx` — quem ler `eax`/`edx` depois do `div` não vai esperar por ele.

Esse é provavelmente o bug de maior impacto silencioso do arquivo, porque `div`/`idiv` **não têm outra forma** — todo uso dessas instruções está errado hoje.

**Fix:** precisa de um caso dedicado que:
- trate `tokens[1]` como fonte (correto já, mas via `dest_registers` incorretamente);
- resolva o par implícito a partir da classe do registrador do operando (`B`→`ax`; `W`→`ax`+`dx`; `R`→`eax`+`edx`; `L`→`rax`+`rdx`) e adicione **esses** como destino (e também como fonte, já que a divisão consome o dividendo neles).

```cpp
} else if (tokens[0] == "div" || tokens[0] == "idiv" || tokens[0] == "mul") {
    // Operando explícito é sempre FONTE (divisor/multiplicador), nunca destino.
    PushOperandSources(source_registers, tokens[1], instruction_string);
    // Resolve o par implícito eax:edx (ou ax:dx, ou ax sozinho p/ 8 bits) a partir
    // do tamanho do operando, e marca como fonte+destino (dividendo/produto).
    // (implementar conforme o "type" do registrador de tokens[1])
}
```

### [x] 2.3 `movsx`/`movzx`/`not`/`cvtsi2ss`/`cvttss2si` tratados como "acumula + seta flags"
🟡 Médio · `SetAttributes` / `IsMOVCopy`

Esses opcodes não estão em `MOVS`, então `IsMOVCopy` retorna `false` para eles, e caem no ramo "não é cópia":
- `movsx`/`movzx`: são cópias com extensão — **não leem o destino antigo** e **não afetam flags** em x86 real.
- `not`: **não afeta flags** em x86 real (diferente de `neg`, que afeta).
- `cvtsi2ss`/`cvttss2si`: conversões int↔float — comportamento de cópia, não afetam EFLAGS geral.

Hoje todos ganham uma falsa dependência RAW no próprio destino e um falso clobber de EFLAGS (pode atrasar desnecessariamente um `jcc` seguinte).

**Fix sugerido:** ampliar a lista `MOVS` (ou criar uma lista separada `NO_FLAGS_COPY`) para incluir esses opcodes, e usar o mesmo teste `IsMOVCopy`/equivalente para decidir se marca flags+auto-dependência.

### [x] 2.4 `BRANCH`: EFLAGS falso em desvios incondicionais + operando de desvio indireto não capturado
🟡 Médio · `SetAttributes`

```cpp
if (type == INSTRUCTION_TYPE::BRANCH) {
    source_registers.push_back(Register('G', 80));
}
```

Isso roda para **todo** `BRANCH`, incluindo `jmp`, `call`, `ret` — que não dependem de EFLAGS (só os condicionais como `je`/`jne` dependem). Compare com `InstructionArm64.cpp`, que distingue corretamente (`b.xx` lê CPSR, `b`/`bl`/`ret`/`br`/`cbz` não).

Além disso, nenhum registrador de operando é extraído — `jmp rax` / `call rax` (desvio indireto por registrador, comum para ponteiros de função/vtables) não geram fonte nenhuma sobre `rax`.

**Fix:** só adicionar EFLAGS para a lista de condicionais (`je,jne,jg,jge,jl,jle,jbe,ja,jae,jb,js,jns,jp,jo`), e extrair registrador de `tokens[1]` quando o operando não for um label (indireto por registrador).

### [x] 2.5 `RegisterTable()` sem `rip` (endereçamento RIP-relative) **[NOVO]**
🟠 Alto · `RegisterTable`

Endereçamento relativo a `rip` (`mov eax, [rip+0x2f3a]`) é extremamente comum em binários x86-64 modernos (PIE/PIC, acesso a globals). Como `"rip"` não está na tabela, `PushPiece` chama `LookupRegister("rip", ...)` → `abort()`. Mesma categoria dos apelidos ABI faltando em RISC-V/ARM64 (itens 4.1 e 5.1 abaixo) — vale decidir se querem suportar (adicionando uma entrada especial, já que `rip` não é um registrador "normal" alocável) ou documentar como não suportado.

### [x] 2.6 (informativo) `push`/`pop` e outros opcodes não estão em nenhuma lista
⚪ Observação — **cobertura ampliada**: `push`/`pop` foram adicionados ao `INT_BASIC` e ganharam caso dedicado no `SetAttributes` (tratam `rsp` implícito como fonte+destino; `pop [mem]` como só-fonte), com testbenchs cobrindo ambos.

Não é bug — é decisão de escopo (`IdentifyType` retorna `false`, gera abort controlado com mensagem clara). Só deixando registrado caso vocês queiram ampliar a cobertura depois, já que são onipresentes em convenção de chamada x86.

---

## 3. `InstructionArm64.cpp`

### [x] 3.1 `ldp` / `stp` (load/store de par) completamente mal tratados **[NOVO — o achado mais grave desta rodada]**
🔴 Crítico · `SetAttributes`

Testei a tokenização de `stp x29, x30, [sp, -16]!` (ver saída acima): `tokens = ["stp","x29","x30","sp","-16","!"]`.

```cpp
if (type == INSTRUCTION_TYPE::LOAD) {
    PushWithMasked(dest_registers, LookupRegister(tokens[1], ...));
    if (tokens.size() > 2) PushWithMasked(source_registers, LookupRegister(tokens[2], ...));
} else if (type == INSTRUCTION_TYPE::STORE) {
    PushWithMasked(source_registers, LookupRegister(tokens[1], ...));
    if (tokens.size() > 2) PushWithMasked(source_registers, LookupRegister(tokens[2], ...));
}
```

Esse código foi escrito assumindo **exatamente 1 registrador de dado + 1 registrador base** (`ldr x0, [sp]` → `tokens=["ldr","x0","sp"]`). Só que `ldp`/`stp` têm **2 registradores de dado + 1 base**:

- **`ldp x0, x1, [sp, 16]`**: `x0` vira destino (ok), mas **`x1` (que deveria ser o 2º destino) vira FONTE** (tratado como se fosse o registrador base do endereço) — e o **registrador base real (`sp`) nunca é lido em lugar nenhum**.
- **`stp x29, x30, [sp, -16]!`**: `x29` e `x30` acabam corretos como fonte (por "coincidência", já que ambos são realmente fontes num store) — mas de novo **`sp`, o endereço real, nunca entra como fonte**.

Como `stp`/`ldp` são o padrão de prólogo/epílogo em praticamente toda função ARM64 compilada (`stp x29, x30, [sp, -16]!` / `ldp x29, x30, [sp], 16`), esse bug afeta a maioria dos traces ARM64 realistas — silenciosamente (não crasha, só produz dependências erradas: falta a dependência no `sp`, e no `ldp` o 2º registrador de dado nem é reconhecido como destino).

**Fix:** tratar `ldp`/`stp` como caso à parte, com os 4 tokens (`op, reg1, reg2, base[+offset]`):
```cpp
} else if (tokens[0] == "ldp") {
    PushWithMasked(dest_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
    PushWithMasked(dest_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
    if (tokens.size() > 3) PushWithMasked(source_registers, LookupRegister(tokens[3], instruction_string, RegisterTable()));
} else if (tokens[0] == "stp") {
    PushWithMasked(source_registers, LookupRegister(tokens[1], instruction_string, RegisterTable()));
    PushWithMasked(source_registers, LookupRegister(tokens[2], instruction_string, RegisterTable()));
    if (tokens.size() > 3) PushWithMasked(source_registers, LookupRegister(tokens[3], instruction_string, RegisterTable()));
}
```
(mesma lógica precisa ser espelhada em `NormalizeInstruction`, que hoje também assume só 1 registrador de dado ao montar a string — vale conferir se a exibição de `ldp`/`stp` está legível.)

### [x] 3.2 `RegisterTable()` sem `sp`, `lr`, `xzr`, `wzr`
🟠 Alto · `RegisterTable`

```cpp
for (int i = 0; i < 31; i++){
    t.emplace("x" + std::to_string(i), Register('L', i, 0xFF));
    t.emplace("w" + std::to_string(i), Register('R', i, 0x0F));
}
```
Vai só até `i < 31` (ou seja, `x0`..`x30`) — `x31`/`sp` nem existe na tabela. `sp` é o nome usado em praticamente todo código real (`str x30, [sp, -16]!`), e `lr` é o apelido comum de `x30` em epílogos. `xzr`/`wzr` (registrador zero) também não existem.

**Fix:**
```cpp
t.emplace("sp", Register('L', 31, 0xFF));
t.emplace("lr", Register('L', 30, 0xFF));   // alias de x30
// xzr/wzr: registrador zero — considerar tratar como "sempre não-fonte/não-destino"
// em vez de mapear para um slot de CDB real, já que fisicamente não é alocável.
```

### [x] 3.3 `cbz`/`cbnz`/`tbz`/`tbnz`/`br`/`blr`: registrador de operando não capturado como fonte
🟠 Alto · `SetAttributes`

```cpp
} else if (type == INSTRUCTION_TYPE::BRANCH) {
    if (tokens[0].rfind("b.", 0) == 0) {
        source_registers.push_back(Register('G', 80));
    }
}
```
Só cobre o caso `b.xx` (lê CPSR). `cbz w0, label` / `cbnz w0, label` / `tbz w0, #3, label` testam um registrador explicitamente — e `br x0` / `blr x0` saltam para um endereço em registrador — nenhum desses registradores é capturado como fonte hoje. Comparar com o `InstructionRiscV.cpp`, que resolve isso de forma genérica (loop sobre todos os tokens, testando `IsRegister`) — vale considerar um padrão parecido aqui.

### [x] 3.4 `bl`/`blr` não marcam `x30` (link register) como destino **[NOVO]**
🟠 Alto · `SetAttributes`

`bl`/`blr` escrevem o endereço de retorno em `x30` implicitamente — isso nunca é adicionado a `dest_registers` em nenhum branch. Uma instrução depois que leia `x30`/`lr` (ex.: `ret` no fim da função, ou um `ldp x29,x30,[sp]` antes do `ret`) não vai enxergar essa dependência. (Esse mesmo padrão de "link register implícito não rastreado" se repete em MIPS — item 4.2 — e RISC-V — item 5.2.)

### [x] 3.5 Heurística `tokens[0].back() == 's'` para detectar variante que seta flags
⚪ Observação · `SetAttributes` — **mantida de propósito** (heurística correta para `adds`/`subs` e futuros `ands`/`bics`); o risco ficou documentado como quirk no testbench (`tb_InstructionArm64.cpp` 7.2, caso `fcvtzs`).

```cpp
if (tokens[0].back() == 's') dest_registers.push_back(Register('G', 80)); // ADDS atualiza CPSR.
```
Funciona hoje porque, nas listas atuais, só `adds`/`subs` terminam em `s`. É uma heurística razoável dado que ARM64 realmente nomeia variantes com flags terminando em `s` (`ands`, `bics`, etc. seguiriam o mesmo padrão) — só deixando registrado que, se vocês adicionarem `ands`/`bics`/`negs` (nem estão nas listas hoje — outra lacuna de cobertura menor) no futuro, o heurístico continua correto; mas qualquer opcode novo que termine em `s` por outro motivo (não relacionado a flags) quebraria silenciosamente essa suposição.

---

## 4. `InstructionMips32.cpp`

### [x] 4.1 `IsRegister` reconhece **qualquer** token com `$` na frente — inclusive labels locais do assembler **[NOVO]**
🔴 Crítico (para código gerado por `gas`/`gcc`) · função livre `IsRegister`

```cpp
static bool IsRegister(const std::string& token) {
    if (token.size() < 2) return false;
    if (token[0] != '$') return false;
    return true;
}
```

O GNU assembler gera rotineiramente labels locais como `$L2`, `$LC0`, `$LBB0_1` para alvos de desvio — **começam com `$`**. Com essa implementação, `IsRegister("$L2")` retorna `true`, então:

1. Em `NormalizeInstruction`, a lógica que preserva o case de labels (`!IsRegister(tokens[i])` → `continue`, ou seja, "só pula o lowercase se NÃO for registrador") passa a **lowercasear o label** (`$L2` → `$l2`), quebrando case-sensitivity do label.
2. Em `SetAttributes`, o `BRANCH` usa `tokens[i][0] == '$'` diretamente (nem chama `IsRegister` — é o mesmo problema duplicado) e tenta `LookupRegister("$l2", ...)` → **não existe na tabela → `std::abort()`**.

Ou seja: qualquer trace MIPS com labels locais `$`-prefixados (comum em saída de compilador, menos comum em traces didáticos manuscritos) derruba o parser.

**Fix:** validar contra a tabela de registradores em vez de só checar o prefixo:
```cpp
static bool IsRegister(const std::string& token) {
    return RegisterTable().count(token) > 0;
}
```
(isso também resolve o item 4.3 de bounds-check de quebra, pois passa a ser a fonte única de verdade — usar essa função tanto em `NormalizeInstruction` quanto em `SetAttributes`, em vez do `tokens[i][0]=='$'` cru.)

Compare com `InstructionSimplified.cpp`, que já usa a versão estrita (`R`/`F` + só dígitos) e além disso usa `IsRegister()` consistentemente em `SetAttributes::BRANCH` — o Simplified não tem esse problema.

**Implementado** com uma ressalva ao fix sugerido: a validação é **case-insensitive** (`tolower` numa cópia antes do `count`), porque o `NormalizeInstruction` testa `IsRegister` **antes** de aplicar lowercase no token — uma checagem estrita quebraria `BEQ $T1, ...` (deixaria `$T1` intacto e o `LookupRegister("$T1")` abortaria). `IsRegister` também passou a ser usado em todo o `SetAttributes` (BRANCH e genérico), eliminando os `tokens[i][0] == '$'` crus.

### [x] 4.2 `jal`/`bal`/`jalr` não marcam `$ra` como destino
🟠 Alto · `SetAttributes`

Mesmo padrão do item 3.4 (ARM64) e 5.2 (RISC-V): o `BRANCH` nunca popula `dest_registers`, então a escrita implícita em `$ra` por `jal`/`bal`/`bltzal`/`bgezal` não gera uma dependência rastreável para quem ler `$ra` depois (ex.: `jr $ra` no final da função).

**Implementado:** `jal`/`bltzal`/`bgezal` escrevem `$ra` (e `bltzal`/`bgezal` continuam lendo o registrador de teste); `jr` lê seu operando; `jalr` decide pelo número de operandos registradores — 1 operando (`jalr $rs`) → retorno em `$ra`; 2 operandos (`jalr $rd, $rs`) → retorno em `$rd` (sem `$ra` falso).

### [x] 4.3 Falta checar `tokens.size()` antes de indexar em `NormalizeInstruction` (LOAD/STORE)
🟡 Médio · `NormalizeInstruction`

```cpp
if (type == INSTRUCTION_TYPE::LOAD || type == INSTRUCTION_TYPE::STORE) {
    normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";
}
```
Se a linha vier truncada (ex.: `lw $t0, 4` sem base), `tokens.size()==3` e `tokens[3]` é acesso fora dos limites (UB). O `InstructionX86Intel.cpp` já faz esse tipo de checagem (`if (tokens.size() < 3) abort();`) antes de indexar — vale replicar o padrão aqui (e nos itens 5.3/6.1 abaixo, mesmo bug em RISC-V e Simplified).

**Implementado** com o mesmo formato de erro do x86 (`[ERRO] Instrução incompleta:`); a mensagem usa `tokens[0]` porque `instruction_string` ainda não está atribuída nesse ponto do `NormalizeInstruction`.

---

## 5. `InstructionRiscV.cpp`

### [x] 5.1 `RegisterTable()` só aceita nomes numéricos — faltam os apelidos ABI
🟠 Alto · `RegisterTable`

```cpp
for (int i = 0; i < 32; i++){
    t.emplace("x" + std::to_string(i), Register('L', i));
    t.emplace("f" + std::to_string(i), Register('F', 32 + i));
}
```
Só `x0`..`x31`/`f0`..`f31`. A esmagadora maioria do assembly RISC-V real usa os nomes ABI: `ra`(x1), `sp`(x2), `gp`(x3), `tp`(x4), `t0`-`t2`(x5-7), `s0`/`fp`(x8), `s1`(x9), `a0`-`a7`(x10-17), `s2`-`s11`(x18-27), `t3`-`t6`(x28-31). Nenhum é reconhecido por `IsRegister` (só aceita prefixo `x`/`f` + dígitos), e para operandos obrigatórios o código chama `LookupRegister` direto sem passar por `IsRegister` → `abort()` em `add a0, a1, a2`.

**Fix:** popular os apelidos na mesma tabela, mapeando para o `id`/`type` do registrador físico correspondente.

**Implementado:** todos os apelidos ABI (`zero`, `ra`, `sp`, `gp`, `tp`, `t0`-`t6`, `s0`/`fp`, `s1`, `s2`-`s11`, `a0`-`a7`) na `RegisterTable`. O `IsRegister` também foi trocado para validação na tabela (case-insensitive, mesmo padrão do 4.1) — sem isso os apelidos só funcionariam como destino e nunca como fonte em `add a0, a1, a2`.

### [x] 5.2 Faltam pseudo-instruções de controle de fluxo extremamente comuns **[NOVO]**
🔴 Crítico · listas de opcodes (`BRANCHES` principalmente)

```cpp
static const std::vector<std::string> BRANCHES
    {"beq", "bne", "blt", "bge", "bltu", "bgeu", "jal", "jalr"};
```
Faltam `j` (pseudo de `jal x0, offset` — desvio incondicional, extremamente comum) e `ret` (pseudo de `jalr x0, 0(x1)` — **a** forma padrão de retornar de função em RISC-V). Qualquer trace usando essas pseudo-instruções (a esmagadora maioria do assembly RISC-V escrito à mão ou gerado por compilador) falha em `IdentifyType` → abort. Também faltam `call`, `li`, `mv`, `nop` em geral (menos crítico que `j`/`ret`, mas mesma categoria).

**Fix:** adicionar `"j"` e `"ret"` a `BRANCHES` (tratando o registrador implícito como `x0` para `j`, e como leitura de `x1` para `ret`), e considerar mapear `li`/`mv`/`nop` para os opcodes reais equivalentes se quiserem cobertura completa.

**Implementado (cobertura completa):** `j`/`ret`/`call` em `BRANCHES` e `li`/`mv`/`nop` em `INT_BASIC`. Semântica no `SetAttributes`: `j` não escreve nem lê; `ret` lê `x1`; `call` escreve `x1`; `nop` nada; `li rd, imm` escreve rd; `mv rd, rs` escreve rd e lê rs.

### [x] 5.3 `jal`/`jalr` com `rd` explícito: o link register é classificado como **fonte**, não destino **[NOVO]**
🔴 Crítico · `SetAttributes`

```cpp
} else if (type == INSTRUCTION_TYPE::BRANCH) {
    for (size_t i = 1; i < tokens.size(); ++i)
        if (IsRegister(tokens[i]))
            source_registers.push_back(LookupRegister(tokens[i], ...));
}
```
Esse loop genérico trata **todo** token que parece registrador como fonte. Para `jal ra, label` (ou `jal x1, label`), `x1`/`ra` é o **destino** (recebe o endereço de retorno) — mas aqui é empurrado para `source_registers`. Diferente do caso "nunca rastreado" do MIPS/ARM64 (itens 4.2/3.4), aqui é ativamente classificado errado.

**Fix:** tratar `jal`/`jalr` como caso à parte — o primeiro registrador (quando presente, já que `jal offset` sozinho implica `rd=x1`) é destino; demais registradores (base de `jalr`) são fonte.

**Implementado:** casos dedicados para `jal`/`jalr` (rd explícito ou `x1` implícito como destino; `jalr` ainda captura `rs1`/base como fonte) e para `j`/`ret`/`call`, com o loop genérico restrito aos `beq`/`bne`/`blt`/`bge`/`bltu`/`bgeu`.

### [x] 5.4 Falta checar `tokens.size()` antes de indexar em `NormalizeInstruction` (LOAD/STORE)
🟡 Médio · mesmo padrão do item 4.3 — `normalized += tokens[1] + ", " + tokens[2] + "(" + tokens[3] + ")";` sem checagem prévia. **Implementado** com o mesmo padrão do 4.3.

---

## 6. `InstructionSimplified.cpp`

### [x] 6.1 Falta checar `tokens.size()` antes de indexar em `NormalizeInstruction` (LOAD/STORE)
🟡 Médio · mesmo padrão dos itens 4.3/5.4. **Implementado** com o mesmo padrão de erro dos demais.

Fora isso, esse arquivo está o mais robusto do lote: `IsRegister` é estrito (`R`/`F` + só dígitos) e `SetAttributes::BRANCH` já usa `IsRegister()` em vez de checar o primeiro caractere cru — ou seja, não tem os problemas dos itens 4.1 (MIPS) nem herda ambiguidade de labels. Nenhum outro problema novo encontrado aqui.

---

## 7. Nitpicks / observações gerais (baixa prioridade)

### [x] 7.1 `NormalizeInstruction` de MIPS32/Simplified: segunda passada de lowercase redundante em `BRANCH`
⚪ Nitpick — **resolvido**: o segundo loop agora só monta a string (o primeiro já normalizou tudo). O RISC-V já estava limpo — não tinha essa redundância.

### [x] 7.2 `lea` classificado como `LOAD`
⚪ Observação · `InstructionX86Intel.cpp` — **resolvido**: `lea` foi movido para `INT_BASIC` com caso dedicado no `SetAttributes` (dest + registradores do endereço como fontes, sem EFLAGS) e `SetLatencies` só atribui `mem_latency` a LOAD/STORE — não concorre mais pela FU de memória.

### [x] 7.3 Padrão `static const ... RegisterTable();` declarado no header
⚪ Observação · `Instruction.h` — **mantido intencionalmente** (decisão de design): funciona corretamente hoje, e a migração para método virtual puro (mais convencional) é cosmética sem ganho de corretude. Fica registrado o alerta: se um dia migrarem para *unity build*, quebra — reavaliar na ocasião.

---

## Ordem sugerida

1. **3.1** (`ldp`/`stp` no ARM64) e **2.2** (`div`/`idiv`/`mul` no x86) — bugs silenciosos de dependência em instruções extremamente comuns.
2. **4.1** (MIPS `$`-label) e **5.2**/**5.1** (RISC-V sem `j`/`ret`/apelidos ABI) — impedem parsear trace realista.
3. **2.1** (`cmp`/`test`/`comiss`), **5.3** (`jal` rd como fonte), **3.4**/**4.2** (link register não rastreado) — corretude de dependência.
4. **3.2** (`sp`/`lr` no ARM64), **2.5** (`rip` no x86) — cobertura de nomes.
5. Bounds-check (4.3/5.4/6.1), `GetCurrentRS` (1.1), `2.3`, `2.4`, `3.3`, `3.5` — refinamentos.
6. Seção 7 — quando sobrar tempo.
