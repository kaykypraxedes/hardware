# TODO — Projeto Tomasulo (Checklist para Finalização)

## 🔧 Refatorações Pendentes
###   Importantes
- [x] **Variáveis em português** — Renomeadas: `tipo`→`type`, `escritas`→`writes`, `linha`→`row`, `fim_mem`→`mem_end`, `mesmoRegistrador`→`SameRegister`, `VecStr`→`FormatCycles`, etc.
- [x] **TABLE_ROW sentinelas inconsistentes** — Struct declara `-1` como "não definido", mas construtor da Thread passa `0`. `CycleStr()` trata `0` como vazio. Unificar para `-1`.
- [-] **Thread::Commit() inchado** — Extrair STORE commit state machine (IDLE→WAITING_MEM→READY) para `TryCommitStore()`.
- [x] **Thread: six-way group iteration boilerplate** — Iteração manual sobre `rs.load`, `rs.store`, `rs.int_basic`, etc. repete-se em 4 lugares. Armazenar grupos em `std::array` e usar loop.
- [x] **Thread: registro de EX split entre callsites** — Início em `TryAdvanceRS()`, fim em `ProcessTransition()`. Consolidar num único local.
- [x] **ReservationStations: `FindFreeFU()` declarado `const` mas com side effect** — Aloca FU via referência. Remover `const` ou renomear para `FindAndAllocateFreeFU()`.
- [ ] **Main.cpp: `PrintFU()` acessa campos da struct diretamente** — Quebra padrão de getters usado no resto do projeto. Adicionar getters em `FU` ou usar template.
- [x] **ReservationStations: FU-group mapping duplicado** — `FindFreeFU()` e `ReleaseFU()` têm switch/if-else idêntico mapeando tipo de instrução → grupo de FU. Extrair para lookup table ou helper `SelectFUGroups()`.
- [x] **Main.cpp: nested `while(getline)` no programa** — Bloco `"programa"` lê da mesma `std::cin` que o loop externo; se não for a última seção, chaves seguintes são engolidas. Aceitar `std::istream&` ou extrair leitura para função separada.

###   Cosméticas
- [x] **Thread: type→RS group mapeamento duplicado** — `Issue()` usa `switch`, `WriteBackNormal()` usa `if-else` para o mesmo mapeamento. Extrair `GetGroupForType()`.
- [x] **Thread::PerformWriteResult(): fluxo assimétrico** — `continue` no STORE c/ ROB pula `RemoveWB()` e contador; `WriteBackStoreWithROB()` faz isso internamente. Tornar simétrico com if-else.
- [x] **Código morto: `AddWB()`, `num_stalls`, `num_ufs_por_tipo`** — Declarados mas nunca usados. Remover.

## ⏳ Funcionalidades Incompletas (ADIADO)

- [ ] **`PROCESSOR_TYPE::IN_ORDER`** — Pipeline clássico de 5 estágios (sem Tomasulo), stalls estruturais diferenciados, etc.
- [x] **`MULTITHREADING_MODEL::COARSE_GRAINED`** — Troca de contexto baseada em `switch_instructions` (contagem de instruções). Atualmente comporta como FINE_GRAINED
- [ ] **Flush especulativo sem ROB** — Quando BRANCH sem ROB completa, limpar das RSs as instruções emitidas após o BRANCH (thread entra WAITING mas RSs ficam ocupadas)
- [ ] **Desdobramento de loop simulado** — Identificação do label indicado pelo branch e desenrolamento do caminho completo da instrução (indicando quantas vezes o loop será feito antes de a comparação ser falsa). Nessa mesma alteração será simulado o flush dos RSs e previsores de desvios baseados em máquinas de estados de 1 bit, 2 bits, que o desvio sempre será tomado ou que o desvio nunca será tomado (mais abrangente pra tornar a simulação ainda mais completa).
- [ ] **Suporte completo à códigos reais de assembly** — Atualmente o suporte é basicamente apenas à trechos de assembly, mas há a ideia de expansão para códigos mais completos com elementos não convenvionais para simulações de Tomasulo, como `sections`, instruções como `mov`, `syscalls`, etc., todos com suas latências adaptadas.

