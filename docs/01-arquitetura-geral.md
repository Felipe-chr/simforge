# SimForge — Arquitetura Geral

> Documento 1 de 3. Visão, módulos, fronteiras, estrutura e fluxo.
> Documentos complementares:
> - [`02-motor-eletrica-dispositivos.md`](02-motor-eletrica-dispositivos.md)
> - [`03-plataforma.md`](03-plataforma.md)

**Status:** proposta de arquitetura — base para ADRs
**Versão do documento:** 1.0
**Público:** arquitetura, desenvolvimento do core, frontend, plugins e integrações

---

## 1. Escopo

O SimForge é um simulador eletrônico profissional com foco inicial em
ESP32-S3: edição visual de circuitos, execução de firmware Arduino, modelos de
componentes, barramentos, instrumentação e integração com mundos externos.

Este documento define a arquitetura de referência. Ele não descreve a
implementação linha a linha; define contratos, fronteiras, fluxos e decisões que
permitem que o produto evolua de um MVP funcional para uma plataforma com
plugins, marketplace, nuvem e IA.

### 1.1 Objetivos de arquitetura

| Objetivo | Como é atendido |
|---|---|
| Núcleo reutilizável | `libsimforge` sem dependência de UI, sistema operacional ou rede |
| Determinismo | tempo virtual, fila de eventos ordenada, RNG semeado, ausência de relógio de parede no loop |
| Extensibilidade | registro de dispositivos, placas, compiladores, solvers e runtimes por contrato |
| Independência de produto | nenhuma referência, include ou dependência do PistaForge |
| Múltiplos hosts | desktop nativo, navegador (WASM), CLI/CI, servidor e API C embarcada |
| Performance | ECS híbrido, pools, eventos em lote, ilhas de simulação, caminho quente sem locks |
| Segurança | firmware em sandbox, plugins por nível de confiança, validação de projetos |
| Evolução | formatos versionados, ABI C estável, camadas anticorrupção |

### 1.2 Não objetivos da v1

- Emular Wi-Fi/Bluetooth com fidelidade de rádio.
- Ser um simulador SPICE de propósito geral.
- Compilar qualquer biblioteca Arduino que acesse registradores do ESP32.
- Garantir determinismo bit a bit entre arquiteturas diferentes sem replay.
- Marketplace aberto sem curadoria e assinatura.

---

## 2. Princípios

1. **O core é a única fonte de verdade.** Estados elétricos, tempo, validação,
   execução de firmware e persistência vivem em `libsimforge`. React desenha,
   captura entrada e apresenta dados.
2. **Tempo é virtual e explícito.** Nenhum módulo do core lê o relógio do
   sistema durante a simulação. O tempo entra por uma porta (`IClock`) apenas no
   host.
3. **Eventos, não polling.** A simulação avança por eventos agendados. Polling
   existe somente na borda (UI, integração externa), nunca no motor.
4. **Contratos antes de implementações.** Dispositivos, placas, compiladores,
   runtimes, solvers e mundos externos são portas com contratos versionados.
5. **Determinismo é uma propriedade testável.** Toda execução produz um hash de
   trace; duas execuções iguais devem produzir o mesmo hash.
6. **Fronteiras explícitas.** O core não conhece React, WebSocket, SQLite,
   Arduino CLI, QEMU ou PistaForge. Cada um desses é um adaptador.
7. **Falha contida.** Firmware e plugins de terceiros nunca derrubam o processo
   principal; execução em sandbox e supervisão com watchdog.
8. **Compatibilidade aditiva.** Formatos e APIs evoluem sem quebrar leitores
   antigos; mudanças incompatíveis exigem nova versão de schema/ABI.
9. **Sem código compartilhado com o PistaForge.** Integração somente por
   protocolos públicos e formatos documentados.
10. **Performance é orçamento.** Cada caminho quente tem meta explícita e
    benchmark contínuo.

### 2.1 Regra de dependência

```
web  ──►  api  ──►  application  ──►  domain (core)
                                      ▲
adapters (runtime, compiler, storage, transport, external world)
     └──────────── implementam portas do domínio ────────────┘
```

O domínio não importa adaptadores. Adaptadores importam o domínio. A camada
`api` é a única que conhece transporte (WebSocket, C ABI, WASM bindings).

### 2.2 Independência do PistaForge

O SimForge não inclui headers, bibliotecas, binários ou estruturas de dados do
PistaForge. A integração é feita por:

- um protocolo genérico de mundo externo (`IExternalWorld` + `world.*`);
- formatos de arquivo públicos e versionados;
- uma API C/JSON estável.

O repositório do SimForge não possui submódulo, path ou dependência do
PistaForge. Um teste de CI verifica que nenhum include ou símbolo `pistaforge`
aparece no código do produto.

---

## 3. Visão arquitetural

```
┌──────────────────────────────────────────────────────────────────────────┐
│                              FRONTEND (React)                            │
│  Editor esquemático │ Instrumentos │ Serial │ Editor de código │ Plugins │
│  Canvas/WebGL, Zustand, Tailwind — sem regra elétrica ou de simulação    │
└───────────────────────────────┬──────────────────────────────────────────┘
                                │ JSON commands + eventos + frames binários
┌───────────────────────────────▼──────────────────────────────────────────┐
│                               API LAYER                                  │
│  Command bus │ subscriptions │ jobs assíncronos │ schema │ auth local    │
│  Transportes: WebSocket │ C ABI │ WASM bindings │ CLI                    │
└───────────────────────────────┬──────────────────────────────────────────┘
                                │
┌───────────────────────────────▼──────────────────────────────────────────┐
│                         SIMULATION ENGINE (core)                        │
│  VirtualClock │ EventQueue │ Scheduler │ Delta cycles │ Trace/Hash       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐  │
│  │ Netlist /    │  │ Device       │  │ Board        │  │ Runtime     │  │
│  │ Electrical   │  │ instances    │  │ instances    │  │ supervisor  │  │
│  │ solver       │  │ (ECS + OOP)  │  │ (MCU + pins) │  │ (sandbox)   │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └─────────────┘  │
└───────┬───────────────────┬───────────────────┬───────────────────┬──────┘
        │                   │                   │                   │
┌───────▼────────┐ ┌────────▼────────┐ ┌────────▼────────┐ ┌────────▼──────┐
│ DEVICE         │ │ BOARD           │ │ COMPILER        │ │ PERSISTENCE   │
│ FRAMEWORK      │ │ FRAMEWORK       │ │ INTEGRATION     │ │ LAYER         │
│ registry, ABI  │ │ esp32s3, ...    │ │ arduino, idf    │ │ JSON, SQLite  │
└───────┬────────┘ └────────┬────────┘ └────────┬────────┘ └───────────────┘
        │                   │                   │
┌───────▼───────────────────▼───────────────────▼──────────────────────────┐
│                          PLUGIN MANAGER                                  │
│  manifest │ ABI check │ assinatura │ trust tier │ hot reload │ sandbox   │
└───────────────────────────────┬──────────────────────────────────────────┘
                                │
┌───────────────────────────────▼──────────────────────────────────────────┐
│                         INTEGRATION LAYER                                │
│  IExternalWorld │ protocolo world.* │ adaptador PistaForge (externo)     │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Os três planos

| Plano | Responsabilidade | Não pode |
|---|---|---|
| **Edição** | Projeto, netlist, biblioteca, validação estática, undo/redo | Avançar o tempo |
| **Simulação** | Tempo, eventos, solver, runtime, instrumentação | Editar o projeto diretamente |
| **Integração** | Compilação, arquivos, plugins, mundo externo, nuvem | Alterar estado elétrico sem passar pelo engine |

O estado editável é imutável durante uma execução. Ao iniciar a simulação, o
engine recebe um **snapshot** do projeto (`CompiledCircuit`). Alterações durante
a execução são enfileiradas e aplicadas em um `sim.reset` transacional, ou
bloqueadas por política (padrão: bloqueadas com erro `sim.busy`).

---

## 4. Módulos

### 4.1 Frontend

**Responsabilidade:** editor visual, instrumentos, depuração e apresentação.

- Editor esquemático em Canvas/WebGL (PixiJS ou renderer próprio), com camadas:
  grade, fios, símbolos, pinos, seleção, overlays.
- Componentes React para ribbon, biblioteca, inspetor, console serial, editor de
  código (Monaco), gráficos (uPlot/ECharts) e diálogos.
- Store de projeção (`Zustand`) alimentada por eventos do core. O frontend pode
  manter estado efêmero de UI (zoom, seleção, painéis), nunca estado elétrico.
- Hit-testing visual e snapping são permitidos como UX; a decisão final de
  conexão, conflito e validação é do core (`circuit.connect`, `circuit.validate`).
- Interpolação visual entre frames de simulação é permitida e explicitamente
  marcada como apresentação; não altera o estado simulado.

**Contrato:** cliente TypeScript gerado a partir dos schemas da API. Nenhum
`import` de código C++.

### 4.2 API Layer

**Responsabilidade:** traduzir transporte em comandos de domínio e eventos.

- Command bus com métodos namespaced (`project.*`, `circuit.*`, `sim.*`,
  `device.*`, `board.*`, `firmware.*`, `plugin.*`, `world.*`, `ai.*`).
- Validação de entrada por JSON Schema antes de tocar no domínio.
- Jobs assíncronos para compilação, carga de firmware, importação e exportação.
- Subscriptions com sequência monotônica, coalescência e backpressure.
- Erros estruturados com código estável, `details`, `retryable` e `trace_id`.
- Transportes: WebSocket (desktop/navegador), C ABI (embedding/PistaForge),
  bindings WASM (browser/cloud), CLI headless.
- Autenticação local por token efêmero; bind em loopback por padrão.

### 4.3 Simulation Engine

**Responsabilidade:** tempo, eventos, convergência elétrica e orquestração dos
runtimes.

- `VirtualClock` em nanossegundos inteiros.
- `EventQueue` com ordenação determinística `(time, phase, seq)`.
- `Scheduler` com fases e delta cycles.
- `Circuit` compilado: nets, pinos, dispositivos e modelos.
- `ElectricalSolver` plugável: `LogicSolver` (MVP) e `MnaSolver` (evolução).
- `RuntimeSupervisor`: carrega, executa, pausa, reinicia e isola firmware.
- `Recorder`/`Replayer`: trace binário com hash e snapshots periódicos.
- `Instrumentation`: tensões, correntes, waveforms, contadores, profiler.

### 4.4 Device Framework

**Responsabilidade:** ciclo de vida e contrato de modelos de componentes.

- Catálogo de descritores (pinos, parâmetros, símbolo, footprint, capacidades).
- Fábrica e registro por `type_id` + versão.
- Contratos: `IDevice`, `IPinListener`, `IBusParticipant`, `IAnalogElement`,
  `IDisplaySink`, `IAudioSink`, `ISerializable`.
- Camadas de fidelidade por dispositivo: funcional, elétrica, temporal.
- Biblioteca embutida + plugins.

### 4.5 Board Framework

**Responsabilidade:** compor MCU, pinagem, alimentação, periféricos onboard e
runtime.

- `BoardDescriptor` declarativo (JSON) + `IBoardBuilder` quando há lógica.
- Mapa de pinos Arduino ↔ GPIO, capacidades (ADC, PWM, strapping, input-only).
- Periféricos onboard: LED, botão BOOT, USB-UART, regulador, PSRAM/flash.
- Múltiplas placas no mesmo netlist, com domínio de terra comum e barramentos
  compartilhados.
- Placas futuras entram por plugin sem recompilar o core.

### 4.6 Compiler Integration

**Responsabilidade:** transformar código-fonte em artefato executável pelo
runtime.

- Backends: `ArduinoShimBackend` (MVP), `PlatformIOBackend`, `ArduinoCliBackend`,
  `EspIdfBackend` (futuro).
- Pipeline isolado: staging, injeção de shim/variant, build, parse de
  diagnósticos, cache por hash, empacotamento do artefato.
- Sem "upload" real: o artefato é entregue ao runtime virtual.
- Relatório de memória (flash/RAM estimadas) e mapa de símbolos.

### 4.7 Persistence Layer

**Responsabilidade:** projetos, assets, catálogo, cache, histórico e gravações.

- Projeto canônico em JSON `.sforge`.
- Índice e estado de workspace em SQLite (WAL).
- Assets endereçados por conteúdo (SHA-256).
- Pacotes de componentes `.sfpkg` assinados.
- Builds, gravações e telemetria em cache local com retenção configurável.
- Escrita atômica, autosave e journal de recuperação.

### 4.8 Runtime Layer

**Responsabilidade:** executar firmware real ou compilado para o shim.

- `WasmRuntime` (sandbox padrão do MVP, determinístico e portátil).
- `NativeShimRuntime` (desempenho, desenvolvimento).
- `QemuRuntime` (evolução, firmware Xtensa real).
- `ReplayRuntime` (firmware gravado, testes de integração).
- Protocolo comum `IRuntime` e `RuntimeStatus`.
- Orçamento de execução por fatia, watchdog e modelo de custo de API.

### 4.9 Plugin Manager

**Responsabilidade:** descobrir, validar, carregar, isolar e atualizar plugins.

- Manifesto, ABI C, versão, capabilities e permissões.
- Trust tiers: built-in, assinado, comunidade.
- Hot reload em modo desenvolvedor com drenagem de instâncias.
- Processo separado para plugins não confiáveis e runtimes.
- Registro de dispositivos, placas, compiladores, exporters e mundos externos.

### 4.10 Integration Layer

**Responsabilidade:** conectar o SimForge ao mundo externo sem conhecer o
produto do outro lado.

- `IExternalWorld`: descrever entidades, ler sensores, acionar atuadores,
  publicar/assinar eventos.
- Protocolo `world.*` versionado sobre WebSocket, pipe ou shared memory.
- Semântica temporal explícita: `sync(t)`, sample-and-hold, timestamps e
  política de timeout.
- Adaptador do PistaForge é um processo externo que implementa o protocolo; o
  SimForge o trata como qualquer outro mundo.

---

## 5. Diagrama de dependências

```
                         ┌─────────────┐
                         │   web/      │  React + TS
                         └──────┬──────┘
                                │ schema gerado (OpenAPI/JSON Schema)
                         ┌──────▼──────┐
                         │  api/       │  comandos + eventos
                         └──┬───────┬──┘
              ┌─────────────┘       └──────────────┐
       ┌──────▼──────┐                        ┌─────▼──────┐
       │ application │                        │  adapters  │
       │ (jobs,      │                        │ ws, sqlite,│
       │  sessões)   │                        │ compiler,  │
       └──────┬──────┘                        │ runtime,   │
              │                               │ world      │
       ┌──────▼───────────────────────────────▼─────┐
       │                  domain                    │
       │  time │ events │ circuit │ devices │ board │
       │  solver │ runtime-iface │ validation       │
       └──────────────────┬─────────────────────────┘
                          │
                 ┌────────▼────────┐
                 │  platform (IF)  │
                 │ clock, fs, net, │
                 │ threads, crypto │
                 └─────────────────┘
```

Regras:

- `domain` não depende de `application`, `adapters` ou `api`.
- `devices` e `boards` dependem de `domain`, nunca de `application`.
- `runtime` implementa uma porta de `domain` e pode depender de `platform`.
- `web` depende apenas do contrato gerado.

---

## 6. Estrutura de diretórios

```
simforge/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ vcpkg.json
├─ README.md
├─ docs/
│  ├─ 01-arquitetura-geral.md
│  ├─ 02-motor-eletrica-dispositivos.md
│  ├─ 03-plataforma.md
│  ├─ adr/
│  ├─ formatos/
│  │  ├─ sforge-v1.md
│  │  ├─ sfpkg-v1.md
│  │  └─ sfrec-v1.md
│  └─ protocolos/
│     ├─ api-v1.md
│     ├─ world-v1.md
│     └─ plugin-abi-v1.md
├─ include/simforge/            # API pública C++ (headers estáveis)
│  ├─ simforge.hpp
│  ├─ time.hpp
│  ├─ circuit.hpp
│  ├─ device.hpp
│  ├─ board.hpp
│  ├─ runtime.hpp
│  ├─ compiler.hpp
│  ├─ external_world.hpp
│  └─ c_api/simforge.h
├─ src/
│  ├─ core/
│  │  ├─ time/                  # clock, duration, conversões
│  │  ├─ event/                 # queue, scheduler, delta cycles
│  │  ├─ ecs/                   # world, archetypes, component pools
│  │  ├─ electrical/            # pin, net, resolver, solvers
│  │  ├─ circuit/               # netlist, compilação, validação, ERC
│  │  ├─ device/                # framework, registry, lifecycle
│  │  ├─ board/                 # framework, pin mux, power tree
│  │  ├─ runtime/               # IRuntime, supervisor, sandbox
│  │  ├─ firmware/              # shim ABI, artefatos, build cache
│  │  ├─ recording/             # trace, snapshots, replay, hash
│  │  ├─ instrumentation/       # probes, waveforms, métricas
│  │  └─ platform/              # interfaces e implementações host
│  ├─ devices/                  # biblioteca embutida (led, button, ...)
│  ├─ boards/                   # esp32s3, futuros
│  ├─ runtime/
│  │  ├─ wasm/                  # wasmtime, host functions
│  │  ├─ native/                # dll/so
│  │  ├─ qemu/                  # bridge de co-simulação (futuro)
│  │  └─ replay/
│  ├─ compiler/
│  │  ├─ shim/                  # Arduino Compatibility Layer
│  │  ├─ platformio/
│  │  ├─ arduino_cli/
│  │  └─ esp_idf/
│  ├─ persistence/
│  │  ├─ project/               # .sforge, migrações
│  │  ├─ sqlite/
│  │  ├─ assets/
│  │  └─ packages/
│  ├─ integration/
│  │  ├─ external_world/
│  │  └─ protocol/
│  ├─ api/
│  │  ├─ commands/
│  │  ├─ events/
│  │  ├─ validation/
│  │  ├─ transport_ws/
│  │  ├─ transport_c/
│  │  └─ bindings_wasm/
│  └─ app/
│     ├─ desktop/               # shell nativo + webview
│     ├─ server/                # modo servidor/headless
│     └─ cli/
├─ plugins/
│  ├─ sdk/include/simforge/plugin/
│  ├─ examples/
│  └─ manifest.schema.json
├─ web/
│  ├─ package.json
│  ├─ vite.config.ts
│  ├─ src/
│  │  ├─ app/
│  │  ├─ features/editor/
│  │  ├─ features/simulation/
│  │  ├─ features/instruments/
│  │  ├─ features/serial/
│  │  ├─ features/code/
│  │  ├─ lib/api/               # cliente gerado + transporte
│  │  ├─ lib/render/
│  │  └─ stores/
│  └─ public/
├─ tests/
│  ├─ unit/
│  ├─ integration/
│  ├─ contracts/
│  ├─ golden/
│  ├─ fuzz/
│  ├─ determinism/
│  └─ bench/
├─ tools/
│  ├─ schema-gen/
│  ├─ package-sign/
│  └─ trace-inspect/
├─ samples/
│  ├─ blink/
│  ├─ button-led/
│  ├─ oled-i2c/
│  └─ traffic-light/
└─ third_party/                 # via vcpkg/submódulos fixados
```

### 6.1 Alvos CMake

| Alvo | Tipo | Depende de |
|---|---|---|
| `simforge_core` | lib estática/shared | platform, third_party mínimos |
| `simforge_devices` | lib | core |
| `simforge_boards` | lib | core, devices |
| `simforge_runtime` | lib | core, platform |
| `simforge_compiler` | lib | core, platform (processos) |
| `simforge_persistence` | lib | core, sqlite, nlohmann |
| `simforge_integration` | lib | core, rede |
| `simforge_api` | lib | todas as anteriores |
| `simforge_c` | shared | api (C ABI) |
| `simforge_wasm` | wasm | core, devices, boards, runtime (sem fs/rede) |
| `simforge_desktop` | exe | api + webview |
| `simforge_server` | exe | api + ws |
| `simforge_cli` | exe | api |
| `simforge_tests` | exe | api + doctest/Catch2 |

Opções de build: `SIMFORGE_BUILD_WASM`, `SIMFORGE_BUILD_DESKTOP`,
`SIMFORGE_ENABLE_SQLITE`, `SIMFORGE_ENABLE_NET`, `SIMFORGE_SANITIZERS`,
`SIMFORGE_STRICT_DETERMINISM`.

### 6.2 Perfis de build

| Perfil | Alvo | Características |
|---|---|---|
| `desktop` | Windows/Linux/macOS | núcleo completo, SQLite, plugins nativos, WebView |
| `server` | Linux/Windows | headless, WS, múltiplas sessões, sem UI |
| `wasm` | navegador/nuvem | sem fs/rede nativos, runtime WASM aninhado ou replay, memória limitada |
| `embedded` | longo prazo | sem heap após init, sem exceções/RTTI, containers de capacidade fixa, subconjunto de dispositivos |

O perfil `embedded` não é escopo da v1. A arquitetura já o viabiliza ao manter o
domínio livre de sistema operacional e ao concentrar alocação, threads e I/O em
portas de plataforma. Um build embarcado usaria apenas `core` + `devices`
selecionados + um runtime de firmware gravado ou interpretado, sem editor.

---

## 7. Fluxos de execução

### 7.1 Inicialização

```
main
 │
 ├─ parse CLI / config
 ├─ Platform::create()            (fs, clock, threads, crypto)
 ├─ PluginManager::discover()     (built-in + instalados)
 ├─ Registry::freeze()            (snapshot de dispositivos/placas)
 ├─ SessionManager::create()
 ├─ API::listen(loopback:port)    (WS) e/ou C ABI
 ├─ DesktopShell::show(web/)      (WebView2/WKWebView/WebKitGTK)
 └─ aguarda comandos
```

### 7.2 Comando da UI

```
React                API                 Application           Domain
  │  circuit.connect  │                       │                   │
  ├──────────────────►│ validate schema       │                   │
  │                   ├──────────────────────►│ load snapshot     │
  │                   │                       ├──────────────────►│
  │                   │                       │  Netlist::connect │
  │                   │                       │  ERC::check       │
  │                   │                       │◄──────────────────┤
  │                   │                       │  diff + revision  │
  │                   │◄──────────────────────┤                   │
  │◄──────────────────┤ result + event        │                   │
  │  atualiza canvas  │                       │                   │
```

### 7.3 Execução da simulação

```
UI          API          Engine            Runtime(sandbox)       Circuit
│ sim.run    │              │                    │                  │
├───────────►│ enqueue      │                    │                  │
│            ├─────────────►│ snapshot           │                  │
│            │              ├───────────────────►│ load artifact    │
│            │              │                    │                  │
│            │              │  loop: next event  │                  │
│            │              │◄───────────────────┤ yield/sleep      │
│            │              │  advance virtual t │                  │
│            │              │  resolve nets      │                  │
│            │              ├───────────────────►│ pin changes      │
│            │              │                    │                  │
│            │              │  batch frame       │                  │
│◄───────────┤◄─────────────┤  (60 Hz)           │                  │
│ render     │              │                    │                  │
```

### 7.4 Compilação e carga

```
UI ──firmware.build──► CompilerService
                         ├─ hash(fonte + libs + board + shim)
                         ├─ cache hit? ──► reutiliza artefato
                         ├─ staging dir isolado
                         ├─ backend (PlatformIO/arduino-cli/shim)
                         ├─ parse de diagnósticos
                         ├─ artefato (.wasm/.dll/.elf) + symbols
                         └─ evento firmware.build.progress
UI ──sim.loadFirmware──► RuntimeSupervisor ──► sandbox
```

### 7.5 Reset transacional

Toda alteração estrutural durante uma sessão segue:

1. `sim.pause` (implícito se `allow_live_edit=false`).
2. Snapshot do estado de UI e instrumentos.
3. Recompilação do circuito.
4. Recriação de dispositivos e nets.
5. Restauração de firmware, breakpoints e probes.
6. `sim.reset` com nova revisão e `revision_id` incrementado.

---

## 8. Concorrência e threads

```
┌──────────────┐   comandos    ┌──────────────┐   eventos    ┌──────────────┐
│ UI thread    │──────────────►│ API thread   │─────────────►│ Sim thread   │
│ (webview)    │◄──────────────│ (serializa)  │◄─────────────│ (autoridade) │
└──────────────┘   frames      └──────────────┘   snapshots  └──────┬───────┘
                                                                     │
                                              ┌──────────────────────┼───────────────┐
                                              │                      │               │
                                       ┌──────▼──────┐        ┌──────▼──────┐  ┌─────▼──────┐
                                       │ Runtime A   │        │ Compiler    │  │ I/O / log  │
                                       │ (processo/  │        │ (processo)  │  │ (thread)   │
                                       │  wasm)      │        │             │  │            │
                                       └─────────────┘        └─────────────┘  └────────────┘
```

Regras:

- O **sim thread** é o único que muta estado elétrico. Comandos entram por uma
  fila MPSC; snapshots saem por double-buffer SPSC.
- A UI nunca bloqueia o sim thread: comandos retornam `accepted` + `job_id` ou
  resultado imediato de leitura.
- Compilação e runtime não confiável rodam em processos separados.
- Instrumentação pesada (waveforms) é agregada no sim thread e serializada em
  lote; nunca faz I/O síncrono no caminho quente.
- Em modo determinístico, o sim thread é single-threaded por ilha. Paralelismo
  só entre ilhas independentes ou em funções puras com resultados ordenados.

### 8.1 Modelo de sincronização com o runtime

```
sim time ──────────────────────────────────────────────────────────────►
          │ slice k        │ slice k+1       │ slice k+2
          │                │                 │
runtime   ████ runUntil(t) ████ runUntil(t)  ████ ...
          │  yields/delay  │  yields/delay   │
          │                │                 │
engine    resolve nets     resolve nets      resolve nets
          advance analog   advance analog    advance analog
          commit frame     commit frame      commit frame
```

- Fatia padrão: 1 ms (configurável de 100 µs a 10 ms).
- O runtime recebe um orçamento de execução e devolve `Yielded`, `Sleeping`,
  `Halted` ou `Error`.
- Leituras de pino dentro da fatia usam o estado latched do último commit;
  sensores analógicos podem interpolar no tempo exato da leitura (configurável).
- Em modo tempo real, o engine permite que o runtime rode adiantado dentro de
  uma janela (padrão 20 ms) e o throttle é aplicado no commit.

---

## 9. Design patterns

| Pattern | Onde | Por quê |
|---|---|---|
| Ports & Adapters (Hexagonal) | Core vs runtime/compiler/storage/world | Isolar domínio e permitir hosts distintos |
| Strategy | `IElectricalSolver`, `IRuntime`, `ICompilerBackend` | Trocar fidelidade e backend sem tocar no engine |
| Factory + Registry | Dispositivos, placas, plugins | Carregamento dinâmico por `type_id` |
| Observer / Publish-Subscribe | Pinos, nets, eventos de simulação | Desacoplar modelos e instrumentação |
| Command | API e undo/redo | Serialização, auditoria, replay |
| State | Máquinas de estado de dispositivos e barramentos | Clareza em protocolos (I2C, 1-Wire) |
| Template Method | `DeviceBase`, `BoardBase` | Ciclo de vida comum com customização pontual |
| Composite | Placa = MCU + periféricos + dispositivos | Composição uniforme |
| Flyweight | Descritores, símbolos, fontes | Milhares de instâncias com metadados compartilhados |
| Object Pool / Arena | Eventos, snapshots, buffers de trace | Evitar alocação no caminho quente |
| PIMPL / Type Erasure | API pública e ABI C | Estabilidade binária |
| Circuit Breaker | Plugins e runtime | Conter falhas e evitar cascata |
| Saga / Unit of Work | Aplicação de patches de IA e edição em lote | Transação com rollback |

---

## 10. Escalabilidade

### 10.1 Dimensões

| Dimensão | MVP | Alvo v1 | Alvo longo prazo |
|---|---|---|---|
| Dispositivos por projeto | 200 | 5.000 | 50.000 |
| Nets | 300 | 10.000 | 100.000 |
| Placas simultâneas | 1 | 4 | 32 |
| Eventos digitais/s | 100k | 1M | 10M |
| Frequência de UI | 60 fps | 60 fps | 120 fps |
| Sessões por host (cloud) | 1 | 8 | 100+ (WASM isolates) |

### 10.2 Estratégias

1. **Ilhas de simulação.** Nets desconexos formam ilhas independentes. O
   scheduler processa ilhas em paralelo e faz merge determinístico por
   `(time, island_id, seq)`.
2. **Compilação do circuito.** O netlist editável é compilado para arrays
   compactos: pinos, nets, adjacências, drivers. Nada de ponteiros ricos no
   caminho quente.
3. **Eventos em lote.** Eventos do mesmo timestamp e mesma net são agrupados;
   uma resolução por net por delta cycle.
4. **Modelos por nível de detalhe.** Dispositivos simples usam representação
   tabular; modelos caros (OLED, NeoPixel) só processam eventos relevantes.
5. **Backpressure na UI.** Frames coalescidos; waveforms amostrados; nunca
   enviar um evento por pino para o frontend.
6. **Cache de compilação.** Hash de fontes, bibliotecas, placa e shim evita
   rebuilds.
7. **Nuvem.** Cada sessão é um isolate WASM com limites de CPU/memória; o
   orquestrador escala horizontalmente e persiste traces, não processos.

---

## 11. Decisões que viram ADR

| ADR | Tema |
|---|---|
| 0001 | Core C++20, sem UI, com API C estável |
| 0002 | Independência total do PistaForge |
| 0003 | Tempo virtual inteiro em nanossegundos |
| 0004 | Fila de eventos determinística com fases e delta cycles |
| 0005 | `LogicSolver` no MVP e `MnaSolver` como evolução |
| 0006 | Firmware em WASM como sandbox padrão do MVP |
| 0007 | ABI C versionada para plugins |
| 0008 | `.sforge` JSON com schema inteiro e migrações |
| 0009 | `IExternalWorld` como única fronteira de integração |
| 0010 | Frontend sem lógica de simulação |
| 0011 | Determinismo: sem wall clock, sem unordered, sem fast-math |
| 0012 | Trust tiers e assinatura de plugins |
| 0013 | Compilação em processo isolado com cache por hash |
| 0014 | API WebSocket + C ABI compartilhando o mesmo command bus |
| 0015 | Limites de recursos e watchdog para firmware |
