# ADR 0018 — QEMU em processo separado

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0006, ADR 0015

## Contexto

A evolução para firmware Xtensa real e ESP-IDF exige emulação de CPU e
periféricos. Integrar QEMU no processo principal traz risco de crash,
licenciamento GPL e conflitos de build.

## Decisão

- QEMU (fork Espressif fixado) roda em processo separado.
- A comunicação usa shared memory/pipe com protocolo versionado.
- O SimForge mantém o tempo autoritativo e sincroniza por fatias com `icount`.
- Periféricos relevantes conversam com o netlist por modelos-ponte.
- GDB stub é exposto para depuração.
- Uma ISS própria só é considerada se houver requisito que o QEMU não atenda.

## Consequências

- Isolamento de crash e licença.
- Overhead de IPC e complexidade de sincronização.
- Empacotamento e atualização do binário QEMU.
- Determinismo depende de `icount` e disciplina de I/O.

## Alternativas

- QEMU in-process: risco e GPL no núcleo.
- ISS próprio: controle total, prazo de 12–24 meses.
