# MIPS nRisc de 8 bits

Processador **MIPS nRisc monociclo** de 8 bits, projetado integralmente em Verilog HDL. Implementa um datapath simplificado (dados, instruções e memória em 8 bits) e vem com um programa embarcado que executa a **Cifra de César** sobre uma mensagem em memória.

**Desenvolvedores:** Kayky Moreira Praxedes, Carlos Ernesto Cardoso dos Reis

> Documentação completa (diagrama de blocos, tabelas de sinais de controle, traces de simulação ciclo a ciclo e o código Assembly comentado linha a linha) está no PDF do projeto — este README é só um panorama geral.

---

## Programa embarcado: Cifra de César

O algoritmo embarcado na memória de instruções:

1. Lê um caractere da memória de dados.
2. Soma o deslocamento (chave, entre -15 e 15).
3. Ajusta o valor caso ultrapasse o intervalo alfabético.
4. Armazena o novo caractere de volta na memória.

Limite de mensagem: **128 caracteres**, definido pelo espaço de endereçamento (7 bits).

---

## Caminho dos dados

1. PC acessa memória de instruções.
2. Instrução é decodificada pela unidade de controle.
3. Registradores são lidos.
4. ULA executa a operação.
5. Resultado é escrito no banco de registradores ou na memória.

---

## Arquitetura

- **Unidade de controle** — Gera os sinais de controle (`ALUSrc`, `MemToReg`, `RegWrite`, `MemRead`, `MemWrite`, `Branch`, `Jump`, `NextOp`, `End`, `ALUOp`) a partir do OpCode e do campo `funct`. Tabela completa de sinais por instrução no PDF.
- **PC (Program Counter)** — Armazena o endereço da próxima instrução.
- **Memória** — Dividida em memória de dados e memória de instruções, ambas com 128 posições de 8 bits (endereços de 7 bits).
- **Banco de registradores** — 4 registradores de uso geral (`$c0`–`$c3`), mais o registrador especial `$re`, que guarda o resultado de comparações (`beq`, `slt`), assumindo `-4` por padrão para evitar desvios não intencionais.
- **ULA** — Realiza operações aritméticas, lógicas e de comparação. Construída hierarquicamente a partir de somadores de 1, 8 e 16 bits (sem usar operadores de alto nível da linguagem), explicitando o fluxo de carry.
- **Extensor de sinal** — Converte imediatos de 3 bits para 8 bits.

---

## Instruções

Instruções de 8 bits, em três formatos:

| Formato | Campos | Uso |
|---|---|---|
| **2R** | OpCode(3) + Reg1(2) + Reg2(2) + funct(1) | `add`, `sub`, `mult`, `ld`, `st`, `beq`, `slt` |
| **1R** | OpCode(3) + Reg(2) + Imediato(3) | `addi`, `jr`, `result` |
| **0R** | OpCode(3) + don't care(4) + funct(1) | `nop`, `hlt` |

O imediato tem sinal e varia de -3 a 3. Bits *don't care* recebem `0` por padrão. Assembly e binário completo de cada instrução, com exemplos, estão no PDF (Seção 4).

---

## Testes

O processador foi validado por testbenches individuais (unidade de controle, PC, memória, banco de registradores, ULA) e end-to-end com o programa embarcado, cifrando múltiplas mensagens (incluindo casos com espaços e wraparound do alfabeto). Resultados completos no PDF, Apêndices B e C.
