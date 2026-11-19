# ADR 0013 — Compilação em subprocesso isolado com cache por hash

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0012, ADR 0015

## Contexto

Compilar firmware executa toolchains externas, scripts de bibliotecas e pode
consumir CPU, disco e rede. Fazer isso no processo principal é arriscado e
difícil de limitar.

## Decisão

- Todo build roda em subprocesso isolado, com timeout, quota de disco e sem
  rede (exceto resolução explícita de bibliotecas).
- A entrada é hasheada: fontes, bibliotecas, placa, shim, flags e versão da
  toolchain.
- Cache content-addressed evita rebuilds.
- Diagnósticos são parseados e devolvidos como dados estruturados.
- Nenhum script do projeto executa no host; hooks só em plugins assinados.

## Consequências

- Builds reproduzíveis e seguros.
- Primeiro build mais lento; cache mitiga.
- Necessidade de gerir cache, lockfile e limpeza.
- Erros de toolchain precisam de tradução para a UI.

## Alternativas

- Compilação in-process: risco de crash e difícil de limitar.
- Compilação em nuvem apenas: offline e privacidade.
