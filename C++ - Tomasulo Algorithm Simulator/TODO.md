# TODO — Projeto Tomasulo (Checklist para Finalização)

Esses itens existem na interface (`enum`, construtores) mas não têm implementação real. Serão abordados em uma fase futura de expansão:

- [ ] **`PROCESSOR_TYPE::IN_ORDER`** — Pipeline clássico de 5 estágios (sem Tomasulo), dispatch width limitado a 1, stalls estruturais diferenciados
- [ ] **`MULTITHREADING_MODEL::COARSE_GRAINED`** — Troca de contexto baseada em `switch_instructions` (contagem de instruções). Atualmente comporta como FINE_GRAINED
- [ ] **Flush especulativo sem ROB** — Quando BRANCH sem ROB completa, limpar das RSs as instruções emitidas após o BRANCH (thread entra WAITING mas RSs ficam ocupadas)
