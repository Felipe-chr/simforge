# ADR 0011 — Determinismo estrito da simulação

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0003, ADR 0004

## Contexto

Replay, testes de regressão e depuração dependem de execuções idênticas. Ordem
de containers, relógio de parede e ponto flutuante não controlado quebram isso.

## Decisão

- Proibido wall clock, `rand()` e iteração não ordenada no caminho de simulação.
- RNG por dispositivo, semeado por `(seed, device_id, purpose)`.
- Sem `fast-math`, FMA implícito ou `long double`.
- Ordenação por IDs estáveis, nunca por endereço.
- Trace grava versão do engine, hash do projeto, seed, hash do artefato e hash
  incremental de eventos.
- O CI executa o mesmo cenário duas vezes e compara o hash.

## Consequências

- Replay e testes confiáveis.
- Restrições de implementação e revisão de código.
- Determinismo entre arquiteturas é limitado; replay é a garantia portátil.
- Performance pode ser levemente afetada.

## Alternativas

- Melhor esforço: bugs intermitentes e replay inviável.
- Tudo em ponto fixo: determinismo forte, mas custo e complexidade altos.
