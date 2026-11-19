# ADR 0003 — Tempo virtual em nanossegundos inteiros

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0004, ADR 0011

## Contexto

Simulações elétricas e de firmware exigem ordenação exata de eventos e
reprodutibilidade. Ponto flutuante acumula erro e dificulta comparação de
traces.

## Decisão

- `SimTime` é `int64` em nanossegundos.
- Conversões para `double` acontecem apenas em modelos físicos e apresentação.
- O relógio de parede nunca participa de decisões de simulação.
- `GuestClock` (firmware) é derivado do `VirtualClock`.

## Consequências

- Ordenação e comparação são exatas.
- Resolução de 1 ns é suficiente para GPIO, I2C, PWM e WS2812.
- Faixa de ~292 anos; overflow é tratado como erro de configuração.
- Modelos que precisam de sub-nanossegundo usam unidades próprias internas.

## Alternativas

- `double` em segundos: erro acumulado e ordenação instável.
- `std::chrono` no estado: tipo pesado e dependente de plataforma.
