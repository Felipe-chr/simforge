# ADR 0012 — Trust tiers e assinatura de plugins

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0007, ADR 0013

## Contexto

Um marketplace aberto e plugins nativos ampliam a superfície de ataque. Nem
todo plugin pode rodar com os mesmos privilégios do núcleo.

## Decisão

- Quatro níveis: `builtin`, `verified`, `community`, `untrusted`.
- Pacotes e manifestos são assinados com Ed25519.
- `verified` passa por curadoria; `community` roda in-process com guarda;
  `untrusted` roda em processo separado ou é bloqueado.
- Permissões declaradas no manifesto e aplicadas pelo sandbox.
- Plugin que crasha duas vezes é quarentenado.
- Revogação por chave e por hash.

## Consequências

- Segurança proporcional ao risco.
- Fricção para publicar/instalar plugins não assinados.
- Necessidade de infraestrutura de chaves, índice assinado e revogação.
- Processos separados adicionam latência.

## Alternativas

- Todos confiáveis: simples, inaceitável para marketplace.
- Todos em processo separado: seguro, porém lento para componentes simples.
