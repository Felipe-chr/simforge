# ADR 0005 — LogicSolver no MVP e MnaSolver como evolução

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0004

## Contexto

A maioria dos projetos de prototipagem é digital (GPIO, I2C, SPI, PWM). Um
solver SPICE completo desde o início atrasaria o MVP e elevaria o custo de
convergência e performance.

## Decisão

- O MVP usa `LogicSolver`: níveis, forças, pull-ups/downs e contenção.
- Dispositivos expõem um modelo funcional (brilho, som, posição) nesse modo.
- `MnaSolver` é um solver plugável, com MNA, Newton-Raphson e integração no
  tempo, ativado por projeto ou por detecção de elemento analógico.
- A interface `IElectricalSolver` é a mesma; dispositivos declaram fidelidade.

## Consequências

- MVP rápido e determinístico.
- Dois modelos por dispositivo quando há fidelidade elétrica.
- Promoção automática pode surpreender; o projeto pode fixar `solver`.
- A arquitetura não precisa ser refeita para adicionar MNA.

## Alternativas

- SPICE desde o dia 1: prazo e complexidade inviáveis.
- Apenas lógico para sempre: não atende tensão/corrente.
