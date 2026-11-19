# ADR 0014 — WebSocket e C ABI sobre o mesmo command bus

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0001, ADR 0009

## Contexto

Desktop, navegador, CLI e outros produtos precisam dos mesmos comandos. Criar
APIs separadas por transporte duplica validação e gera divergência.

## Decisão

- Existe um único `CommandBus` com métodos versionados.
- WebSocket, C ABI, WASM e CLI são transportes finos sobre o mesmo bus.
- O envelope e os schemas são idênticos; muda apenas o framing.
- Eventos usam push no WebSocket e callback/poll nos demais.
- O cliente TypeScript é gerado do mesmo schema.

## Consequências

- Paridade funcional entre hosts.
- Testes de contrato cobrem todos os transportes.
- WebSocket adiciona latência e um servidor local.
- A API C precisa de buffers e ownership bem definidos.

## Alternativas

- API REST separada: mais superfície e divergência.
- Apenas C ABI: inviável no navegador.
