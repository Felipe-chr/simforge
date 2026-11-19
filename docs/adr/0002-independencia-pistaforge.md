# ADR 0002 — Independência total do PistaForge

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0009

## Contexto

O PistaForge é um produto existente que pode consumir projetos do SimForge no
futuro. Compartilhar código ou domínio criaria acoplamento, versões travadas e
vazamento de regras entre produtos.

## Decisão

- O SimForge não inclui headers, bibliotecas, binários, submódulos ou estruturas
  de dados do PistaForge.
- A integração ocorre exclusivamente por:
  - API C/JSON pública;
  - protocolo `world.*`;
  - formatos documentados (`.sforge`, `.sforgebundle`).
- O adaptador do PistaForge é um processo externo, mantido fora do repositório
  do SimForge.
- Um teste de CI falha se aparecer `pistaforge` em includes/símbolos do produto.

## Consequências

- Os dois produtos evoluem em ritmos independentes.
- A integração tem custo de serialização e versionamento.
- É necessário manter um adaptador e testes de contrato do lado do PistaForge.

## Alternativas

- Biblioteca compartilhada: acoplamento e risco de regressão cruzada.
- API proprietária do PistaForge: viola a independência.
