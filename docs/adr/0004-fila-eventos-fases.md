# ADR 0004 — Fila de eventos determinística com fases e delta cycles

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0003, ADR 0011

## Contexto

Um loop de passo fixo desperdiça CPU e não garante ordem estável entre efeitos
combinacionais. Um loop puramente orientado a eventos precisa lidar com
múltiplos efeitos no mesmo instante.

## Decisão

- A fila ordena por `(time_ns, phase, seq)`, com `seq` monotônico global.
- Fases: `PreInput → Input → Evaluate → Settle → Commit → PostCommit`.
- Efeitos sem avanço de tempo são resolvidos em delta cycles, com limite de 64.
- Oscilação persistente gera `sim.oscillation` com os nets envolvidos.
- O tempo só avança para o próximo instante com trabalho pendente.
- O passo de 60 Hz é apenas publicação de frames, não o passo da simulação.

## Consequências

- Ordem determinística e reproduzível.
- Realimentação combinacional vira erro diagnosticável.
- Requer disciplina para não criar eventos infinitos.
- O scheduler precisa de estruturas eficientes (heap + wheel).

## Alternativas

- Passo fixo global: simples, porém lento e impreciso.
- Ordenação apenas por tempo: empates não determinísticos.
