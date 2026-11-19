# ADRs do SimForge

Decisões de arquitetura registradas. Status inicial: **proposto**. Um ADR passa
a **aceito** após revisão explícita; mudanças posteriores exigem novo ADR ou
emenda datada.

## Índice

| ADR | Decisão | Status |
|---|---|---|
| [0001](0001-core-cpp20-api-c.md) | Núcleo C++20 com API C estável | proposto |
| [0002](0002-independencia-pistaforge.md) | Independência total do PistaForge | proposto |
| [0003](0003-tempo-virtual-ns.md) | Tempo virtual em nanossegundos inteiros | proposto |
| [0004](0004-fila-eventos-fases.md) | Fila de eventos determinística com fases e delta cycles | proposto |
| [0005](0005-solvers-logic-mna.md) | LogicSolver no MVP e MnaSolver como evolução | proposto |
| [0006](0006-firmware-wasm-sandbox.md) | Firmware em WASM como sandbox padrão | proposto |
| [0007](0007-plugin-abi-c.md) | ABI C versionada para plugins | proposto |
| [0008](0008-sforge-json-schema.md) | `.sforge` em JSON com schema inteiro | proposto |
| [0009](0009-external-world.md) | `IExternalWorld` como fronteira de integração | proposto |
| [0010](0010-frontend-sem-logica.md) | Frontend sem lógica de simulação | proposto |
| [0011](0011-determinismo-estrito.md) | Determinismo estrito da simulação | proposto |
| [0012](0012-trust-tiers-plugins.md) | Trust tiers e assinatura de plugins | proposto |
| [0013](0013-compilacao-subprocesso.md) | Compilação em subprocesso isolado com cache | proposto |
| [0014](0014-api-uma-base.md) | WebSocket e C ABI sobre o mesmo command bus | proposto |
| [0015](0015-limites-watchdog.md) | Limites de recursos e watchdog para firmware | proposto |
| [0016](0016-unidades-grade-si.md) | Geometria em grade inteira e elétrica em SI | proposto |
| [0017](0017-campos-desconhecidos.md) | Preservação de campos desconhecidos | proposto |
| [0018](0018-qemu-processo-separado.md) | QEMU em processo separado | proposto |
| [0019](0019-zero-dependencias-fase-0.md) | Zero dependências externas na fase 0 | proposto |
| [0020](0020-ponte-dev-ndjson.md) | Ponte de desenvolvimento NDJSON com `simforge-cli api` | proposto |

## Ordem sugerida de aceite

1. **Bloco fundacional:** 0001, 0002, 0003, 0004, 0010, 0011, 0014, 0016.
2. **Bloco de plataforma:** 0005, 0006, 0007, 0008, 0009, 0012, 0017, 0019.
3. **Bloco de evolução:** 0013, 0015, 0018, 0020.

Os ADRs 0001–0010 devem estar aceitos antes do início da fase 1.

## Template

```markdown
# ADR NNNN — Título

- **Status:** proposto | aceito | substituído por ADR XXXX
- **Data:** AAAA-MM-DD
- **Relacionado:** ADR XXXX

## Contexto
## Decisão
## Consequências
## Alternativas
```
