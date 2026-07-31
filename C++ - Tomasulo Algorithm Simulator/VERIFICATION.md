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

** ANALISADO ATÉ `IsSwitchCycle()`**

## Processor.cpp/.h

## Main.cpp/.h
