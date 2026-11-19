# ADR 0020 — Ponte de desenvolvimento NDJSON com `simforge-cli api`

- **Status:** proposto
- **Data:** 2026-11-18
- **Relacionado:** ADR 0010, ADR 0014, ADR 0019

## Contexto

A UI precisa mostrar o núcleo real (motor lógico, `.sforge`, CommandBus) antes de
existir um servidor WebSocket de produção ou um host com a ABI C. O
`simforge-cli api` já fala o envelope da `api-v1` em NDJSON no stdin/stdout, mas
um navegador não abre processos.

Sem uma ponte, o frontend ficaria preso ao `MockTransport` e os bugs de
integração (por exemplo, caminho UTF-8 com acento) só apareceriam na fase 1.

## Decisão

- Um plugin do **dev server** do Vite (`web/server/simforge-cli.ts`) sobe
  `simforge-cli api` e mantém um processo por servidor, reutilizando a sessão.
- Duas rotas locais:
  - `GET /sf/status` — disponibilidade, executável, projeto, velocidade, erro;
  - `POST /sf/command` — um `ApiRequest` → um `ApiResponse`, em FIFO.
- O cliente (`CliTransport`) implementa o mesmo `Transport` do mock.
  `resolveTransport()` escolhe, nesta ordem: `VITE_SIMFORGE_TRANSPORT`,
  ponte `ok`, mock.
- A **cadência** do tempo virtual fica na ponte: `sim.run` marca o estado e a
  ponte envia `sim.step` a cada 40 ms reais (20 ms virtuais × velocidade). A UI
  nunca inventa tempo; passos manuais continuam vindo de `sim.step`.
- Qualquer mutação de projeto no núcleo invalida a engine; o store recarrega com
  `sim.load` antes de pedir frames.
- A ponte é **de desenvolvimento**: não vira superfície de produção, não
  autentica e escuta apenas o dev server.

## Consequências

- A UI exercita o núcleo de verdade com uma única dependência de Node no dev
  server (já necessária para o Vite).
- Bug real encontrado e corrigido: o `std::string` UTF-8 da API era interpretado
  no code page ANSI ao virar `std::filesystem::path`; a conversão foi
  centralizada em `simforge/paths.hpp` e coberta por teste.
- O protocolo da ponte é o mesmo da ABI C, então o teste de integração vale para
  os dois caminhos.
- Latência de processo + JSON aparece na UI; aceitável para desenvolvimento.
  Produção deve usar WebSocket/binário (ADR 0014).
- Se o executável não existir, a UI cai no mock e mostra `api v1 · mock` na
  barra de status.

## Alternativas

- **Núcleo em WASM no navegador:** duplicaria o alvo de build e não cobre o
  motor nativo desta fase.
- **Servidor WebSocket agora:** mais trabalho e mais superfície; o plugin do
  Vite resolve o ciclo de feedback sem comprometer a arquitetura.
- **ABI C via Electron/Tauri:** válido no desktop, mas acopla o protótipo a um
  shell nativo antes da hora.
