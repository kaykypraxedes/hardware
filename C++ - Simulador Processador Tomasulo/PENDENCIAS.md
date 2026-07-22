# Pendências — Refatoração

## Fase 1 — Mudanças Pequenas (cosméticas e pontuais)

### 1.1 Código morto (mantido como pendente — pode ser funcionalidade não implementada)
- [ ] `_codigo/headers/Componentes.h:73` — Campo `UF::id` declarado em struct, setado em `UF{}`, mas nunca lido
- [ ] `_codigo/headers/Processador.h:22-25` — Struct `LinhaCiclos` declarada; método `getTabelaCiclos()` declarado em `Processador.h:45`, nunca implementado
- [ ] `_codigo/headers/Componentes.h:22` + `_codigo/Componentes.cpp:19` — Método `Registrador::trocaBusy()` existe, nunca chamado
- [ ] `_codigo/headers/Processador.h:60` — Campo `num_ufs_por_tipo` declarado, nunca lido/usado

### 1.2 Comentários desatualizados (resolvidos)
- [x] `Thread.h:64` — Remover `// ← novo` ao lado de `buffer_WB_pendente`
- [x] `Main.cpp:1-3` — Atualizar comentário de execução (`../teste.txt` → `test_cases/inputs/`)
- [x] `Processador.cpp:78-79` — Corrigir comentário: só rotaciona para SMT, não GRANULACAO_FINA
- [x] `Processador.cpp:92` — Remover `// FIX: separado...` (FIX já resolvido)
- [x] `Thread.cpp:325` — Remover `// Mover para executarWr()...` (já implementado)

### 1.3 Números mágicos → constantes nomeadas (resolvidos)
- [x] `NUM_REGISTRADORES=32` — `_codigo/headers/Componentes.h:8` (constexpr)
- [x] `LIMITE_CICLOS=10000` — `_codigo/Main.cpp:14` (constexpr)
- [x] `CAPACIDADE_ROB=30` — `_codigo/headers/Thread.h:10` (constexpr)

### 1.4 Adicionar validação simples (resolvidos)
- [x] `_codigo/ReservationStations.cpp:54,66` — `addIssue()` valida `getId() < NUM_REGISTRADORES` antes de acessar `cdb.F[regJ.getId()]` / `cdb.R[regK.getId()]`
- [x] `_codigo/Thread.cpp:86` — `definirLatenciaEspecifica()` valida `posicao` no vetor

### 1.5 Inconsistências de includes e estilo (resolvidos)
- [x] `<cctype>` removido de `_codigo/headers/Instrucao.h` (não usado lá)
- [x] Separadores padronizados: `// ====` → `// ────` em `_codigo/Main.cpp`

---

## Fase 2 — Nomenclatura e Assinaturas

### 2.1 Parâmetros: valor → `const&` (resolvido)

**`identificaTipo(std::string&)` → `const std::string&`:**
- [x] **Decl**: `_codigo/headers/Componentes.h:58-60` (privado de `Registrador`)
- [x] **Def**: `_codigo/Componentes.cpp:102-110`
- [x] **Caller**: `_codigo/Componentes.cpp:11` (em `Registrador::Registrador(std::string)`)

- [x] **Decl**: `_codigo/headers/Instrucao.h:78-80` (privado de `Instrucao`)
- [x] **Def**: `_codigo/Instrucao.cpp:71-113`
- [x] **Caller**: `_codigo/Instrucao.cpp:45` (em `stringParaInstrucao`)

**`identificaId(std::string&)` → `const std::string&`:**
- [x] **Decl**: `_codigo/headers/Componentes.h:61-63` (privado de `Registrador`)
- [x] **Def**: `_codigo/Componentes.cpp:112-116`
- [x] **Caller**: `_codigo/Componentes.cpp:12`

**`alocarRS(std::string&, int)` → `alocarRS(const std::string&, int)`:**
- [x] **Decl**: `_codigo/headers/Componentes.h:39-42` (público de `Registrador`)
- [x] **Def**: `_codigo/Componentes.cpp:74-82`
- [x] **Callers**: `_codigo/ReservationStations.cpp:81,82` (em `addIssue`) + `_codigo/ReservationStations.cpp:147,148` (em `atualizarDependencias`)

**Construtores com `std::vector` por valor:**
- [x] `Thread::Thread(vector<string>, bool, vector<int>, vector<int>)` — `Thread.h:34-39` + `Thread.cpp:11-23`
- [x] `Thread::Thread(vector<int>, vector<string>, bool, vector<int>, vector<int>)` — `Thread.h:41-46` + `Thread.cpp:25-38`
- [x] `Processador::Processador(int,int,bool,TipoProcessador,ModeloMultithreading,vector<string>,vector<int>,vector<int>,vector<int>)` — `Processador.h:31-41` + `Processador.cpp:9-31`
- [x] `Processador::iniciarThreads(vector<string>,bool,int,vector<int>,vector<int>,vector<int>)` — `Processador.h:70-77` + `Processador.cpp:90-104`

### 2.2 Renomear funções e variáveis (resolvido)

| Nome antigo | Nome novo | Arquivos afetados |
|------------|----------|-------------------|
| `executarIssue()` | `Issue()` | `Thread.h:67-69`, `Thread.cpp:131-169`, `Processador.cpp:60-62`, `tb_Thread.cpp`, `tb_Processador.cpp` |
| `executarExMem()` | `ExMem()` | `Thread.h:71-73`, `Thread.cpp:171-181`, `Processador.cpp:50`, `tb_Thread.cpp` |
| `executarWr()` | `Wr()` | `Thread.h:75-77`, `Thread.cpp:183-195`, `Processador.cpp:51`, `tb_Thread.cpp` |
| `executarCommit()` | `Commit()` | `Thread.h:79-81`, `Thread.cpp:433-476`, `Processador.cpp:52` (era `executarCommitPublico` → unificado em `Commit()`) |
| `executarCommitPublico()` | *(removido)* | Lógica movida para dentro de `Commit()` em `Thread.cpp` |
| `executaExMemTodos()` | `iniciarFaseExOuMem()` | `Thread.h:86-88`, `Thread.cpp:197-253` |
| `executaWrTodos()` | `detectarTransicoesDeFase()` | `Thread.h:90-92`, `Thread.cpp:280-341` |
| `consumirBufferWB()` | `realizarWriteResult()` | `Thread.h:94-96`, `Thread.cpp:343-407` |
| `broadcast` (lambda) | `broadcastCDB()` | `Thread.h:98-100`, `Thread.cpp:350-391` |
| `coletar` (em `iniciarFaseExOuMem`) | `coletarCandidatasParaAvancar()` | `Thread.h:102-104`, `Thread.cpp:207-222` |
| `coletar` (em `detectarTransicoesDeFase`) | `coletarEventosDeTransicao()` | `Thread.h:106-108`, `Thread.cpp:286-304` |
| `liberarRScommit()` | `liberarRSPorRegistrador()` | `Thread.h:110-112`, `Thread.cpp:409-430` |
| `liberarPorPC` (lambda) | `liberarRSPorPC()` | `Thread.h:114-116`, `Thread.cpp:243-267` |
| `aux` (em `inicializarComponentes`) | `rs_config` / `uf_config` | `Thread.cpp:58-76` |
| `Evento` (local em `executaWrTodos`) | Struct movida para `Thread.h:20-23` | Disponível globalmente na Thread |

- [x] Adicionar comentário breve sobre `Vj/Vk/Qj/Qk` em `_codigo/headers/ReservationStations.h:61-64`

### 2.3 Interface de construtores (resolvido)
- [x] `num_ufs` **volta a 6 elementos**: `Processador.cpp` não faz mais `push_back(largura_de_despacho)`. Cada `Thread` recebe `largura_despacho` como parâmetro separado (default=1) e usa diretamente para `uf.commit`. Removeu o `>= 7` em `inicializarComponentes`. (`Thread.h`, `Thread.cpp`, `Processador.cpp`, `tb_Thread.cpp`)
- [x] `capacidade_rob` **vira parâmetro do construtor**: 6º parâmetro com default `CAPACIDADE_ROB=30`. Inicializado na lista de inicialização como `tem_rob ? capacidade_rob : 1`. Removeu o hardcoded de `inicializarComponentes`. (`Thread.h`, `Thread.cpp`)
- [x] `CAPACIDADE_ROB` mantido como `constexpr` em `Thread.h:10` (default do parâmetro).
- [x] `tb_Thread.cpp`: `NUM_UFS_PADRAO` agora tem 6 elementos; comentário sobre 7º elemento removido.

---

## Fase 3 — Refatoração Interna (estrutura e organização)

### 3.1 Eliminar lambdas duplicadas → métodos privados

| Lambda | Ocorrências | Extrair para método privado | Status |
|--------|------------|---------------------------|--------|
| `coletar` (em `iniciarFaseExOuMem`) | `Thread.cpp:226-240` | `Thread::coletarCandidatasParaAvancar(grupo)` | [x] |
| `coletar` (em `detectarTransicoesDeFase`) | `Thread.cpp:291-304` | `Thread::coletarEventosDeTransicao(grupo, ciclo)` | [x] |
| `broadcast` + `bcst` aninhada | `Thread.cpp:379-402` | `Thread::broadcastCDB(pc, dest, ciclo)` | [x] |
| `liberarPorPC` (libera RS por PC) | `Thread.cpp:361-365` + `Thread.cpp:413-419` | `Thread::liberarRSporPC(grupo, pc, ciclo)` | [x] |
| `buscar` (aloca UF livre) | `ReservationStations.cpp:223-234` (em `procuraUFlivre`) | `RS::alocarUFLivre(grupo, ciclo)` | [ ] |
| `liberar_em` (desaloca UF) | `ReservationStations.cpp:260-265` (em `liberarUF`) | `RS::desalocarUFdoGrupo(grupo, ciclo)` | [ ] |

### 3.2 Consolidar `std::sort` repetido
- [x] Sort de `vector<RS*>` (em `iniciarFaseExOuMem`) → extraído para `ordenarCandidatasPorPC()` ✓
- [ ] Sort de `vector<Evento>` (em `detectarTransicoesDeFase`) — ainda inline
- [ ] Sort de `vector<int>` (em `realizarWriteResult`) — ainda inline

### 3.3 Unificar switches de mapeamento tipo→grupo RS/UF
- [ ] `_codigo/ReservationStations.cpp:241-248` (switch em `procuraUFlivre`)
- [ ] `_codigo/ReservationStations.cpp:272-279` (switch em `liberarUF`)
- [ ] `_codigo/Thread.cpp:144-153` (switch em `executarIssue`)
- [ ] `_codigo/Thread.cpp:420-432` (switch em `consumirBufferWB`)
- [ ] Considerar tabela de mapeamento (`array`/`map`: `TipoInstrucao` → `(GrupoRS, GrupoUF_EX, GrupoUF_MEM)`) em vez de switches

### 3.4 Corrigir `const_cast` (resolvido)
- [x] `_codigo/ReservationStations.cpp:147` — `const_cast<std::string&>(id)` removido automaticamente ao mudar assinatura de `alocarRS` para `const std::string&` (item 2.1).

### 3.5 Expandir validação de bounds contra out-of-bounds
- [ ] `_codigo/ReservationStations.cpp:100-107` — acessa `cdb.F[regJ.getId()]` / `cdb.R[regJ.getId()]` sem validar `getId() < NUM_REGISTRADORES`
- [ ] `_codigo/ReservationStations.cpp:109-116` — acessa `cdb.F[regK.getId()]` / `cdb.R[regK.getId()]` sem validar `getId() < NUM_REGISTRADORES`
- [ ] `_codigo/ReservationStations.cpp:147-148` — acessa `cdb.F[dest.getId()]` / `cdb.R[dest.getId()]` sem validar `getId() < NUM_REGISTRADORES`
- [ ] `_codigo/Thread.cpp:388-392` — acessa `cdb.F[dest.getId()]` / `cdb.R[dest.getId()]` em `consumirBufferWB` sem validar `getId() < NUM_REGISTRADORES`

---

## Fase 4 — Arquitetura (refatoração estrutural)

### 4.1 Clarear fluxo Issue→EX→MEM→WB
- [ ] `Wr()` (`Thread.cpp:183-195`) faz 3 coisas: (1) flush `buffer_WB_pendente→buffer_WB`, (2) `realizarWriteResult()`, (3) `detectarTransicoesDeFase()`. Separar em etapas explícitas no pipeline.
- [ ] `realizarWriteResult()` (`Thread.cpp:343-407`) — broadcast extraído para `broadcastCDB()` ✓ (item 3.1); liberação de RS extraída para `liberarRSPorRegistrador()` + `liberarRSPorPC()` ✓ (item 3.1)
- [ ] Considerar `enum FaseInstrucao` + estado na RS em vez de múltiplos buffers (`buffer_WB`, `buffer_WB_pendente`)

### 4.2 Melhorar encapsulamento
- [ ] `_codigo/Thread.cpp` — `tabela_de_instrucoes[pc].ciclo_*` acessado diretamente em:
  - `:157` (ciclo_issue), `:176-198` (múltiplos em Commit)
  - `:257-261` (ciclo_EX, ciclo_MEM), `:324-338` (ciclo_EX, ciclo_MEM)
  - `:352-432` (múltiplos em realizarWriteResult)
  - Criar métodos setters em `Thread` (ex: `registrarIssue(pc, ciclo)`, `registrarInicioEX(pc, ciclo)`)
- [ ] `_codigo/Thread.cpp` — `buffer_WB` e `buffer_WB_pendente` manipulados diretamente:
  - `:116` (push_back), `:120` (range-for), `:327,338` (push_back)
  - `:348` (sort), `:352` (front), `:367,407` (erase)
  - Encapsular com métodos `adicionarWB(pc)`, `proximoWB()`, `removerWB()`

### 4.3 Evitar cópias profundas desnecessárias
- [ ] `_codigo/headers/Thread.h:51` + `_codigo/Thread.cpp:43` — `getCDB()` retorna `CDB` por valor → mudar para `const CDB&`
- [ ] `_codigo/headers/Thread.h:52` + `_codigo/Thread.cpp:44` — `getRS()` retorna `ReservationStations` por valor → mudar para `const ReservationStations&`
- [ ] `_codigo/headers/Thread.h:53` + `_codigo/Thread.cpp:45` — `getUF()` retorna `UnidadesFuncionais` por valor → mudar para `const UnidadesFuncionais&`
- [ ] `_codigo/headers/Thread.h:54` + `_codigo/Thread.cpp:46` — `getTabela()` retorna `vector<LinhaTabela>` por valor → mudar para `const vector<LinhaTabela>&`
- [ ] `_codigo/headers/Processador.h:48-50` + `_codigo/Processador.cpp:35` — `getThread()` retorna `Thread` por valor → mudar para `const Thread&`
- [ ] `_codigo/headers/Processador.h:52-54` + `_codigo/Processador.cpp:36` — `getTabelaThread()` retorna `vector<LinhaTabela>` por valor → mudar para `const vector<LinhaTabela>&`
- [ ] `_codigo/Main.cpp` — Pontos que precisam de cópia para impressão. Criar métodos `getCDBCopia()`, `getRSCopia()`, etc.:
  - `Main.cpp:375` — `p.getThread(0).getRS()` → precisa de cópia
  - `Main.cpp:392` — `p.getThread(0).getCDB()` → precisa de cópia
  - `Main.cpp:464` — `p.getThread(0).getUF()` → precisa de cópia
  - `Main.cpp:362-365` — `p.getTabelaThread(0)` → precisa de cópia

---

## Legenda

| Prioridade | Significado |
|------------|-------------|
| Fase 1 | Mudanças seguras, sem risco de quebrar testes |
| Fase 2 | Muda nomes/assinaturas — requer `make simtest-update` após |
| Fase 3 | Altera lógica interna — requer validação com `make test` + `make simtest` |
| Fase 4 | Mudanças arquiteturais — requer validação completa e possível revisão de `.expected` |
