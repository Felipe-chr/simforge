# SimForge — Plataforma, Integração, Operação e Roadmap

> Documento 3 de 3. Plugins, formato de projeto, persistência, mundo externo,
> performance, segurança, IA, roadmap, riscos, MVP, stack e complexidade.

---

## 1. Sistema de plugins

### 1.1 Tipos de plugin

| Tipo | O que adiciona | Carregamento |
|---|---|---|
| `device` | modelos de componentes | in-process (confiável) |
| `board` | placas e pinagens | in-process |
| `compiler` | backend de build | processo separado |
| `runtime` | executor de firmware | processo separado |
| `solver` | solver elétrico alternativo | in-process (confiável) |
| `exporter` | CSV, PNG, vídeo, netlist | in-process |
| `world` | adaptador de mundo externo | processo separado |
| `ui` | painel/inspetor React | sandbox no webview |

### 1.2 ABI C estável

Nada de STL, exceções ou tipos C++ cruzam a fronteira. Structs versionadas por
tamanho, handles opacos, allocator explícito e erros por código.

```c
/* simforge/plugin/abi.h */
#define SF_PLUGIN_ABI_V1 0x00010000u

typedef struct sf_error {
    uint32_t struct_size;
    uint32_t code;
    const char* message;   /* UTF-8, válido até a próxima chamada */
} sf_error;

typedef struct sf_plugin_info {
    uint32_t struct_size;
    uint32_t abi_version;      /* SF_PLUGIN_ABI_V1 */
    const char* plugin_id;     /* "acme.sensor.temp" */
    const char* name;
    const char* version;       /* semver */
    const char* vendor;
    uint32_t capabilities;     /* bitmask */
} sf_plugin_info;

typedef struct sf_plugin_host {
    uint32_t struct_size;
    void* host_ctx;
    void (*log)(void*, int level, const char* msg);
    void* (*alloc)(void*, size_t);
    void  (*free)(void*, void*);
} sf_plugin_host;

typedef struct sf_device_vtable {
    uint32_t struct_size;
    void* (*create)(const char* params_json, sf_error*);
    void  (*destroy)(void* instance);
    int   (*configure)(void*, const char* params_json, sf_error*);
    int   (*attach)(void* instance, void* host_ctx, sf_error*);
    int   (*on_event)(void* instance, const void* event, sf_error*);
    int   (*write_state)(void* instance, void* writer, sf_error*);
    int   (*read_state)(void* instance, void* reader, sf_error*);
} sf_device_vtable;

typedef struct sf_device_registration {
    uint32_t struct_size;
    const char* type_id;              /* "acme.sensor.temp" */
    uint32_t type_version;
    const char* descriptor_json;      /* pinos, parâmetros, símbolo */
    const sf_device_vtable* vtable;
} sf_device_registration;

typedef struct sf_registrar sf_registrar;   /* opaco, fornecido pelo host */

/* Exportadas pelo plugin. */
SF_EXPORT const sf_plugin_info* sf_plugin_query(void);
SF_EXPORT int sf_plugin_init(const sf_plugin_host*, sf_error*);
SF_EXPORT int sf_plugin_register(sf_registrar*,
                                 const sf_device_registration*,
                                 size_t count,
                                 sf_error*);
SF_EXPORT void sf_plugin_shutdown(void);
```

O host converte `descriptor_json` em `DeviceDescriptor`, valida pinos e
parâmetros e registra a fábrica. O plugin nunca recebe ponteiros para tipos C++
internos; `host_ctx` é opaco e só pode ser usado pelas funções do host.

Regras de ABI:

- Build com o mesmo compilador/ABI C; `extern "C"` e calling convention fixa.
- `struct_size` permite adicionar campos no fim sem quebrar.
- Strings UTF-8 com ownership definido; nunca devolver ponteiro para stack.
- Nenhuma exceção atravessa a fronteira; todo erro vira `sf_error`.
- Thread-safety declarada no manifesto: `main_only`, `sim_thread`,
  `any_thread`.
- Compatibilidade: host aceita `abi_major` igual; `minor` maior é compatível se
  as capabilities exigidas existirem.

### 1.3 Manifesto

```json
{
  "format": "simforge.plugin",
  "schema": 1,
  "id": "acme.sensor.temp",
  "name": "Sensores ACME",
  "version": "1.4.2",
  "abi": "1.0",
  "kind": ["device"],
  "entry": {
    "windows": "acme_sensors.dll",
    "linux": "libacme_sensors.so",
    "macos": "libacme_sensors.dylib",
    "wasm": "acme_sensors.wasm"
  },
  "capabilities": ["device.temperature", "device.humidity"],
  "permissions": ["fs.read:assets", "net.connect:api.acme.com"],
  "deterministic": true,
  "thread_model": "sim_thread",
  "signature": "ed25519:Ur2bCo0qHQnGrrfLDqoiGjHHPWQcWWuxxZ4h3bET+E4JqgliftSTQArv8cG4lTLFjBxL2KJYjJ4BbJDojH/7Gw==",
  "min_host": "0.4.0"
}
```

### 1.4 Registro e ciclo de vida

```
discover(paths)
   ├─ ler manifest.json
   ├─ validar schema, versão, ABI, assinatura
   ├─ checar trust tier e permissões
   ├─ carregar biblioteca
   ├─ sf_plugin_query → sf_plugin_init(host)
   ├─ sf_plugin_register(registry)
   └─ marcar capabilities disponíveis

create device/board
   └─ factory.create() → IDevice/IBoardBuilder

shutdown
   ├─ destruir instâncias
   ├─ sf_plugin_shutdown
   └─ unload (ou manter para hot reload)
```

### 1.5 Hot reload

```
watch(plugin.dll)
   │ arquivo mudou
   ▼
sim.pause(reason="plugin.reload")
   ├─ bloquear novas instâncias
   ├─ serializar estado dos dispositivos do plugin
   ├─ destruir instâncias
   ├─ copiar DLL para shadow path (evita lock no Windows)
   ├─ unload
   ├─ load nova versão
   ├─ registrar
   ├─ recriar instâncias e restaurar estado
   └─ sim.resume
```

Limitações:

- Só em modo desenvolvedor (`--dev`) ou com `plugin.hot_reload=true`.
- Se o estado serializado não for compatível com a nova versão, a sessão é
  reiniciada e o usuário é avisado.
- Plugins `process` (runtime/compiler/world) fazem reload matando e reiniciando
  o processo, não a DLL.

### 1.6 Trust tiers

| Tier | Origem | Execução | Permissões |
|---|---|---|---|
| `builtin` | núcleo | in-process | todas |
| `verified` | marketplace, assinado | in-process | declaradas + auditoria |
| `community` | usuário, assinado | in-process com guard | declaradas |
| `untrusted` | desconhecido | processo separado | mínimas, sem fs/rede |

- Assinatura Ed25519 sobre o hash do pacote.
- Lista de revogação e pin de versão.
- Plugin que crasha 2× é quarentenado (`plugin.quarantined`).

---

## 2. Formato `.sforge`

### 2.1 Princípios

- JSON canônico, UTF-8, schema inteiro.
- IDs ULID estáveis; nunca índices de array como referência.
- Topologia elétrica separada da geometria visual.
- Campos desconhecidos preservados no round-trip.
- Unidades explícitas; coordenadas inteiras em unidades de grade.

### 2.2 Exemplo

```json
{
  "format": "simforge.project",
  "schema": 1,
  "project": {
    "id": "01J9Z6K3V8Q2M4T7R1C5B0N8XA",
    "name": "LED e botão com ESP32-S3",
    "revision": 42,
    "created_at": "2026-11-14T12:00:00Z",
    "updated_at": "2026-11-14T13:10:00Z"
  },
  "units": {
    "length": "mm",
    "grid_mm": 1.27,
    "voltage": "V",
    "current": "A"
  },
  "boards": [
    {
      "id": "01J9Z6M0A1B2C3D4E5F6G7H8J9",
      "type": "simforge.esp32s3.devkitc",
      "type_version": 1,
      "position": { "x": 120, "y": 80 },
      "rotation": 0,
      "params": { "psram": true }
    }
  ],
  "devices": [
    {
      "id": "01J9Z6N7B2C3D4E5F6G7H8J9K0",
      "type": "simforge.led",
      "type_version": 1,
      "position": { "x": 200, "y": 60 },
      "rotation": 90,
      "params": { "color": "red", "vf": 1.9, "r_series": 330 },
      "pins": {
        "A": { "net": "N1" },
        "K": { "net": "GND" }
      }
    },
    {
      "id": "01J9Z6P4C3D4E5F6G7H8J9K0M1",
      "type": "simforge.button",
      "type_version": 1,
      "position": { "x": 60, "y": 140 },
      "rotation": 0,
      "params": { "pull": "up", "bounce_ms": 5 },
      "pins": {
        "1": { "net": "N2" },
        "2": { "net": "GND" }
      }
    }
  ],
  "nets": [
    {
      "id": "N1",
      "name": "LED_A",
      "kind": "digital",
      "pins": [
        { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "pin": "GPIO2" },
        { "device": "01J9Z6N7B2C3D4E5F6G7H8J9K0", "pin": "A" }
      ]
    },
    {
      "id": "N2",
      "name": "BTN",
      "kind": "digital",
      "pins": [
        { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "pin": "GPIO4" },
        { "device": "01J9Z6P4C3D4E5F6G7H8J9K0M1", "pin": "1" }
      ]
    },
    {
      "id": "GND",
      "name": "GND",
      "kind": "ground",
      "pins": [
        { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "pin": "GND" },
        { "device": "01J9Z6N7B2C3D4E5F6G7H8J9K0", "pin": "K" },
        { "device": "01J9Z6P4C3D4E5F6G7H8J9K0M1", "pin": "2" }
      ]
    }
  ],
  "wires": [
    {
      "id": "W1",
      "net": "N1",
      "points": [{ "x": 150, "y": 60 }, { "x": 200, "y": 60 }],
      "color": "#e11d48"
    }
  ],
  "firmware": {
    "entry": "src/main.cpp",
    "backend": "platformio",
    "framework": "arduino",
    "libraries": [],
    "artifact": "sha256:9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d",
    "build_id": "01J9Z7Q4W8E2R5T1Y3U6I9O0P2"
  },
  "simulation": {
    "solver": "auto",
    "seed": 12345,
    "slice_us": 1000,
    "analog_step_us": 10,
    "real_time": false
  },
  "layout": {
    "zoom": 1.0,
    "pan": { "x": 0, "y": 0 },
    "grid": true
  },
  "extensions": {}
}
```

### 2.3 Semântica

- `nets[].pins` é a verdade elétrica; `wires` é apenas geometria.
- Um pino pertence a no máximo uma net; o validador rejeita duplicidade.
- `kind`: `digital`, `analog`, `power`, `ground`, `bus`.
- `buses` referenciam nets; não substituem a topologia. O exemplo acima não usa
  barramentos; exemplos de I2C/SPI/UART estão em
  [`formatos/sforge-v1.md`](formatos/sforge-v1.md).
- `GND` é uma net `ground` e serve de referência comum a placa e dispositivos.
- `firmware.artifact` é um hash; o binário vive no cache de builds.
- `extensions` preserva dados de plugins/versões futuras.

### 2.4 Versionamento e migração

```
schema 1 ──► schema 2 ──► schema 3
   │            │            │
   └─ migrate1to2 ─ migrate2to3 ─► documento atual
```

- Leitor aceita `schema <= current` e aplica a cadeia de migração.
- `schema > current` é recusado com `project.schema_unsupported`.
- Mudanças aditivas não incrementam schema.
- Migrações são funções puras, testadas com golden files.
- Campos desconhecidos são preservados; se uma migração os tornar inválidos, o
  usuário recebe um relatório, não perda silenciosa.

### 2.5 Validação em níveis

| Nível | Verifica | Bloqueia? |
|---|---|---|
| Sintática | JSON, schema, tamanho, UTF-8 | sim |
| Referencial | IDs, nets, pinos, placas | sim |
| Elétrica (ERC) | curto, flutuante, tensão, corrente | aviso/erro configurável |
| De simulação | dispositivo suportado, runtime, bibliotecas | erro |
| De segurança | paths, assets, conteúdo executável | sim |

### 2.6 Bundle

`.sforgebundle` = ZIP com `project.sforge`, `assets/`, `firmware/` (opcional) e
`manifest.json` com hashes. Serve para troca, submissão e marketplace. O
formato de trabalho continua sendo `.sforge` + pasta de assets.

---

## 3. Persistência

### 3.1 Layout local

```
%LOCALAPPDATA%/SimForge/            (Windows)
~/.local/share/simforge/            (Linux)
~/Library/Application Support/SimForge/   (macOS)
├─ settings.json
├─ workspace.db                    (SQLite)
├─ assets/sha256/<hash>
├─ packages/<id>/<version>/
├─ builds/<hash>/
├─ recordings/
├─ logs/
└─ cache/
```

Projetos ficam onde o usuário escolher:

```
meu-projeto/
├─ projeto.sforge
├─ assets/            (imagens, símbolos, firmware)
├─ src/               (firmware)
├─ platformio.ini     (gerado)
└─ .simforge/         (build, cache local, journal)
```

### 3.2 SQLite

| Tabela | Conteúdo |
|---|---|
| `workspace` | projetos recentes, caminho, revisão, última abertura |
| `settings` | chaves/valores versionados |
| `catalog` | dispositivos/placas disponíveis (built-in + plugins) |
| `packages` | pacotes instalados, versão, hash, assinatura, trust |
| `builds` | hash de entrada, status, artefato, log, data |
| `recordings` | traces, duração, hash, projeto |
| `assets` | nome lógico → hash, tamanho, mime |
| `migrations` | `PRAGMA user_version` e histórico |

- WAL habilitado; uma conexão de escrita, leituras concorrentes.
- Migrações versionadas e testadas.
- Nunca armazenar o projeto no SQLite; ele é JSON portátil.

### 3.3 Assets

- Conteúdo endereçado por SHA-256.
- Deduplicação automática.
- Verificação de integridade ao abrir.
- Limite de tamanho e profundidade; sem symlinks; sem executáveis em assets
  (exceto firmware assinado no bundle).

### 3.4 Pacotes de componentes `.sfpkg`

```
acme-sensors-1.4.2.sfpkg (ZIP)
├─ manifest.json
├─ symbol.svg
├─ models/device.json
├─ bin/acme_sensors.dll
├─ LICENSE
└─ signature.ed25519
```

Instalação: validar assinatura, hash, ABI, permissões; copiar para
`packages/<id>/<version>`; registrar no catálogo; nunca sobrescrever versão em uso.

---

## 4. Integração com mundos externos

### 4.1 Contrato

```cpp
namespace simforge {

enum class EntityKind { Sensor, Actuator, Bidirectional };

struct ExternalEntity {
    std::string id;              // "semaforo.norte"
    EntityKind kind;
    std::string type;            // "traffic_light", "radar", "barrier"
    ValueSchema schema;          // tipos e unidades
    std::string description;
};

struct WorldInfo {
    std::string id;
    std::string name;
    std::string protocol_version;
    SimTime latency_budget;
};

struct ReadResult {
    Value value;
    SimTime valid_at;
    Quality quality;             // Good | Stale | Unavailable
};

class IExternalWorld {
public:
    virtual ~IExternalWorld() = default;

    virtual WorldInfo info() const = 0;
    virtual std::span<const ExternalEntity> entities() const = 0;

    // Barreira de tempo: o mundo externo deve estar em t ao retornar.
    virtual bool sync(SimTime t) = 0;

    virtual ReadResult read(std::string_view entity, SimTime t) = 0;
    virtual bool write(std::string_view entity, const Value&, SimTime t) = 0;

    using EventCallback = std::function<void(const ExternalEvent&)>;
    virtual Subscription subscribe(EventCallback) = 0;
};

} // namespace simforge
```

### 4.2 Protocolo `world.*`

```
SimForge                                    Mundo externo
   │  world.hello {protocol, session, t0}        │
   ├────────────────────────────────────────────►│
   │  world.caps {entities[], rates, units}      │
   │◄────────────────────────────────────────────┤
   │  world.sync {t}                             │
   ├────────────────────────────────────────────►│
   │  world.read {entity, t}                     │
   ├────────────────────────────────────────────►│
   │  world.read.result {value, valid_at, q}     │
   │◄────────────────────────────────────────────┤
   │  world.write {entity, value, effective_at}  │
   ├────────────────────────────────────────────►│
   │  world.event {entity, value, t}             │
   │◄────────────────────────────────────────────┤
   │  world.bye {reason}                         │
   └────────────────────────────────────────────►│
```

Regras:

- `sync(t)` é obrigatório em modo determinístico; o mundo deve responder com
  `synced` ou `timeout`.
- Leituras são *sample-and-hold* com timestamp; a qualidade indica staleness.
- Escritas têm `effective_at`; o mundo pode aplicar imediatamente ou agendar.
- Eventos externos entram na fila como `WorldInput` no timestamp informado.
- Timeout não bloqueia o sim thread: a leitura retorna `Unavailable` e o
  dispositivo aplica valor seguro configurado.
- Em replay, leituras e eventos vêm do trace; o mundo não é consultado.

### 4.3 Diagrama de integração

```
┌──────────────────────────────┐         ┌──────────────────────────────┐
│ SimForge                     │         │ Produto externo              │
│                              │         │ (ex.: PistaForge)            │
│ Firmware Arduino             │         │                              │
│   │ pinMode/digitalWrite     │         │  Cena / mundo virtual        │
│   ▼                          │         │   semáforo, cancela, radar   │
│ MCU (ESP32-S3)               │         │   sensor de passagem         │
│   │ GPIO/I2C/SPI             │         │                              │
│   ▼                          │         │                              │
│ Netlist + dispositivos       │         │                              │
│   │                          │         │                              │
│   ▼                          │         │                              │
│ IExternalWorld  ◄── world.* ─┼─────────┼─►  Adaptador (fora do        │
│ (sensores/atuadores externos)│  WS/pipe │    SimForge)                 │
└──────────────────────────────┘         └──────────────────────────────┘
```

O adaptador é responsável por traduzir `traffic_light` para o objeto da cena
externa, `radar` para velocidade, `barrier` para posição, etc. O SimForge só
conhece tipos genéricos e valores.

### 4.4 Exemplos de entidades

| Entidade | Tipo | Direção | Valores |
|---|---|---|---|
| Semáforo | `traffic_light` | atuador | `red`, `yellow`, `green`, `off` |
| Cancela | `barrier` | atuador + sensor | `open`, `closed`, `moving`; `position_mm` |
| Radar | `speed_sensor` | sensor | `speed_kmh`, `valid` |
| Sensor de proximidade | `proximity` | sensor | `distance_mm` |
| Detector de passagem | `passage_detector` | sensor/evento | `vehicle_id`, `direction`, `t` |
| Botão externo | `button` | sensor | `pressed` |

### 4.5 SimForge como componente de outro produto

Dois modos, ambos sem acoplamento:

1. **SimForge consome o mundo** (diagrama acima): o firmware controla objetos
   externos.
2. **SimForge é embarcado**: outro produto carrega `libsimforge` via C ABI ou
   conecta no servidor local, abre um `.sforge` e usa os `IExternalWorld` do
   projeto para expor os pinos como API.

```
PistaForge
   │  (processo ou libsimforge via C ABI)
   ▼
SimForge session (headless)
   ├─ circuit + firmware
   ├─ external world "pistaforge"
   │     ├─ sensor: posição do carro
   │     └─ atuador: semáforo da pista
   └─ API: sf_session_step/read/write
```

O PistaForge nunca inclui headers internos do SimForge; usa a API C pública ou o
protocolo. O SimForge nunca sabe que o outro lado é o PistaForge.

---

## 5. Performance

### 5.1 Orçamentos

| Métrica | MVP | v1 | Longo prazo |
|---|---|---|---|
| Comando de UI (p95) | 20 ms | 10 ms | 5 ms |
| Frame de UI | 16,6 ms | 16,6 ms | 8,3 ms |
| Resolução de net | 100 ns | 50 ns | 20 ns |
| Eventos digitais/s | 100k | 1M | 10M |
| Dispositivos | 200 | 5.000 | 50.000 |
| Placas | 1 | 4 | 32 |
| RAM por sessão | 100 MB | 500 MB | 1 GB |
| Tempo real do firmware | 1× | 1× | 2× |

### 5.2 ECS híbrido

```
EntityId ──► índice estável
   │
   ├─ DeviceComponent  (tipo, fábrica, estado polimórfico)
   ├─ PinComponent     (SoA: nível, força, proprietário, net)
   ├─ NetComponent     (SoA: estado, dirty, adjacência CSR)
   ├─ TransformComponent (posição, rotação — UI)
   └─ ProbeComponent   (instrumentação)
```

- Dados quentes (pinos, nets) em arrays contíguos.
- Modelos complexos (OLED, I2C) permanecem orientados a objeto, referenciados
  por índice.
- Sem `shared_ptr` no caminho quente; handles/índices e pools.
- Componentes frios (geometria, metadados) em storage separado.

### 5.3 Memória

- Arena por frame para eventos e snapshots.
- Pool de eventos por tamanho (32/64/128 bytes).
- String interning para nomes de pinos, nets e tipos.
- Buffers de trace em blocos; escrita assíncrona.
- Limite configurável por sessão; `sim.memory_limit` gera erro, não OOM.

### 5.4 Concorrência

- Sim thread única por ilha.
- Ilhas independentes em paralelo com merge determinístico.
- Compilação, runtime não confiável e plugins `untrusted` em processos.
- I/O de trace e logs em thread separada com fila limitada.
- UI recebe snapshots double-buffered; nunca shared mutable state.

### 5.5 Benchmarks

- `bench.net_resolve` — 1M resoluções.
- `bench.event_queue` — 10M push/pop.
- `bench.solver_logic` — circuito de 10k nets.
- `bench.solver_mna` — 1k nós, 10k passos.
- `bench.firmware_shim` — loop vazio, GPIO toggling, I2C.
- `bench.ui_frame` — 10k símbolos.
- `bench.replay` — trace de 60 s.
- Gate de regressão: ±10% em relação à baseline; CI falha acima disso.

### 5.6 Nuvem

```
Cliente (navegador)
   │  WSS + token
   ▼
API Gateway ──► Session Orchestrator ──► Worker (WASM isolate)
                    │                        ├─ libsimforge (wasm)
                    │                        ├─ runtime de firmware
                    │                        └─ trace store
                    └─ catálogo, assets, billing
```

- Cada sessão é um isolate com limites de CPU, memória e duração.
- O estado autoritativo é o trace + snapshot; o worker pode ser recriado.
- Assets e builds são content-addressed e compartilhados por hash.
- Sessões interativas usam o mesmo protocolo da versão desktop; não há API
  "de nuvem" separada.

---

## 6. Segurança

### 6.1 Modelo de ameaças

| Ameaça | Vetor | Mitigação |
|---|---|---|
| Firmware malicioso | loop infinito, memória, syscalls | WASM com fuel, memória limitada, sem WASI fs/net; processo separado |
| Plugin malicioso | DLL nativa | assinatura, trust tier, processo separado, job object/seccomp |
| Projeto malicioso | paths, zip bomb, JSON profundo | validação, limites, canonicalização, sem execução |
| Mundo externo hostil | dados inválidos, flood | schema, rate limit, timeout, valores seguros |
| API local | outro processo na máquina | token efêmero, loopback, origin check, CORS restrito |
| Supply chain | dependência comprometida | lockfile, SBOM, hashes, auditoria |
| IA | prompt injection, código inseguro | sandbox, validação, aprovação humana |

### 6.2 Sandbox de firmware

- WASM (wasmtime) com:
  - memória máxima (padrão 8 MB);
  - fuel/epoch por fatia;
  - imports explícitos (GPIO, Serial, Wire, SPI, tempo);
  - sem filesystem, rede ou threads por padrão;
  - timeout de execução e interrupção.
- Runtime nativo (`dll`) só em modo desenvolvedor, com aviso e job object.
- QEMU em processo separado com limites de CPU/memória e sem rede.

### 6.3 Isolamento de plugins

- In-process apenas para `builtin`/`verified`.
- `community`: in-process com guard + quarentena em crash.
- `untrusted`: processo separado com ponte de comandos e shared memory.
- Permissões declaradas e aplicadas (fs, net).
- Assinatura e revogação.

### 6.4 Validação e memória

- Fronteiras usam `std::span`, `std::string_view` e checks de tamanho.
- Sem `reinterpret_cast` de dados não confiáveis.
- ASan/UBSan no CI; fuzzing de JSON, protocolos e parsers de artefatos.
- Limites de profundidade de JSON, número de dispositivos, nets e assets.
- Escrita atômica e verificação de hash ao abrir projetos.

---

## 7. Camada de IA

### 7.1 Princípio

A IA **propõe**, o core **valida**, o usuário **aprova**. Nenhum modelo escreve
diretamente em arquivos ou executa comandos sem passar pelo pipeline.

### 7.2 Arquitetura

```
┌─────────────┐   prompt/tarefa   ┌──────────────────┐
│ UI (React)  │──────────────────►│ AI Orchestrator  │
└─────────────┘                   │ (fora do core)   │
                                  └───────┬──────────┘
                                          │ tools (API)
                    ┌─────────────────────┼─────────────────────┐
                    ▼                     ▼                     ▼
             ┌────────────┐        ┌────────────┐        ┌────────────┐
             │ Circuit    │        │ Firmware   │        │ Simulation │
             │ tools      │        │ tools      │        │ tools      │
             └─────┬──────┘        └─────┬──────┘        └─────┬──────┘
                   │                     │                     │
                   └──────────┬──────────┴──────────┬──────────┘
                              ▼                     ▼
                       ┌────────────┐        ┌────────────┐
                       │ Validator  │        │ Compiler + │
                       │ (core)     │        │ Runtime    │
                       └─────┬──────┘        └─────┬──────┘
                             │                     │
                             └──────────┬──────────┘
                                        ▼
                                 ┌────────────┐
                                 │ Patch +    │
                                 │ diff para  │
                                 │ aprovação  │
                                 └────────────┘
```

### 7.3 Capacidades

| Capacidade | Pipeline |
|---|---|
| Gerar circuito por prompt | catálogo → proposta de dispositivos/nets → ERC → diff |
| Gerar firmware Arduino | código → compilar → rodar testes → corrigir |
| Detectar erro de ligação | ERC/regras + explicação do modelo |
| Explicar falha | trace + estado + logs → hipótese + evidência |
| Simular cenários | gerar estímulos de mundo externo + asserções |

### 7.4 Ferramentas expostas ao modelo

- `catalog.search`, `catalog.describe`
- `circuit.addDevice`, `circuit.connect`, `circuit.remove`
- `circuit.validate`
- `firmware.write`, `firmware.build`, `firmware.flash`
- `sim.run`, `sim.step`, `sim.readProbe`, `sim.trace`
- `project.diff`, `project.applyPatch`

### 7.5 Segurança e avaliação

- Saída do modelo é sempre um patch estruturado, nunca shell.
- Limites de tokens, custo e tempo.
- Log de auditoria de prompts, ferramentas e patches.
- Dataset de avaliação com circuitos e falhas conhecidas; regressão por tarefa.
- Modo offline/local (ONNX/llama.cpp) para projetos sensíveis.
- Conteúdo de projeto tratado como dado não confiável (prompt injection).

---

## 8. Roadmap

### Fase 1 — Fundação e MVP (meses 1–3)

- Core: tempo, eventos, pinos, nets, `LogicSolver`.
- Dispositivos: LED, botão, resistor, fonte, GND.
- ESP32-S3 shim: GPIO, tempo, Serial.
- Editor React: canvas, biblioteca, fios, inspetor.
- `.sforge` schema 1, abrir/salvar.
- API WS + C ABI mínima.
- Serial monitor.
- **Saída:** piscar LED, ler botão, Serial, salvar/abrir, 60 fps.

### Fase 2 — Analógico e displays (4–6)

- ADC, PWM/LEDC, `analogRead/Write`.
- I2C framework + SSD1306 + LCD I2C.
- Instrumentos: tensão, corrente, waveform.
- Undo/redo e validação ERC.
- **Saída:** OLED I2C e LCD funcionando com bibliotecas Arduino.

### Fase 3 — Barramentos e sensores (7–9)

- SPI, UART completo, 1-Wire.
- NeoPixel (RMT ou modelo de custo).
- DHT22, servo, buzzer, sensores genéricos.
- Gravação/replay e trace.
- **Saída:** projeto com 5+ dispositivos, replay determinístico.

### Fase 4 — ESP-IDF e emulação (10–15)

- Backend ESP-IDF.
- QEMU ESP32-S3, FreeRTOS, timers, DMA, registradores.
- GDB stub e depuração.
- **Saída:** firmware ESP-IDF real executando no emulador.

### Fase 5 — Integração PistaForge (16–18)

- `IExternalWorld` completo.
- Protocolo `world.*` estável.
- Adaptador do PistaForge (fora do repo).
- **Saída:** semáforo controlado por firmware reagindo a eventos da pista.

### Fase 6 — Marketplace (19–24)

- `.sfpkg`, assinatura, catálogo, atualização.
- Curadoria, avaliações, licenças.
- **Saída:** instalação de componente sem recompilar o core.

### Fase 7 — IA (25+)

- Geração de circuito/firmware, diagnóstico e cenários.
- Avaliação contínua e guardrails.
- **Saída:** assistente que propõe, valida e explica.

---

## 9. Riscos técnicos

| Risco | Prob. | Impacto | Mitigação |
|---|---|---|---|
| Compatibilidade Arduino limitada | Alta | Alto | escopo declarado, relatório de compatibilidade, shim incremental |
| Timing de bit-bang impreciso | Alta | Médio | modelo de custo calibrado, RMT, componentes transacionais |
| Convergência do solver MNA | Média | Alto | gmin/source stepping, passo adaptativo, fallback lógico |
| NeoPixel/WS2812 | Média | Médio | RMT, decodificador de pulsos, testes de timing |
| Ferramenta WASM/toolchain | Média | Alto | native shim como fallback, CI com wasm32-wasi |
| QEMU (licença/manutenção) | Média | Alto | processo separado, versão fixada, plano B ISS |
| Performance em projetos grandes | Média | Alto | ECS, pools, ilhas, benchmarks desde o dia 1 |
| Segurança de plugins | Média | Alto | trust tiers, processo separado, assinatura |
| Bibliotecas que usam registradores | Alta | Médio | mensagem clara, migração para emulação |
| Determinismo entre plataformas | Média | Médio | modo estrito, replay, hash de trace |
| Marketplace sem confiança | Média | Médio | curadoria, assinatura, sandbox |
| Escopo e prazo | Alta | Alto | fases com critérios de saída, MVP enxuto |
| IA gerar código inseguro | Alta | Médio | validação, sandbox, aprovação humana |

---

## 10. MVP de 90 dias

**Equipe assumida:** 1 engenheiro de core, 1 de frontend, 1 de runtime/compilador,
QA parcial. Se houver menos pessoas, priorizar core + editor e adiar I2C.

| Semana | Entrega |
|---|---|
| 1 | Repositório, CMake, CI, esqueleto da API, app desktop, React/Vite |
| 2 | Tempo, event queue, scheduler, testes de determinismo |
| 3 | Pin/net, `LogicSolver`, LED/resistor/fonte, ERC básico |
| 4 | Editor: canvas, grade, biblioteca, arrastar, fios, snap |
| 5 | `.sforge` schema 1, salvar/abrir, validação, IDs |
| 6 | ESP32-S3 board, shim GPIO/tempo, build pipeline |
| 7 | Runtime WASM, `pinMode/digitalWrite/digitalRead`, watchdog |
| 8 | Serial monitor, `Serial.print`, eventos e frames |
| 9 | Botão, pull-up, bounce, interrupções, exemplos |
| 10 | PWM/LEDC + LED dimerizável; instrumentos básicos |
| 11 | Hardening: ASan, fuzz, limites, logs, erros; testes E2E |
| 12 | Empacotamento, instalador, documentação, demo, alpha |

**Critérios de saída:**

- Blink, botão-LED e Serial funcionando com código Arduino real.
- Projeto salva, reabre e mantém topologia e layout.
- 200 dispositivos a 60 fps.
- Determinismo: mesmo projeto + seed → mesmo hash de trace.
- Nenhum crash em 1 h de fuzzing de projeto e protocolo.
- Build Windows + Linux; wasm no navegador (demo).

---

## 11. Tecnologias e bibliotecas

| Área | Escolha | Alternativas | Observação |
|---|---|---|---|
| Linguagem core | C++20 | C++23 | requisito |
| Build | CMake 3.28+ + Ninja | Meson | presets Debug/Release/ASan |
| Pacotes C++ | vcpkg (manifest) | Conan | lockfile e SBOM |
| Compiladores | MSVC, Clang, GCC | — | CI nos três |
| JSON | nlohmann/json + simdjson (leitura) | RapidJSON | schema com valijson |
| Logs | spdlog | — | async no host |
| Testes | doctest ou Catch2 v3 | GoogleTest | rápido, header-only |
| Bench | Google Benchmark | nanobench | baseline no CI |
| Profiling | Tracy | Superluminal | zonas no engine |
| Alocador | mimalloc | jemalloc | opcional por host |
| Banco | SQLite (WAL) | — | embutido |
| Rede | Boost.Asio/Beast ou uWebSockets | IXWebSocket | WS local |
| WASM | wasmtime C API | wasmer | fuel + memória |
| Compiladores firmware | PlatformIO Core + arduino-cli | — | subprocesso |
| Cripto | libsodium/OpenSSL | — | Ed25519, SHA-256 |
| Desktop shell | WebView2 / WKWebView / WebKitGTK | Tauri/Electron | sem runtime JS extra |
| Frontend | React 19 + TS 5 + Vite + Tailwind | Svelte | requisito |
| Estado | Zustand | Redux Toolkit | projeção do core |
| Render | PixiJS/WebGL | Canvas 2D | milhares de símbolos |
| Editor código | Monaco | CodeMirror 6 | firmware |
| Serial | xterm.js | — | ANSI |
| Gráficos | uPlot | ECharts | waveforms |
| i18n | i18next | — | PT/EN |

Licenças: preferir MIT/BSD/Apache; QEMU é GPL (processo separado, oferta de
fonte); verificar licenças de bibliotecas Arduino no build.

---

## 12. Complexidade estimada

Pessoa-mês (PM), confiança média; inclui testes e documentação.

| Módulo | PM | Risco |
|---|---|---|
| Core: tempo/eventos/scheduler | 3–4 | baixo |
| Pin/net/`LogicSolver` | 3–4 | médio |
| `MnaSolver` | 5–8 | alto |
| ECS/pools/performance | 3–4 | médio |
| Device framework + registry | 2 | baixo |
| Dispositivos básicos (LED, botão, R, fonte) | 1 | baixo |
| SSD1306 + LCD I2C | 2–3 | médio |
| Servo, DHT22, buzzer, sensores | 2–3 | médio |
| NeoPixel/WS2812 | 2–3 | alto |
| Board framework + ESP32-S3 | 3–4 | médio |
| Shim Arduino (SACL) | 4–6 | alto |
| Runtime WASM + sandbox | 2–3 | médio |
| Compiler integration (PlatformIO) | 3–4 | médio |
| ESP-IDF + QEMU | 8–12 | muito alto |
| Plugins/ABI/hot reload | 3–4 | médio |
| `.sforge` + migrações | 1–2 | baixo |
| Persistência/SQLite/assets | 2 | baixo |
| API WS + C ABI + schemas | 3 | médio |
| Frontend editor | 6–8 | médio |
| Instrumentos/Serial/Code editor | 3–4 | baixo |
| `IExternalWorld` + protocolo | 2–3 | médio |
| Segurança/sandbox | 2–3 | médio |
| IA | 3–6 | alto |
| Infra/CI/release | 3–4 | baixo |
| **MVP (fases 1–2)** | **~14–20** | |
| **v1 (fases 1–3)** | **~28–38** | |
| **v2 (até fase 5)** | **~50–70** | |

---

## 13. ADRs recomendados

| ADR | Decisão |
|---|---|
| 0001 | `libsimforge` em C++20, sem UI, com API C estável |
| 0002 | Independência total do PistaForge; integração só por protocolo/formato |
| 0003 | Tempo virtual em nanossegundos inteiros |
| 0004 | Fila de eventos determinística com fases e delta cycles |
| 0005 | `LogicSolver` no MVP; `MnaSolver` como evolução |
| 0006 | Firmware em WASM como sandbox padrão |
| 0007 | ABI C versionada e manifesto para plugins |
| 0008 | `.sforge` JSON com schema inteiro e migrações |
| 0009 | `IExternalWorld` como fronteira genérica de integração |
| 0010 | Frontend React sem lógica de simulação |
| 0011 | Determinismo estrito: sem wall clock, unordered, fast-math |
| 0012 | Trust tiers e assinatura de plugins |
| 0013 | Compilação em subprocesso isolado com cache por hash |
| 0014 | API WebSocket + C ABI sobre o mesmo command bus |
| 0015 | Limites de recursos e watchdog para firmware |
| 0016 | Geometria em unidades de grade inteiras; elétrica em SI |
| 0017 | Preservação de campos desconhecidos no round-trip |
| 0018 | QEMU em processo separado na fase de emulação |

---

## 14. Próximos passos

1. Revisar e aceitar os ADRs 0001–0010 antes de escrever código de produção.
2. Spikes de 2 semanas:
   - scheduler + net resolution com 10k nets;
   - compilar um sketch Arduino para WASM e piscar um LED virtual;
   - editor React com 5k símbolos a 60 fps;
   - protocolo `world.*` com um semáforo fake.
3. Congelar o schema `.sforge` v1 e os contratos da API.
4. Montar o CI (Windows/Linux, Debug/Release/ASan, determinismo, benchmarks).
5. Iniciar a fase 1 com critérios de saída acordados.
