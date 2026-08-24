# Guia de Implementação — Decodificação Multi-Arquitetura

## Objetivo

Fazer o simulador suportar, de forma robusta, a decodificação e
passagem de parâmetros de instruções assembly de múltiplas
arquiteturas (MIPS32, x86, ARM64, etc.), organizando as instruções
em uma tabela (formato Tomasulo / pipelining), sem armazenamento
de dados por enquanto.

## Como usar este documento

O documento está dividido em duas partes:

- **Parte 1 — Metodologia Geral**: válida para qualquer arquitetura
  nova. Não deveria precisar ser reescrita a cada arquitetura —
  só refinada quando surgir uma lição nova (como aconteceu com o
  padrão de agrupamento `OP_SHAPE`, extraído do MIPS Simplificado).
- **Parte 2 — Estado Atual**: dados, decisões e status específicos
  da arquitetura em foco no momento. Essa parte é reescrita/zerada
  a cada nova arquitetura.

---

# PARTE 1 — Metodologia Geral (válida para qualquer arquitetura)

## Metodologia adotada

1. **Levantar a sintaxe completa e o comportamento esperado** de
   cada instrução antes de tocar em código (documento de
   referência, ex.: `_simplified_sintax.txt`).
   - Não vale apenas listar o que a fonte apresenta explicitamente:
     **procurar ativamente por instruções "irmãs"/complementares**
     que costumam faltar em levantamentos rápidos. Lição real: as
     instruções que *escrevem* em `HI`/`LO` (`mult`, `div`, ...)
     estavam listadas, mas as que *leem* esse resultado (`mflo`,
     `mfhi`) só apareceram numa checagem cruzada posterior.
   - Perguntas-guia para essa checagem: "toda instrução que escreve
     num registrador implícito tem uma contraparte que lê dele?";
     "toda variante `unsigned`/`doubleword`/`linked` de uma
     instrução comum foi listada?"; "existe uma instrução genérica
     (`load`/`store`) que precisa reconciliar múltiplos tipos de
     destino?".
   - Quando um opcode tem sintaxes estruturalmente muito diferentes
     (aridade ou natureza de operandos distintos), tratar cada
     variante como instrução separada na decodificação (ex.:
     `jalr` com 1 ou 2 registradores).

2. **Implementar a decodificação e passagem de parâmetros
   individualmente**, instrução por instrução — sem tentar
   generalizar ainda. Isso se divide em duas partes que **não**
   devem ser confundidas entre si:

   **a) Elementos estruturais/genéricos** (escritos uma única vez,
   compartilhados por toda a arquitetura desde o início):
   - `RegisterTable()`: tabela `nome → Register`, incluindo aliases
     de hardware (ex.: `ra` = `r31`) e registradores
     especiais/implícitos (ex.: `hi`/`lo`).
   - `MakeCDB()`: monta o banco de registradores físicos
     (`FillCDB()` por classe/máscara) e define `print_banks`
     (faixas usadas na impressão/debug da tabela de registradores).
   - `SplitInstruction()`: tokenizador. Definir o conjunto de
     delimitadores com cuidado — caracteres que parecem
     delimitadores podem fazer parte do opcode (ex.: `.` em
     `add.d`, `l.s`, `cvt.w.d` no MIPS simplificado **não** é
     delimitador).
   - Helpers de classificação de token: `IsIntReg`, `IsFloatReg`,
     `IsImmediate` (signed/unsigned), `IsOffset`, `IsLabel`,
     `LookupReg`, `ToLower` — validam/resolvem um token sem saber
     qual opcode está sendo tratado.
   - `IdentifyType()`: mapeia opcode → `INSTRUCTION_TYPE` via
     listas de opcodes por categoria (`ContainsOpcode`).

   **b) Tratamento individual por instrução** (o que de fato é
   "um por um" nessa fase):
   - Dentro de `ValidateInstruction()` e `SetAttributes()`, cada
     opcode recebe seu próprio `if (op == "...")`/`case` —
     **mesmo que dois opcodes tenham sintaxe idêntica, eles ficam
     duplicados de propósito nessa fase**. Resistir à tentação de
     agrupar aqui: o agrupamento é a Etapa 4, feito só depois de
     validar caso a caso.

3. **Testbench**: valida que a sintaxe esperada é aceita, que os
   registradores/latências resultantes estão corretos, e que
   sintaxes incorretas são rejeitadas. Formato confirmado em
   `tb_ArchSimplified.cpp` + `tb_Helpers.h`:

   **a) Infraestrutura compartilhada — `tb_Helpers.h`** (não
   reimplementar por arquitetura; incluir esse header em todo
   testbench novo):
   - `check(nome, condição)`: registra OK/FALHOU no console e
     incrementa os contadores globais `passed`/`failed`.
   - `has_reg(regs, type, id)`: verifica presença de um registrador
     `(type, id)` num vetor — usar em vez de comparar contagem
     exata, já que sombreamento de sub-registradores (ex.:
     aliases/flags em arquiteturas como x86) pode adicionar
     variantes mascaradas.
   - `only_ids(regs, {ids...})` / `no_type(regs, type)`: predicados
     auxiliares para arquiteturas com registradores de tipos extras
     (ex.: flags de CPU) que não devem aparecer onde não são
     esperados.
   - `section(nome)` / `print_title(nome)`: só formatação de saída
     no console, não afetam o resultado do teste.

   **b) Casos de sucesso — testar por `OP_SHAPE`, não por opcode.**
   Como o agrupamento (Etapa 4) garante que opcodes do mesmo
   `OP_SHAPE` têm validação/atributos idênticos, o testbench cobre
   **um opcode representativo por shape**, documentando no título
   da seção quantos opcodes aquele caso representa (ex.: "andi/sll
   — representa 9 opcodes iguais"). Isso evita ~96 blocos de teste
   quase idênticos sem perder cobertura real. Estrutura por caso:
   1. `Parse()` da sintaxe válida (incluindo variantes relevantes,
      como offset negativo ou imediato negativo, quando fizerem
      sentido para o shape).
   2. Checar `GetInstructionType()`.
   3. Checar `GetDestRegisters()` / `GetExSourceRegisters()` /
      `GetMemSourceRegisters()` (presença **e** ausência onde
      aplicável — ex.: "dest vazio" para instruções que não
      escrevem registrador).
   4. Checar `GetExLatency()`/`GetMemLatency()` quando relevante.

   **c) Casos de rejeição (sintaxe inválida) — bloco separado,
   manual.** Como `Parse()`/`ValidateInstruction()` falham hoje via
   `std::abort()` (mata o processo inteiro), **não** dá para
   automatizar vários casos de falha no mesmo `main()` sem um
   mecanismo de "death test" (processo filho + checagem de exit
   code). A solução adotada: manter esses casos **comentados**, ao
   final do arquivo, um `Parse()` inválido por bloco, com a
   instrução de descomentar e rodar **um de cada vez**
   manualmente. Se um dia compensar automatizar (`fork()` +
   `waitpid()`, ou trocar `abort()` por exceção só em modo de
   teste), documentar a mudança aqui.

   **d) Seções obrigatórias além das categorias de instrução** (ver
   `tb_ArchSimplified.cpp` como referência de estrutura completa):
   1. *Tolerância de formatação/case* — espaçamento variado, tabs,
      maiúsculo/minúsculo no opcode e nos registradores; confirmar
      que a `instruction_string` canônica é idêntica independente
      da formatação de entrada.
   2. Uma seção por categoria de instrução, com o(s) `OP_SHAPE`
      representativo(s) de cada uma.
   3. *Normalização / `instruction_string`* — padding do opcode,
      preservação de caixa em labels, formato de endereçamento
      load/store reconstruído.
   4. *Integração* — múltiplas instruções em sequência, confirmando
      que uma instância não vaza estado (`dest`/`source`) para
      outra.
   5. (Manual, comentado) *Casos de falha* — um `Parse()` inválido
      por bloco, para rodar isoladamente.

   Como o testbench é organizado por `OP_SHAPE` desde o início, ele
   continua válido sem modificação depois do agrupamento (Etapa 4)
   — é o próprio critério de que agrupar não mudou comportamento.

4. **Agrupamento/generalização**, somente após tudo funcionando
   caso a caso. Padrão usado (extraído da comparação entre a
   versão pré e pós-agrupamento do MIPS Simplificado):
   - Criar um `enum class OP_SHAPE` que agrupa opcodes por sintaxe
     **e** semântica idênticas (mesma aridade, mesmos tipos de
     operando, mesmo efeito sobre destino/fonte). Nomear cada valor
     de forma descritiva (ex.: `INT_3REG`, `INT_2REG_IMM_S`,
     `MULDIV_2REG_HILO`).
   - Criar um `static const std::unordered_map<std::string,
     OP_SHAPE> OPCODE_SHAPE` ligando cada opcode ao seu `OP_SHAPE`.
   - Reescrever `ValidateInstruction()` e `SetAttributes()` como um
     único `switch (OPCODE_SHAPE.at(op))` cada, eliminando os
     `if (op == "...")` duplicados da Etapa 2b.
   - **Não agrupar opcodes que "parecem" iguais mas têm efeito
     colateral diferente** (lição aprendida: `mul` escreve em `rd`;
     `mult`/`multu` escrevem em `HI`/`LO` — mesma "família" na doc,
     `OP_SHAPE` diferente).
   - Depois de agrupar, rodar de novo o testbench da Etapa 3 sem
     alterações — ele deve continuar passando sem modificação
     (esse é o critério de que o agrupamento não mudou
     comportamento, só reduziu duplicação).

## Passos de integração (fora da decodificação pura)

> A confirmar/detalhar: os arquivos revisados até agora cobrem
> apenas `Instruction`/decodificação, não o restante do pipeline.

- [ ] Criar o par de arquivos seguindo o padrão de nomenclatura e
      include guard existente: `Architectures/<Nome>.cpp` +
      `Architectures/headers/<Nome>.h`, incluindo
      `../../headers/Architecture.h`.
- [ ] Registrar a nova subclasse de `Instruction` no ponto em que
      as arquiteturas são selecionadas/instanciadas (mencionado em
      `Architecture.h` como `InstructionFactory` — confirmar o
      mecanismo exato quando esse arquivo for revisado).
- [ ] Verificar se a nova arquitetura precisa de latências
      (`ex_latency`/`mem_latency`) diferentes das padrão.
      **Atenção**: `Instruction::base_ex_latencies` e
      `base_mem_latencies` são `static` na classe-base e
      compartilhados por **todas** as arquiteturas já
      implementadas — alterá-los afeta todas de uma vez. Latências
      únicas por instrução usam `SetExLatency()`/`SetMemLatency()`
      na instância, não os vetores estáticos.

## Cuidados / "gotchas" gerais

- A declaração `static` de `RegisterTable()` no header gera warning
  de função não usada em módulos que só incluem o header sem
  implementá-la — por isso o
  `#pragma GCC diagnostic push/ignored "-Wunused-function"` ao
  redor da declaração em `Architecture.h`. Manter esse padrão ao
  copiar a estrutura para uma nova arquitetura.
- `FillCDB()`, `IsRegister()` e `ContainsOpcode()` já existem em
  `Architecture.cpp`/`.h` — não reimplementar por arquitetura.
- Preservar a convenção de comentários `// Público:` / `//
  Privado:` acima de cada método e o estilo Doxygen (`@brief`,
  `@details`, `@param`, `@return`) nos headers, para manter a
  padronização entre arquiteturas.
