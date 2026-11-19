# ADR 0015 — Limites de recursos e watchdog para firmware

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0006

## Contexto

Firmware pode entrar em loop infinito, alocar sem limite ou travar a simulação.
Sem limites, uma sessão derruba o app ou a nuvem.

## Decisão

- Runtime recebe `instruction_budget`, `event_budget` e memória máxima.
- Loop sem yield gera `firmware_hang` e interrupção.
- WASM usa fuel/epoch; nativo usa watchdog e job object.
- Limites são configuráveis por projeto e por host.
- Violação gera evento e estado, nunca crash do processo principal.

## Consequências

- Sessões permanecem responsivas.
- Falsos positivos exigem ajuste de orçamento.
- Firmware muito pesado pode precisar de modo não determinístico.
- Erros ficam diagnosticáveis com stack/PC quando disponível.

## Alternativas

- Sem limites: simples, inaceitável em multiusuário.
- Timeout apenas de parede: não funciona em fast-forward.
