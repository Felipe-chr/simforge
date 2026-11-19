# ADR 0019 — Zero dependências externas na fase 0

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0008, ADR 0014

## Contexto

A arquitetura previa `nlohmann/json` para persistência e `doctest`/Catch2 para
testes. Ambos adicionam download, versão e tempo de build, e o ambiente do
projeto não tem gerenciador de pacotes disponível. Além disso, o formato
`.sforge` exige **ordem de chaves estável** e **preservação de campos
desconhecidos**, que o `nlohmann::json` (mapa ordenado, mas sem ordem de
inserção) não entrega sem camada extra.

## Decisão

- A fase 0 usa um DOM JSON interno (`src/core/json/`) com:
  - ordem de inserção preservada;
  - números guardados como texto + `int64`/`double` quando aplicável;
  - erro com linha e coluna;
  - serialização canônica.
- Os testes usam um harness mínimo próprio (`tests/framework.hpp`).
- Nenhuma dependência de terceiros é obrigatória para compilar, testar ou rodar
  o núcleo.
- Dependências futuras (SQLite, wasmtime, WebSocket) entram por alvo e por
  perfil de build, nunca no núcleo.

## Consequências

- `git clone && cmake --build` funciona offline.
- O DOM é código nosso: precisa de testes, fuzzing e revisão de segurança.
- Performance e cobertura de JSON Schema são menores que uma lib madura; o
  validador de schema continua externo (`tools/validate_contracts.py`).
- Quando uma dependência se provar necessária, ela entra por ADR e fica
  isolada no alvo que a usa.

## Alternativas

- `nlohmann/json`: maduro, porém pesado e sem ordem de inserção.
- `rapidjson`: rápido, mas DOM imutável e sem preservação de ordem.
- `simdjson`: excelente leitura, não serve para escrita canônica.
