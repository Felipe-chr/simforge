# ADR 0010 — Frontend sem lógica de simulação

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0001, ADR 0008

## Contexto

Reimplementar regras elétricas, tempo ou validação em TypeScript criaria
divergências com o core e impediria reuso em outros hosts.

## Decisão

- React/TS desenha, captura entrada e apresenta dados.
- Toda decisão de topologia, ERC, tempo e simulação é do core.
- Hit-testing visual e snapping são permitidos como UX; a confirmação é um
  comando (`circuit.connect`, `circuit.pick`).
- O cliente é gerado a partir dos schemas da API.
- Interpolação visual entre frames é explicitamente apresentação.

## Consequências

- Uma única fonte de verdade.
- Latência de ida e volta para interações; mitigada por comandos otimistas e
  snapshots.
- O frontend pode ser substituído sem reescrever regras.
- Exige contrato de API bem definido e testado.

## Alternativas

- Motor JS no navegador: duplicação e divergência.
- Frontend nativo: perde portabilidade e ecossistema React.
