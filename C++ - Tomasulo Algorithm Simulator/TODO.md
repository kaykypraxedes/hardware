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
- [ ] `Thread.cpp:110-119` — switch Issue() → lookup table
- [ ] `ReservationStations.cpp:241-248` — switch procuraUFlivre → lookup table
- [ ] `ReservationStations.cpp:272-279` — switch liberarUF → lookup table
- [ ] `Thread.cpp:248-261` — if-else writeBackNormal → lookup table

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
- [ ] Remover código morto: `FU::id`, `ToggleBusy()`, `GetCycleTable()`
- [ ] Inicializar `Register::id` com valor padrão em `Componentes.h`
- [ ] Extrair helpers `check()` / `secao()` / `passou`/`falhou` para header compartilhado nos testbenchs

## 📖 Documentação
- [ ] Adicionar badges e pré-requisitos ao README.md
- [ ] Criar versão inglesa do FULL_DOCUMENTATION
- [ ] Adicionar seção "Known Issues" / "Future Work" ao README

---

## 🐛 Observações Adicionais (baixa prioridade)

- [ ] **Comentários ruidosos**: `// & const para evitar cópia (tipos pequenos teriam um ganho marginal)` aparece 15+ vezes em getters de tipos pequenos (int, bool, enum) — comentário enganoso
- [ ] **Duplicação em Main.cpp**: Código de impressão do CDB (`cdb.F[j]` / `cdb.R[j]`) duplica manualmente a lógica do template `imprimirEstrutura`
- [ ] **Header com excesso de métodos privados**: `Thread.h` expõe ~30 métodos privados — muitos poderiam ser funções livres ou agrupados em `detail` namespace
- [ ] **`Instruction::base_ex_latencies/base_mem_latencies` estáticos**: Qualquer mudança nas latências afeta todas as instruções globalmente — poderia ser um parâmetro de construção
- [ ] **`buffer_WB` como `std::vector`**: Operações `front()`/`erase(begin())` são O(n) — `std::deque` ou `std::queue` seriam mais adequados
- [ ] **Sem namespaces**: Todo o código está no namespace global — usar `namespace tomasulo`
- [ ] **`Register::id` no construtor default**: Permanece sem inicialização (`int id;`), enquanto `type` tem `{'Z'}`

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
