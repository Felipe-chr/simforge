# ADR 0009 — `IExternalWorld` como fronteira de integração

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0002

## Contexto

O SimForge precisa interagir com semáforos, cancelas, radares e outros mundos,
incluindo o PistaForge, sem conhecer nenhum produto específico.

## Decisão

- A única fronteira é `IExternalWorld`, com entidades genéricas
  (`sensor`, `actuator`, `bidirectional`), `sync(t)`, `read`, `write` e eventos.
- O protocolo `world.*` é versionado, independente de transporte.
- O adaptador de cada produto é externo.
- Em modo determinístico, leituras e eventos são gravados no trace; replay não
  consulta o mundo.

## Consequências

- Qualquer produto pode integrar implementando o protocolo.
- Há custo de serialização e sincronização temporal.
- Timeouts precisam de valores seguros e política de degradação.
- O SimForge pode ser cliente ou servidor.

## Alternativas

- API específica do PistaForge: acoplamento proibido.
- Integração por arquivos: não atende a tempo real/eventos.
