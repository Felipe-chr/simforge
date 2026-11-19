# Documentação do SimForge

Arquitetura de referência e contratos do simulador. Os documentos são a fonte
de verdade para ADRs, formatos e APIs.

## Ordem de leitura

1. [`01-arquitetura-geral.md`](01-arquitetura-geral.md) — visão, módulos,
   estrutura, fluxos e escalabilidade.
2. [`02-motor-eletrica-dispositivos.md`](02-motor-eletrica-dispositivos.md) —
   motor de eventos, modelo elétrico, frameworks, ESP32-S3 e compilação.
3. [`03-plataforma.md`](03-plataforma.md) — plugins, formatos, persistência,
   integração, performance, segurança, IA, roadmap, riscos e MVP.
4. [`04-spikes-fase-0.md`](04-spikes-fase-0.md) — spikes técnicos, critérios de
   saída e go/no-go antes do MVP.

## Formatos

| Documento | Schema | Estado |
|---|---|---|
| [`formatos/sforge-v1.md`](formatos/sforge-v1.md) | [`sforge-v1.schema.json`](formatos/sforge-v1.schema.json) | definido |
| [`formatos/sfrec-v1.md`](formatos/sfrec-v1.md) | binário (especificado) | definido |
| [`formatos/sfpkg-v1.md`](formatos/sfpkg-v1.md) | [`sfpkg-v1.schema.json`](formatos/sfpkg-v1.schema.json) | definido |

## Protocolos

| Documento | Schema | Estado |
|---|---|---|
| [`protocolos/api-v1.md`](protocolos/api-v1.md) | [`api-v1.schema.json`](protocolos/api-v1.schema.json) | definido |
| [`protocolos/world-v1.md`](protocolos/world-v1.md) | [`world-v1.schema.json`](protocolos/world-v1.schema.json) | definido |
| [`protocolos/plugin-abi-v1.md`](protocolos/plugin-abi-v1.md) | [`plugin-v1.schema.json`](protocolos/plugin-v1.schema.json) | definido |

## Decisões de arquitetura

| Índice | Conteúdo |
|---|---|
| [`adr/README.md`](adr/README.md) | 18 ADRs propostos, ordenados por bloco de aceite |

## Validação

Da raiz do repositório:

```bash
pip install jsonschema
python tools/validate_contracts.py
```

Verifica: schemas como JSON Schema 2020-12, exemplos contra o schema
correspondente, parse de todos os blocos JSON e links relativos.

## Convenções

- Tempo em nanossegundos inteiros (`int64`).
- Elétrica em unidades SI internamente; arquivos usam unidades explícitas
  (`V`, `A`, `Ω`, `mm`).
- Coordenadas de esquemático em unidades de grade inteiras (`grid_mm = 1.27`).
- IDs estáveis (ULID); nunca índices de array como referência.
- `schema` inteiro em todo documento; leitura tolerante a campos desconhecidos.
- Nenhum código ou símbolo do PistaForge no repositório.

## Estado

- Arquitetura e contratos: definidos.
- Exemplos e schemas: validados por `tools/validate_contracts.py`.
- Implementação: não iniciada.
- Próximo passo: aceitar os ADRs 0001–0010 e executar os spikes da fase 0.
