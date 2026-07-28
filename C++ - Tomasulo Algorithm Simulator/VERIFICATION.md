# Geral
- Muitas funções possuem uma superlotação de métodos privados que dificulta acompanhar a linha lógica. Verificar funções auxiliares que podem ser transformadas em `static` para ir apenas para a implementação no **.cpp** e tornar o ambos (**.h** e **.cpp**) mais limpos.

# De cada módulo:
## Components.cpp/.h
- [ ] AllocateRS e DeallocateRS tem retorno void ao invés de bool (não dá para saber se a alocação ou desalocação foi bem sucedida).
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
- [ ] O construtor vazio dessa classe é necessário ou se isso é uma margem para falha de implementação.
- [ ] Verificar se `GetDestRegister()` `GetJ()` e `GetK()` não poderiam ser `const &`, já que `Register` é uma classe consideravelmente grande.
- [ ] Pensar se passar a `string` diretamente para a `instruction_string` é uma boa ideia, ou se não valia apena implementar uma função de normalização (padroniza tudo em maiúsculo, espaçamento uniforme, etc.). Esse tipo já faria a verificação de validade da instrução (seria bool) e a partir dele as demais funções trabalhariam (como ela está normalizada é mais fácil)
- [ ] Tem este comentário no **.h**: *// - instruction_string DEVE vir antes de PC para que o initializer list a inicialize antes de SplitInstruction() ser chamado*. Eu queria saber se esse comentário faz sentido real ou se não tem diferença.

## ReservationStations.cpp/.h
- [ ] O construtor poderia ser definido integralmente apenas no **.h** (já que não tem métodos, apenas uma definição de atributos).
- [ ] A variável `dest_valido` dentro de `AddIssue()` está em português (fora da normalização feita). Deve virar `valid_dest`.
- [ ] Verificar se os registradores do `CDB` vão de 1-32 ou de 0-31 para ver qual o range correto de validação da variável `valid_dest`.
- [ ] `ReadSourceOperand()` é um método ineficiente, pois passa o registrador e atributos do registrador (poderia resumir o parâmetro). Verifique se é isso mesmo.
- [ ] A variável `valid_dest` além de não fazer a verificação completa de validade (deveria ser `dest.GetType() != 'Z' && dest.GetId() >= 0 && dest.getId() < num_registers`, que é feita de maneira redundante no método `ReadSourceOperand()`). Poderia ser feito em `AddIssue()`: `if(!valid_dest) return false; ...` (resume muito a lógica de verificação e retira esse elemento que polui os outros métodos).
- [ ] O `if-else` do método `ReadSourceOperand()` está muito mal explicado (não dá para saber sem uma análise pesada o que cada caso faz e por que).
- [ ] O método `ResolveBothDependencies()` não deixa claro sua real função sem um contexto. Poderia mudar para `ResolveCDBDependencies()` ou outro nome a sua sugestão. 

**PAREI DE ANALISAR EM `ResolveSingleDependency()`**
