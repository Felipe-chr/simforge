# ADR 0017 — Preservação de campos desconhecidos

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0008

## Contexto

Plugins e versões futuras adicionam dados ao projeto. Se o leitor atual
descartar esses campos, abrir e salvar com uma versão antiga causa perda
silenciosa.

## Decisão

- O leitor preserva campos desconhecidos no round-trip.
- Campos conhecidos são validados; desconhecidos são opacos.
- Migrações devem preservar extensões; se não for possível, o documento é
  marcado `lossy` e o usuário confirma.
- Escrita é canônica para campos conhecidos e estável para desconhecidos.

## Consequências

- Compatibilidade forward e backward.
- O modelo interno precisa carregar um "resto" opaco por objeto.
- Testes de round-trip com campos sintéticos.
- Risco de dados inválidos persistirem; validação de segurança cobre.

## Alternativas

- Descartar desconhecidos: perda de dados.
- Recusar qualquer campo desconhecido: impede evolução aditiva.
