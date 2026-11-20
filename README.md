# SimForge

Simulador eletrônico profissional com foco inicial em **ESP32-S3**: edição
visual de circuitos, execução de firmware Arduino, componentes, barramentos,
instrumentação e integração com mundos externos.

- **Core:** C++20, CMake, modular, multiplataforma (`libsimforge`)
- **Frontend:** React + TypeScript + Vite + Tailwind (sem lógica de simulação)
- **Formato:** `.sforge` (JSON versionado)
- **Integração:** API C/JSON + protocolo `world.*` (independente de produto)
- **Status:** núcleo funcional (motor lógico, `.sforge`, CommandBus, C ABI e CLI);
  frontend visual em `web/` falando com o núcleo pela ponte `simforge-cli api`
  (com `MockTransport` de fallback)

## Rodar a interface (um comando)

```powershell
tools\run.cmd
```

Compila o núcleo se ainda não houver `build\release\src\simforge-cli.exe`, instala
as dependências do frontend na primeira vez, sobe o dev server em
<http://localhost:5173> e abre o navegador. A UI conversa com o núcleo real pela
ponte `simforge-cli api`; sem o executável, cai no mock.

```powershell
tools\run.cmd -Rebuild      # recompila o núcleo antes de subir
tools\run.cmd -Port 5174    # outra porta
tools\run.cmd -NoOpen       # não abre o navegador
```

Os `.cmd` existem para não depender da ExecutionPolicy do PowerShell. O
equivalente explícito é
`powershell -NoProfile -ExecutionPolicy Bypass -File tools\run.ps1`.

## Compilação e testes

Requisitos: CMake ≥ 3.24, Ninja e MSVC (Visual Studio com workload C++). O
script localiza a toolchain via `vswhere` e configura o ambiente sozinho.

```powershell
# Build + testes (preset release; recomendado no Windows com WDAC)
tools\build.cmd -Preset release -Test

# Equivalente sem o atalho
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -Preset release -Test

# Build de desenvolvimento
tools\build.cmd -Preset dev

# Ou direto pelo CMake
cmake --preset release
cmake --build --preset release
ctest --preset release
```

> **WDAC/Smart App Control.** Algumas máquinas corporativas bloqueiam
> executáveis grandes recém-compilados. O `.exe` de teste Debug tem ~1,9 MB e
> pode ser bloqueado; o Release (~0,5 MB) roda. Use `-Preset release` para
> testar nesses ambientes.

## CLI

```powershell
simforge-cli version
simforge-cli validate projeto.sforge
simforge-cli run projeto.sforge --until-ns 1000000

# NDJSON sobre o mesmo CommandBus da API C
'{"id":"1","method":"app.info"}' | simforge-cli api
```

## Uso da biblioteca

Dois pontos de entrada, ambos estáveis:

```cpp
// C++20: header guarda-chuva
#include "simforge/simforge.hpp"

simforge::Session session;
const std::string response =
    session.execute(R"({"id":"1","method":"app.info","params":{}})");
```

```c
/* ABI C, para FFI e integradores */
#include "simforge/c_api/simforge.h"

sf_session* session = NULL;
sf_session_create(NULL, &session);
/* ... sf_session_execute / sf_session_subscribe ... */
sf_session_destroy(session);
```

O contrato de comandos, erros e eventos está em
[`docs/protocolos/api-v1.md`](docs/protocolos/api-v1.md).

## Exemplos

| Arquivo | Conteúdo |
|---|---|
| [`examples/blink-led.sforge`](examples/blink-led.sforge) | ESP32-S3 DevKitC, LED em `GPIO2`, botão em `GPIO0`, três nets |

Os exemplos são carregados pelo teste `project_example_blink_led_is_valid`, que
garante que continuam abrindo e validando sem diagnósticos.

## Testes

O harness é próprio (ADR 0019), sem dependências externas. `ctest` cobre JSON,
elétrica (tabela-verdade 16 combinações), determinismo do motor, projeto
(`.sforge` round-trip e exemplo), CommandBus/API v1 e a ABI C em runtime.

```powershell
.\tools\test.ps1                 # release, todos os testes
.\tools\test.ps1 -Filter session # só um grupo
```

## Benchmarks

`simforge_bench` mede os critérios de `docs/spikes/` e fica **fora** do ctest:
medição não deve quebrar CI.

```powershell
.\build\release\bench\simforge_bench.exe            # tabela no console
.\build\release\bench\simforge_bench.exe --markdown # linhas para a doc
.\build\release\bench\simforge_bench.exe --check    # falha se exceder limite
```

Resultados registrados em
[`docs/spikes/resultados.md`](docs/spikes/resultados.md).

## Frontend (`web/`)

Camada visual em React 19 + TypeScript + Vite + Tailwind 4. **Não há lógica de
simulação no frontend** (ADR 0010): a UI envia comandos e desenha as respostas no
envelope da [`api-v1`](docs/protocolos/api-v1.md).

```powershell
cd simforge\web
& "$env:LOCALAPPDATA\nodejs\npm.cmd" install
& "$env:LOCALAPPDATA\nodejs\npm.cmd" run dev    # http://localhost:5173
& "$env:LOCALAPPDATA\nodejs\npm.cmd" run build  # tsc -b + vite build
```

- Transporte padrão: ponte `CliTransport` sobre a `simforge-cli api` (NDJSON),
  com `GET /sf/status` e `POST /sf/command` no dev server. Sem o executável, cai
  no `MockTransport` (`src/lib/api/mock.ts`), que implementa os mesmos métodos
  em processo com o cenário `blink-led` — o mesmo de
  [`examples/blink-led.sforge`](examples/blink-led.sforge).
- A aba **Bancada** força/libera pinos com `sim.debug.setPin` /
  `sim.debug.releasePin`; no exemplo, `GPIO2 = high` acende o LED.
- Para outro transporte (WebSocket, ABI C), basta implementar `Transport` e
  registrar em `src/lib/api/index.ts`; `src/stores/workspace.tsx` não muda.
- Detalhes de estrutura e decisões: [`web/README.md`](web/README.md).

> Node nesta máquina veio de ZIP em `%LOCALAPPDATA%\nodejs` (o MSI exige
> elevação) e `npm.ps1` é bloqueado pela ExecutionPolicy — use `npm.cmd`.

## Documentos

| Documento | Conteúdo |
|---|---|
| [`docs/01-arquitetura-geral.md`](docs/01-arquitetura-geral.md) | Visão, módulos, dependências, diretórios, fluxos, concorrência, escalabilidade |
| [`docs/02-motor-eletrica-dispositivos.md`](docs/02-motor-eletrica-dispositivos.md) | Motor de eventos, modelo elétrico, solvers, frameworks de dispositivo/placa, ESP32-S3, compilação |
| [`docs/03-plataforma.md`](docs/03-plataforma.md) | Plugins, `.sforge`, persistência, `IExternalWorld`, performance, segurança, IA, roadmap, riscos, MVP, stack, complexidade |
| [`docs/04-spikes-fase-0.md`](docs/04-spikes-fase-0.md) | Spikes técnicos, critérios de saída e go/no-go |

## Contratos formalizados

| Contrato | Documento |
|---|---|
| API de comandos e eventos | [`docs/protocolos/api-v1.md`](docs/protocolos/api-v1.md) |
| Formato de projeto `.sforge` | [`docs/formatos/sforge-v1.md`](docs/formatos/sforge-v1.md) + [schema](docs/formatos/sforge-v1.schema.json) |
| Gravação/replay `.sfrec` | [`docs/formatos/sfrec-v1.md`](docs/formatos/sfrec-v1.md) |
| Pacotes `.sfpkg` | [`docs/formatos/sfpkg-v1.md`](docs/formatos/sfpkg-v1.md) + [schema](docs/formatos/sfpkg-v1.schema.json) |
| Mundo externo `world.*` | [`docs/protocolos/world-v1.md`](docs/protocolos/world-v1.md) + [schema](docs/protocolos/world-v1.schema.json) |
| ABI de plugins | [`docs/protocolos/plugin-abi-v1.md`](docs/protocolos/plugin-abi-v1.md) + [manifesto](docs/protocolos/plugin-v1.schema.json) |
| ADRs (19) | [`docs/adr/README.md`](docs/adr/README.md) |

## Validação dos contratos

```bash
pip install jsonschema
python tools/validate_contracts.py
```

O script valida os cinco schemas como JSON Schema 2020-12, checa cada exemplo
publicado contra o schema correspondente, garante que todos os blocos JSON da
documentação parseiam e que nenhum link relativo está quebrado. Ele é a base do
job de CI dos contratos.

## Princípios

1. Toda a simulação vive em `libsimforge`; a UI apenas desenha e envia comandos.
2. Tempo virtual, fila de eventos e execução determinística.
3. Dispositivos, placas, compiladores, runtimes e mundos externos são plugins.
4. O SimForge não depende do PistaForge; a integração é feita por APIs e
   formatos públicos.
5. Compatibilidade aditiva: schemas e ABI evoluem sem quebrar consumidores.
6. Firmware e plugins de terceiros executam em sandbox.

## MVP (90 dias)

LED, botão, GPIO, Serial e PWM com firmware Arduino real, editor visual,
`.sforge` e API local — detalhado em
[`docs/03-plataforma.md`](docs/03-plataforma.md#10-mvp-de-90-dias).
