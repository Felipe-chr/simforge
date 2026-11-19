# ADR 0008 — `.sforge` em JSON com schema inteiro

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0017

## Contexto

Projetos precisam ser legíveis, versionáveis e interoperáveis. Um formato
binário reduz tamanho, mas dificulta diff, inspeção e integração.

## Decisão

- O projeto é JSON UTF-8 com `format` e `schema` inteiro.
- IDs são ULIDs estáveis; topologia (`nets`) é separada da geometria (`wires`).
- Coordenadas em unidades de grade inteiras; elétrica em SI.
- Campos desconhecidos são preservados.
- Migrações são funções puras em cadeia, com golden files.
- Binários de firmware são referenciados por hash, não embutidos.

## Consequências

- Diff e merge funcionam; plugins conseguem ler/escrever.
- Arquivos podem ficar grandes; limite de 64 MB e compactação no bundle.
- A validação precisa de schema e regras semânticas.
- JSON Schema é a fonte de verdade do formato.

## Alternativas

- Binário/zip: menor, porém opaco e ruim para diff.
- YAML: mais legível, mas ambíguo e lento.
