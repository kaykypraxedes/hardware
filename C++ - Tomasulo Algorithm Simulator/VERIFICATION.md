# Geral
- Muitas funções possuem uma superlotação de métodos privados que dificulta acompanhar a linha lógica. Verificar funções auxiliares que podem ser transformadas em `static` para ir apenas para a implementação no **.cpp** e tornar o ambos (**.h** e **.cpp**) mais limpos.

# De cada módulo:
## Components.cpp/.h
- [x] AllocateRS e DeallocateRS tem retorno void ao invés de bool (não dá para saber se a alocação ou desalocação foi bem sucedida).
- [x] `ParseType()` e `ParseId()` em vez de `IdentifyType()`/`IdentifyId()` — agora retornam `bool`
- [x] `HasPendingProducer()` movido para `private`
- [x] `GetAllocationTimes()` e `GetCurrentRS()` — **analisado: não podem ser `const &`** (constroem/derivam resultados, não retornam membro direto)
- [x] Construtor vazio `Register()` eliminado — unificado em `Register(std::string = "")`
- [x] `int→size_t` em forward loops
- [x] `ParseType` sem try-catch (código morto removido)
- [x] `ParseId` valida range `[0, num_registers)`
- [x] Construtor `Register(string)` aborta (`std::cerr` + `std::abort()`) em string inválida

## Instruction.cpp/.h
- [x] `SetAttributes()`: ELSE J, ELSE K e BRANCH J com verificação F/R (imediato não passa ao construtor)
- [x] O construtor vazio dessa classe é necessário ou se isso é uma margem para falha de implementação.
- [x] Verificar se `GetDestRegister()` `GetJ()` e `GetK()` não poderiam ser `const &`, já que `Register` é uma classe consideravelmente grande.
- [x] Pensar se passar a `string` diretamente para a `instruction_string` é uma boa ideia, ou se não valia apena implementar uma função de normalização (padroniza tudo em maiúsculo, espaçamento uniforme, etc.). Esse tipo já faria a verificação de validade da instrução (seria bool) e a partir dele as demais funções trabalhariam (como ela está normalizada é mais fácil)
- [x] Tem este comentário no **.h**: *// - instruction_string DEVE vir antes de PC para que o initializer list a inicialize antes de SplitInstruction() ser chamado*. Eu queria saber se esse comentário faz sentido real ou se não tem diferença.

## ReservationStations.cpp/.h
- [x] O construtor poderia ser definido integralmente apenas no **.h** (já que não tem métodos, apenas uma definição de atributos). Poderia, mas foi optado por não.
> Reformulação quase completa dos métodos `ReadSourceOperand()` e `UpdateDependencies()`, que tornaram as observações obsoletas.
- [-] A variável `dest_valido` dentro de `AddIssue()` está em português (fora da normalização feita). Deve virar `valid_dest`.
- [-] Verificar se os registradores do `CDB` vão de 1-32 ou de 0-31 para ver qual o range correto de validação da variável `valid_dest`.
- [-] `ReadSourceOperand()` é um método ineficiente, pois passa o registrador e atributos do registrador (poderia resumir o parâmetro). Verifique se é isso mesmo.
- [-] A variável `valid_dest` além de não fazer a verificação completa de validade (deveria ser `dest.GetType() != 'Z' && dest.GetId() >= 0 && dest.getId() < num_registers`, que é feita de maneira redundante no método `ReadSourceOperand()`). Poderia ser feito em `AddIssue()`: `if(!valid_dest) return false; ...` (resume muito a lógica de verificação e retira esse elemento que polui os outros métodos).
- [-] O `if-else` do método `ReadSourceOperand()` está muito mal explicado (não dá para saber sem uma análise pesada o que cada caso faz e por que).
- [-] O método `ResolveBothDependencies()` não deixa claro sua real função sem um contexto. Poderia mudar para `ResolveCDBDependencies()` ou outro nome a sua sugestão. 
- [x] O método `TryAllocateNormal()` poderia virar `TryAllocateFU()`. 
- [x] O `TryAllocateLoadStore()` poderia virar `TryAllocateMEM()` e ele apenas tenta fazer a alocação do estado MEM (o geral ainda ficaria no `TryAllocateOnCDB()`, que executaria se o `TryAllocateMEM()` retornasse falso).
- [x] Criar uma função auxiliar para evitar cópia de código no `TryAllocate*()` (mesmo código se repete basicamente 3x).
- [x] Transformar `DeallocateFUFromGroup()` em um método `static` com retorno bool (verificar se a desalocação aconteceu da maneira esperada) da mesma forma como foi feito com `AllocateFreeFu()` (métodos básicamente iguais).
- [x] Estude se os métodos `ReleaseFU()` e `FindFreeFU()` não podem ser resumidos de alguma maneira (visto que é basicamente o mesmo código, mas jogando em uma função diferente). O mesmo para `AllocateFreeFu()` e `DeallocateFUFromGroup()`.

## Thread.cpp/.h

- [x] Nome pouco explicativo dos `Enums` e dos seus elementos internos (buscar uma definição mais clara).
- [x] Estado `THREAD_STATE::BLOCKED` nunca é setado (pelo menos não encontrei). Acho que era um recurso que eu estava fazendo para lidar com Branchs sem previsor de desvio ou com multithread (último ainda não implementado).
- [x] Acredito que o vetor `switch_instructions` era para indicar o quais instruções teriam uma latência de EX e/ou MEM diferentes. Além de não ser utilizado para nada, o nome está pouco sugestivo da sua utilidade e não pode ser um vetor simples, mas sim um `touple<int, int, int>` (o primeiro sendo a posição do PC e o segundo e terceiro quais os valores de EX e MEM a serem considerados). É preciso também implementar uma função ou um trecho de código para que as instruções específicas recebam essa nova latência no segundo construtor.
- [x] O `bool has_rob` dentro do construtor é desnecessário (pode ser inferido). Se no final ele passar um `int rob_capacity` e um `bool has_predictor` necessariamente ele tem ROB, se não, ele não tem (os 2 pois só pode ter previsor se tem ROB). Acho que vai ter que substituir o construtor atual por um assim:

```C++
Thread(
    const std::vector<std::string>&             assembly,
    const std::vector<std::tuple<int,int,int>>& new_latency = {},
    const std::vector<int>&                     num_rs = {},
    const std::vector<int>&                     num_fus = {},
    const std::vector<int>&                     switch_cycles = {},
    int                                         dispatch_width = 1,
    int                                         rob_capacity = 0,
    bool                                        has_predictor = false
);
```

Já resume em apenas um construtor (faz uma verificação se o `mod_latency_instructions` está vazio para fazer as modificações nas instruções).

- [x] Esta passagem no Construtor: `instruction_table.push_back({Instruction(i++, instr), 0, 0, {}, {}, 0, 0});` está certa? Me parece que ele está passando o `PC = 0` em todos (o PC é sequencial e imutavel nesse processador). Isso poderia resolver problemas de consistência que fazem `Instruction` ter que ter um atributo `PC` (desvio de função e falha de encapsulamento).
- [x] Verificação atual para englobar RSs e FUs: `if(num_rs.size() >= NUM_RS_GROUPS) aux = num_rs; else aux = {5,5,5,4,3,2};`. Isso permite um número maior de valores que é simplesmente ignorado. É para passar o valor correto e, se não for passado, ser lançado um erro com abort semelhante ao já implementado em módulos passados:

```C++
// Verifica se ele está vazio:
if(num_rs.empty()) aux = {5,5,5,4,3,2}; // Valor arbitrário default.
// Verifica se o valor passado é válido:
else if(num_rs.size() != NUM_RS_GROUPS){
    std::cerr << "[ERRO] Quantidade inválida de valores para RSs: " << num_rs.size() << "\n";
    std::abort();
}
```

O mesmo para FUs. O fu.commit só é iniciado se ele tiver ROB.
- [x] `SetCustomLatency()` fica obsoleto já que dentro do construtor é definido as latências específicas.

Resumir funções desnecessárias (wrappers pequenos ou funções de uma linha), que dificultam o entendimento e poluem o código. A solução será a aplicação direta dessas operações na função que as chamava:
  - [x] `RegisterIssue()` — vira `instruction_table[pos].issue_cycle = cycle;` direto em `Issue()`
  - [x] `SetWR()` — vira `instruction_table[pos].wr_cycle = cycle;` direto em `WriteBackNormal()`
  - [x] `NextWB()` — vira `return wb_buffer.front();` direto em `PerformWriteResult()`
  - [x] `RemoveWB()` — vira `wb_buffer.erase(wb_buffer.begin());` direto em `PerformWriteResult()` (2 pontos, mesma função)
  - [x] `AddPendingWB()` — vira `pending_wb_buffer.push_back(position);` direto em `DetectPhaseTransitions()` (2 pontos, mesma função)
  - [x] `AddExCycle()` — vira `instruction_table[pos].ex_cycles.push_back(cycle);` direto em `StartExOrMemPhase()` e `DetectPhaseTransitions()`
  - [x] `AddMemCycle()` — vira `instruction_table[pos].mem_cycles.push_back(cycle);` idem
  - [x] `FlushPendingWBBuffer()` — vira `insert` + `clear` (2 linhas) no início de `Wr()`
  - [x] `SortWBBuffer()` — vira `std::sort(wb_buffer.begin(), wb_buffer.end())` direto em `PerformWriteResult()` (wrapper do `insertionSort`)

- [x] Remover a função de código morto `AddWB()` (declarada e implementada, nenhuma chamada em todo o projeto) de `Thread.h` e `Thread.cpp`.
- [x] Fusão de funções parecidas em uma única lógica:
- `CollectCandidatesToAdvance()`, `CollectCandidatesFromGroup()`, `TryAdvanceRS()` → fundidas em `StartExOrMemPhase()`.
- `CollectTransitionEvents()`, `CollectEventsFromGroup()`, `ProcessTransition()` → fundidas em `DetectPhaseTransitions()`.
- `FindWBInGroup()` → fundida em `BroadcastCDB()`.
- `WriteBackStoreWithROB()` → vira um branch dentro de `PerformWriteResult()` (com a correção do bug de release de STORE com ROB, na seção **BUG** abaixo).
**Funções livres / arquivo inteiro**
- [x] `PositionOfRS`, `SortCandidatesByPosition` — saem; o comparador vira lambda inline em `std::sort` no local de uso.
- [x] `PositionOfEvent`, `SortEventsByPosition` — saem por completo, incluindo a própria ordenação (motivo lógico, ver a seção **Eliminar a ordenação de `events`** abaixo).
- [x] `ReleaseRSByRegister()` — sai, substituída por `ReleaseRSByPosition()` + `GetRSGroupForType()` (motivo lógico, ver a seção **Troca de critério** abaixo).
- [x] **`SortUtils.h`** — arquivo inteiro removido do projeto. As duas ordenações que sobrevivem (`candidates`, `wb_buffer`) usam `std::sort` de `<algorithm>` diretamente.

**Funções a serem refatoradas/divididas por quantidade de código**

 - [x] Ponto central: o padrão **"chamar a mesma função nos 6 grupos de RS"** se repetia em pelo menos 4 lugares, e o mapeamento **`INSTRUCTION_TYPE → grupo de RS`** era refeito à mão em 2 lugares. Dois helpers novos resolvem isso:

```C++
// Aplica uma operação sobre os 6 grupos de RS.
template <typename Func>
static void ForEachRSGroup(RESERVATION_STATIONS& rs, Func&& func) {
    func(rs.load);
    func(rs.store);
    func(rs.int_basic);
    func(rs.int_mult_div);
    func(rs.float_basic);
    func(rs.float_mult_div);
}

// Único ponto de verdade do mapeamento tipo → grupo (mesmo espírito do
// GetFUGroup() já existente em ReservationStations.cpp).
static std::vector<ReservationStation>& GetRSGroupForType(
    RESERVATION_STATIONS& rs,
    INSTRUCTION_TYPE      type
) {
    switch (type) {
        case INSTRUCTION_TYPE::LOAD:         return rs.load;
        case INSTRUCTION_TYPE::STORE:        return rs.store;
        case INSTRUCTION_TYPE::FLOAT_BASIC:  return rs.float_basic;
        case INSTRUCTION_TYPE::INT_MUL:
        case INSTRUCTION_TYPE::INT_DIV:      return rs.int_mult_div;
        case INSTRUCTION_TYPE::FLOAT_MUL:
        case INSTRUCTION_TYPE::FLOAT_DIV:    return rs.float_mult_div;
        default:                             return rs.int_basic; // cobre BRANCH e INT_BASIC
    }
}
```

Funções que mudam por causa disso:

- [x] **`Issue()`** — o `switch` local que escolhe o grupo de RS sai; vira `GetRSGroupForType(rs, type)`.
- [x] **`WriteBackNormal()`** — o `if/else if` de 6 ramos escolhendo grupo + tipo de release sai; vira `GetRSGroupForType(rs, type)` + `ReleaseRSByPosition(group, position, cycle)`.
- [x] **`StartExOrMemPhase()`** — absorve a coleta de candidatas (`ForEachRSGroup` + filtro de `has_rob`/`unresolved_branch_position` inline), a ordenação (`std::sort`) e o avanço de fase (lógica de `TryAdvanceRS` inline no loop final). Fica com um único corpo: coletar → ordenar → avançar.
- [x] **`BroadcastCDB()`** — absorve a busca da RS produtora (`ForEachRSGroup`) e a resolução de dependências nos outros grupos (`ForEachRSGroup` aninhado). Validação de `dest` inválido sobe pro topo da função (fail-fast, evita rodar o loop dos 6 grupos à toa quando não há destino).
- [x] **`DetectPhaseTransitions()`** — absorve a coleta de eventos (`ForEachRSGroup`) e o processamento (antigo `ProcessTransition`, agora inline no loop). **Sem** a ordenação por posição — ver a seção **Eliminar a ordenação de `events`** abaixo.
- [x] **`InitializeComponents()`** — o `if(vazio) default; else if (tamanho errado) abort; else usa`; vira `aux = vazio ? default : num_rs; if (aux.size() != N) abort;` — mesma regra, uma ramificação a menos. Mesma coisa para FUs.
- [-] **`ExMem()`** — os dois `if` que calculam "todas as instruções terminaram" viram um ternário único.

**Funções a serem refatoradas por questão de lógica**

**BUG — release de `STORE` com ROB nunca acontece**
`STORE` com ROB é liberado da RS logo na transição `EX → MEM` (a latência real de memória é simulada depois, no `Commit()`, via `store_commit_state`). Por isso, essa RS **nunca chega em `phase == WB`** — ela é excluída de propósito de `StartExOrMemPhase` assim que entra em `MEM`, pra não competir pela FU de memória.

Se o release desse caminho usar a mesma `ReleaseRSByPosition()` que exige `phase == WB` (usada no caminho normal), a condição nunca bate e a RS fica presa em `busy = true` para sempre. Com o pool default de RS de `store` (5), a partir do 6º `STORE` do programa `Issue()` trava pra sempre nessa instrução — bug determinístico, não um edge case raro.

**Ação:** manter dois helpers de release, não um só:
```C++
// Caminho normal: a RS realmente chegou em WB antes de ser liberada.
static void ReleaseRSByPosition(
    std::vector<ReservationStation>& group, int position, int cycle
) {
    for (ReservationStation& r : group) {
        if (r.GetBusy() &&
            r.GetInstructionPhase() == INSTRUCTION_PHASE::WB &&
            r.GetCurrentInstruction().GetPosition() == position) {
            r.Release(cycle);
            break;
        }
    }
}

// Caminho STORE+ROB: a RS é liberada ainda em MEM, por design — não checar fase.
static void ReleaseStoreRSWithROB(
    std::vector<ReservationStation>& group, int position, int cycle
) {
    for (ReservationStation& r : group) {
        if (r.GetBusy() && r.GetCurrentInstruction().GetPosition() == position) {
            r.Release(cycle);
            break;
        }
    }
}
```
Usar `ReleaseStoreRSWithROB()` só no branch `if (type == STORE && has_rob)` de `PerformWriteResult()`. Usar `ReleaseRSByPosition()` em todo o resto (via `WriteBackNormal()`).

**Troca de critério: registrador → posição (correção, não só estilo)**
Nos outros 5 tipos (tudo exceto `STORE`+ROB), trocar `ReleaseRSByRegister()` (match por registrador de destino, sem `break`, podendo liberar mais de uma RS por chamada) por `ReleaseRSByPosition()` (match pela posição exata da instrução) é uma **correção de comportamento**, não neutra:
- `BroadcastCDB()` já identifica a RS produtora pela posição, não pelo registrador.
- Num cenário de WAW com porta de WR limitada (duas RS diferentes, mesmo registrador de destino, ambas em `WB` ao mesmo tempo no mesmo grupo), o match por registrador podia liberar a RS **errada** — uma instrução mais nova, antes da própria vez dela passar por `WriteBackNormal()`.
**Ação:** adotar o match por posição nesses 5 casos (já é o que `GetRSGroupForType()` + `ReleaseRSByPosition()` fazem), e deixar um comentário no código explicando o motivo — é fácil essa distinção se perder de novo numa futura "limpeza".

**Eliminar a ordenação de `events`, não só a implementação**
Das 3 ordenações do arquivo, só 2 são funcionalmente necessárias:
- `candidates` (em `StartExOrMemPhase`) — **necessária**: quem é processado primeiro "ganha" a FU disputada.
- `wb_buffer` (em `PerformWriteResult`) — **necessária**: quem é processado primeiro "ganha" a porta de WR limitada (`fu.wr`).
- `events` (em `DetectPhaseTransitions`) — **dispensável**: cada evento só mexe na própria posição em `instruction_table`, não há disputa de recurso entre eles nesse ponto, e o resultado final (`pending_wb_buffer → wb_buffer`) é reordenado de novo por `wb_buffer`'s sort antes de qualquer uso.
**Ação:** remover o `std::sort`/`std::stable_sort` de `events` inteiramente — não só trocar a implementação. Processar os eventos na ordem em que `ForEachRSGroup` os encontrar.

**Assinatura final de `Thread.h` (privados)**
Depois de tudo isso, os métodos privados de `Thread` ficam reduzidos a:
```
InitializeComponents()
StartExOrMemPhase()
PerformWriteResult()
WriteBackNormal()
BroadcastCDB()
DetectPhaseTransitions()
```

## Processor.cpp/.h

## Main.cpp/.h
