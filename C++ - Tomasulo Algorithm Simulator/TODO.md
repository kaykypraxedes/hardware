# TODO — Projeto Tomasulo (Checklist para Finalização)

## 📋 Itens Críticos (pré-GitHub)
- [x] **LICENSE** — Adicionar arquivo de licença (MIT recomendado)
- [x] **`.gitignore`** — Ignorar build/, executable, *.o, /tmp/

## 🔧 Refatorações Seguras (não quebram testes)

### Extrair lambdas em ReservationStations.cpp
- [x] `buscar` (procuraUFlivre, linha 223) → método `RS::alocarUFLivre(grupo, ciclo)`
- [x] `liberar_em` (liberarUF, linha 260) → método `RS::desalocarUFdoGrupo(grupo, ciclo)`

### Consolidar sorts inline em Thread.cpp
- [x] Sort de `vector<Evento>` (detectarTransicoesDeFase, linha 301) → método
- [x] Sort de `buffer_WB` (realizarWriteResult, linha 211) → método

### Adicionar bounds validation
- [x] `ReservationStations.cpp:100-107` — validar `getId() < NUM_REGISTRADORES` em Qj/Qk (regJ)
- [x] `ReservationStations.cpp:109-116` — validar `getId() < NUM_REGISTRADORES` em Qj/Qk (regK)
- [x] `ReservationStations.cpp:147-148` — validar `getId() < NUM_REGISTRADORES` em dest
- [x] `Thread.cpp:281-288` — validar `getId() < NUM_REGISTRADORES` em broadcastCDB

### Unificar switches de mapeamento tipo→grupo RS/UF (IGNORADO)
- [-] `Thread.cpp:110-119` — switch Issue() → lookup table (IGNORADO: preferência por switches legíveis)
- [-] `ReservationStations.cpp:241-248` — switch FindFreeFU → lookup table (IGNORADO)
- [-] `ReservationStations.cpp:272-279` — switch ReleaseFU → lookup table (IGNORADO)
- [-] `Thread.cpp:248-261` — if-else WriteBackNormal → lookup table (IGNORADO)

## 🏗️ Melhorias Arquiteturais

### Encapsulamento
- [x] Criar setters para `tabela_de_instrucoes[pc].ciclo_*`
      (`registrarIssue()`, `adicionarCicloEX()`, `adicionarCicloMEM()`, `definirWR()`)
- [x] Encapsular `buffer_WB` / `buffer_WB_pendente`
      (`flushBufferWBPendente()`, `proximoWB()`, `removerWB()`, `adicionarWBPendente()`)

### Performance (cópias profundas)
- [x] `getCDB()` → `const CDB&` (Thread.h)
- [x] `getRS()` → `const ReservationStations&` (Thread.h)
- [x] `getUF()` → `const UnidadesFuncionais&` (Thread.h)
- [x] `getTabela()` → `const vector<LinhaTabela>&` (Thread.h)
- [x] `getThread()` → `const Thread&` (Processador.h)
- [x] `getTabelaThread()` → `const vector<LinhaTabela>&` (Processador.h)
- [x] Criar métodos `get*Copia()` em Main.cpp para impressão

### Clareza do pipeline
- [ ] Separar `Wr()` em etapas explícitas (flush → writeResult → detectTransicoes)
- [ ] Considerar `enum FaseInstrucao` + estado na RS em vez de `buffer_WB` duplo

## 🧹 Limpeza de Código
- [-] Remover código morto: `FU::id`, `ToggleBusy()`, `GetCycleTable()` (IGNORADO: mantido para implementações futuras)
- [x] Inicializar `Register::id` com valor padrão em `Componentes.h`
- [x] Extrair helpers `check()` / `secao()` / `passou`/`falhou` para header compartilhado nos testbenchs

---

## 🔧 Refatorações de Código (Alta Prioridade)

### Reorganizar métodos com rótulos `// Público:` / `// Privado:` e seções padronizadas

Reorganizar cada `.cpp` seguindo este padrão:

**1. Cabeçalhos de seção** no formato `// ─── NOME ───` com traços até o limite:
- `// ─── GETTERS ───` — todos os getters simples (cada um com seu rótulo individual)
- `// ─── CONSTRUTORES ───` — construtores
- `// ─── DEMAIS MÉTODOS ───` — demais métodos (ou seções específicas para Thread)

**2. Rótulo individual** `// Público:` ou `// Privado:` na linha imediatamente anterior a **cada** definição de método, sem exceção (inclusive getters de uma linha).

**3. Comentários descritivos existentes** preservados entre o rótulo e a assinatura.

**4. Remover** `// Códigos pequenos:` e `// Códigos grandes:` (substituídos pelas seções acima).

**5. Ordem dos métodos dentro de `// ─── DEMAIS MÉTODOS ───`** varia por módulo:

| Módulo | Estrutura |
|--------|-----------|
| `Components.cpp` | Privados antes dos públicos (pois são chamados pelo construtor) |
| `Instruction.cpp` | Privados antes dos públicos (pois são chamados pelo construtor) |
| `Processor.cpp` | Público wrapper → privados helpers (ordem sequencial de execução) |
| `ReservationStations.cpp` | Público wrapper → privados helpers (ordem sequencial: AddIssue → UpdateDependencies → UpdateCountdown → ResolveDependency → Release) |
| `Thread.cpp` | Dividido em seções por estágio da pipeline: `// ─── ISSUE ───`, `// ─── EX/MEM ───`, `// ─── WR ───`, `// ─── COMMIT ───`. Dentro de cada estágio: público wrapper → privados helpers. |

Nenhuma lógica, assinatura ou header é alterado — apenas comentários e ordem das definições.

Ordem de implementação (do menos para o mais complexo):
1. `Components.cpp` — 118 linhas, 6 métodos (✓ concluído)
2. `Instruction.cpp` — 149 linhas, 10 métodos
3. `Processor.cpp` — 104 linhas, 5 métodos
4. `ReservationStations.cpp` — 289 linhas, 11 métodos
5. `Thread.cpp` — 466 linhas, ~24 métodos

- [x] `Components.cpp`
- [x] `Instruction.cpp`
- [x] `Processor.cpp`
- [x] `ReservationStations.cpp`
- [x] `Thread.cpp`

### Quebrar métodos superlotados em funções menores

Após a reorganização, dividir métodos que acumulam múltiplas responsabilidades em funções menores e mais coesas. Exemplos identificados:

- `Thread::Wr()` — flush + write result + detecção de transições em um único método
- `Thread::PerformWriteResult()` — STORE c/ ROB e WriteBackNormal no mesmo fluxo
- `Thread::WriteBackNormal()` — broadcast CDB + liberação de RS (por registro ou PC) num switch de 8 branches
- `Thread::BroadcastCDB()` — percorre 6 grupos chamando `FindWBInGroup`, que por sua vez varre cada RS e resolve dependências
- `Thread::Commit()` — lógica de STORE c/ ROB, BRANCH e demais instruções misturadas
- `ReservationStation::AddIssue()` — leitura Qj/Qk + auto-WAR + alocação CDB em ~80 linhas
- `ReservationStation::UpdateDependencies()` — resolução de dependências + alocação de FU no mesmo método
- `Processor::ExecuteIssue()` — política de escalonamento (fine-grained, SMT) misturada com a lógica de despacho

Cada método listado deve ser analisado individualmente e, quando houver coesão temática, extraído em 2+ métodos menores com nomes descritivos.

### Demais itens

- [ ] **`std::vector::erase(begin())` → `std::deque`** — `wb_buffer` e `pending_wb_buffer` usam `front()`/`erase(begin())` O(n). Substituir por `std::deque<int>` ou `std::queue<int>`
- [ ] **Namespace global → `namespace tomasulo`** — Todo o código polui o escopo global; encapsular em `namespace tomasulo`
- [ ] **Main.cpp: duplicação do CDB printing** — Código de `cdb.F[j]`/`cdb.R[j]` (linhas 364-420) replica manualmente a lógica do template `PrintStructure`
- [ ] **Comentários ruidosos: `// const & para evitar cópia`** — Remover de getters que retornam `int`/`bool`/`char`/`enum` (tipos pequenos onde const& não traz ganho)
- [ ] **`Processor` construtor: `num_fus` por valor** — `Processor.h:38` recebe `std::vector<int>` por valor, inconsistente com `num_rs` e `switch_instructions` (const&). Mudar para `const std::vector<int>&`
- [ ] **`Instruction.cpp`: include desnecessário** — Inclui `Components.h` sem usar diretamente (já incluso via `Instruction.h`)
- [ ] **`Register::IdentifyId()` sem proteção `stoi`** — `Components.cpp:117` usa `std::stoi(s.substr(1))` sem try/catch; string malformada quebra sem mensagem clara

## 🟡 Melhorias Arquiteturais (Média Prioridade)

- [ ] **`CYCLE_LIMIT = 10000` hardcoded** — Tornar configurável ou aumentar para valor seguro (~1M) com flag de warning no output
- [ ] **STORE com ROB: lógica de commit frágil** — `Thread.cpp:370-378` usa `mem_cycles.empty()`/`size()==1` como flag de estado. Extrair para `enum STORE_COMMIT_STATE { IDLE, WAITING_MEM, READY }`
- [ ] **`Thread::has_predictor` declarado mas nunca setado** — `Thread.h:88` existe o campo, mas nenhum construtor o inicializa (sempre `false`). `Processor` deve passar o flag para `Thread`
- [ ] **Magic number `6` espalhado** — `Thread.cpp:70`, `Thread.cpp:78` usam `num_rs.size() >= 6`. Extrair para `static const int NUM_RS_GROUPS = 6` e `NUM_FU_GROUPS = 6`

## ⏳ Funcionalidades Incompletas (ADIADO — para próxima fase)

Esses itens existem na interface (`enum`, construtores) mas não têm implementação real. Serão abordados em uma fase futura de expansão:

- [ ] **`PROCESSOR_TYPE::IN_ORDER`** — Pipeline clássico de 5 estágios (sem Tomasulo), dispatch width limitado a 1, stalls estruturais diferenciados
- [ ] **`MULTITHREADING_MODEL::COARSE_GRAINED`** — Troca de contexto baseada em `switch_instructions` (contagem de instruções). Atualmente comporta como FINE_GRAINED
- [ ] **Branch prediction real** — Implementar BTB / tabela de histórico. Atualmente `has_predictor` só evita parar dispatch após BRANCH, sem lógica preditiva
- [ ] **Flush especulativo sem ROB** — Quando BRANCH sem ROB completa, limpar das RSs as instruções emitidas após o BRANCH (thread entra WAITING mas RSs ficam ocupadas)

## 🐛 Observações Adicionais (baixa prioridade)

- [ ] **Header com excesso de métodos privados**: `Thread.h` expõe ~30 métodos privados — muitos poderiam ser funções livres ou agrupados em `detail` namespace
- [ ] **`Instruction::base_ex_latencies/base_mem_latencies` estáticos**: Qualquer mudança nas latências afeta todas as instruções globalmente — poderia ser um parâmetro de construção
- [ ] **`Register::id` no construtor default**: Permanece sem inicialização (`int id;`), enquanto `type` tem `{'Z'}`
- [ ] **`dest_pending_in_cdb` mistura português e inglês** — Renomear para `dest_pending_in_cdb` (consistente) ou `dest_pending_on_cdb`

## ✅ Já Concluído (Fases 1-2 + infraestrutura)

### Reorganização de pastas
- [x] `_code/` → `Code/`
- [x] `test_cases/` → `Code/test-cases/`
- [x] Makefile e build/ movidos para Code/
- [x] Paths e documentação atualizados

### Fase 1 — Mudanças cosméticas
- [x] Código morto identificado (mantido como pendência técnica)
- [x] Comentários desatualizados corrigidos
- [x] Números mágicos → constexpr
- [x] Validação adicionada em addIssue() e definirLatenciaEspecifica()
- [x] Includes e separadores padronizados

### Fase 2 — Nomenclatura e assinaturas
- [x] Parâmetros: valor → const& (identificaTipo, identificaId, alocarRS, construtores)
- [x] 12+ funções renomeadas (Issue, ExMem, Wr, Commit, broadcastCDB, etc.)
- [x] Interface de construtores simplificada (num_ufs com 6 elementos, capacidade_rob como parâmetro)

### Limpeza de testbenchs
- [x] Debug prints removidos de tb_Thread.cpp

### Fase 3 — Padronização de idioma e formatação
- [x] Classes, structs e enums traduzidos para inglês
- [x] Métodos e funções renomeados para PascalCase inglês
- [x] Variáveis e campos renomeados para snake_case inglês
- [x] `constexpr` → `static const`
- [x] Comentários preservados em português (elementos passivos)
