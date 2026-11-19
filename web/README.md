# SimForge Web

Interface visual do SimForge: bancada, editor de circuito, inspetor, controles de
simulação e dock (console, serial, código, instrumentos).

> **Escopo atual:** camada de apresentação. A UI não calcula nada — ela envia
> comandos e desenha respostas no envelope da [`api-v1`](../docs/protocolos/api-v1.md).
> Há dois transportes, escolhidos na inicialização:
>
> - **`core`** — ponte com o núcleo C++ de verdade, via `simforge-cli api`
>   (NDJSON). É o padrão quando o executável existe no build `release`.
> - **`mock`** — `MockTransport` em processo (`src/lib/api/mock.ts`) com o
>   cenário `blink-led` que espelha
>   [`examples/blink-led.sforge`](../examples/blink-led.sforge). Serve de
>   referência e de fallback quando não há build do núcleo.

## Como rodar

```powershell
cd simforge\web
& "$env:LOCALAPPDATA\nodejs\npm.cmd" install
& "$env:LOCALAPPDATA\nodejs\npm.cmd" run dev      # http://localhost:5173
& "$env:LOCALAPPDATA\nodejs\npm.cmd" run build    # tsc -b + vite build
& "$env:LOCALAPPDATA\nodejs\npm.cmd" run lint
```

Node nesta máquina foi instalado por ZIP em `%LOCALAPPDATA%\nodejs`, porque o MSI
exige elevação. Use `npm.cmd`: `npm.ps1` é bloqueado pela ExecutionPolicy.

## Estrutura

```
server/
  simforge-cli.ts ponte NDJSON com o núcleo (dev server): /sf/status, /sf/command
src/
  app/            shell da aplicação (Toolbar, Dock, StatusBar, App)
  components/     primitivos de UI (Button, Chip, Panel, Tabs, Field) e ícones
  features/
    editor/       canvas SVG: BoardNode, DeviceNode, WireLayer, Library, Inspector
    simulation/   barra de transporte (run/pause/step/reset/velocidade)
    serial/       monitor serial
    code/         visualizador do sketch
    instruments/  osciloscópio
  lib/
    api/          tipos do contrato api-v1, transportes e resolveTransport()
    render/       geometria de pinos, roteamento de fios, níveis lógicos
    format.ts     tempo virtual, bytes, IDs
  stores/
    workspace.tsx estado da UI e única ponte com o transporte
```

## Contrato

- Toda mensagem segue `ApiRequest` → `ApiResponse` (`src/lib/api/types.ts`),
  com `id`, `ok`, `result`/`error` e `meta.revision`.
- A UI só conhece métodos do catálogo: `app.*`, `project.state`,
  `circuit.*`, `device.catalog`, `board.catalog`, `sim.*`, `sim.debug.*`,
  `events.poll`.
- Estados elétricos chegam prontos em `sim.frame` (`devices[]`, `nets[]`).
  O frontend apenas colore pinos e fios.

## Ligar ao núcleo

A ponte já está implementada e é o caminho padrão em desenvolvimento:

1. Compile o núcleo no preset `release`:
   `powershell -NoProfile -ExecutionPolicy Bypass -File ..\tools\build.ps1 -Preset release`
   (o `simforge-cli.exe` sai em `..\build\release\src\`).
2. `npm run dev`. O plugin `server/simforge-cli.ts` sobe
   `simforge-cli api --project ..\examples\blink-led.sforge` e expõe:
   - `GET /sf/status` — disponibilidade, executável, projeto, velocidade, erro;
   - `POST /sf/command` — um `ApiRequest` → `ApiResponse`, em FIFO.
3. `resolveTransport()` (`src/lib/api/index.ts`) testa `/sf/status` e escolhe:

   | condição | transporte |
   |---|---|
   | `VITE_SIMFORGE_TRANSPORT=core` | `CliTransport` (falha explícita se a ponte cair) |
   | `VITE_SIMFORGE_TRANSPORT=mock` | `MockTransport` |
   | sem variável e ponte `ok` | `CliTransport` |
   | sem variável e sem ponte | `MockTransport` |

A ponte também cuida da cadência: `sim.run` só marca o estado; o avanço do tempo
vem de `sim.step` a cada 40 ms reais (20 ms virtuais × velocidade). A UI nunca
inventa tempo. Ao mudar o projeto (`circuit.*`), o store recarrega o motor com
`sim.load`, porque o núcleo invalida a engine a cada revisão.

```
npm run dev                                   # tenta o núcleo, cai no mock
$env:VITE_SIMFORGE_TRANSPORT = 'mock'; npm run dev
$env:VITE_SIMFORGE_TRANSPORT = 'core'; npm run dev
```

Qualquer outra implementação de `Transport` (WebSocket, ABI C, placa remota)
entra no mesmo ponto, sem tocar em componentes.

## Bancada

A aba **Bancada** do inspetor lista as placas, os pinos ligados a nets e o nível
atual de cada net (`sim.frame`). Os botões `L` / `H` / `Z` enviam
`sim.debug.setPin` e `sim.debug.releasePin`; o LED do exemplo acende ao forçar
`GPIO2 = high` e apaga em `Z` (alta-Z), tanto no núcleo quanto no mock.

## Solução de problemas

- **`LNK1104: não é possível abrir simforge-cli.exe`** — o dev server está com o
  executável em uso. Pare o `npm run dev` (ou mate o processo), recompile e suba
  de novo; o Windows trava o `.exe` aberto pela ponte.
- **`spawn UNKNOWN` / “política de Controle de Aplicativo bloqueou”** — o Smart
  App Control pode bloquear o binário recém-linkado (principalmente depois de um
  link que falhou). Apague o `.exe` e recompile; se persistir, libere a pasta ou
  assine o binário.
- **A UI mostra `api v1 · mock`** — a ponte não respondeu `ok`. Confira
  `/sf/status`, o caminho em `cli` e o campo `error`; sem o build `release` do
  núcleo o mock é o fallback esperado.
- **Depois de recompilar o núcleo** — reinicie o dev server (ou salve
  `server/simforge-cli.ts` para forçar o restart): a ponte sobe um processo por
  servidor e não troca o binário em execução.

## Notas de implementação

- Tailwind 4 via `@tailwindcss/vite`, tokens em `src/index.css` (`shell-*`,
  `signal`, `ok`, `warn`, `err`) e utilidades `grid-canvas`, `scroll-slim`.
- Canvas é SVG com `viewBox` 980×640; zoom altera o `viewBox` (sem
  `transform`, para não desalinhar a conversão de coordenadas).
- Arrasto usa `setPointerCapture` no SVG e snap de 12 unidades.
- TypeScript 7 com `verbatimModuleSyntax` + `erasableSyntaxOnly`: use
  `import type`, sem enums/namespaces.
- Estado da UI em React Context (`src/stores/workspace.tsx`), sem Zustand: o
  protótipo não justifica a dependência (ADR 0019). Se o store crescer, migrar é
  local.
