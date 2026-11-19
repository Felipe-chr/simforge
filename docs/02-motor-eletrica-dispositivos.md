# SimForge — Motor, Modelo Elétrico, Dispositivos e Placas

> Documento 2 de 3. Detalha o coração técnico do SimForge: tempo, eventos,
> elétrica, frameworks, ESP32-S3 e compilação.

---

## 1. Modelo temporal

### 1.1 Representação

```cpp
namespace simforge {

// Tempo virtual em nanossegundos inteiros.
using SimTime = std::int64_t;

inline constexpr SimTime kNever = std::numeric_limits<SimTime>::max();
inline constexpr SimTime kNsPerUs = 1'000;
inline constexpr SimTime kNsPerMs = 1'000'000;
inline constexpr SimTime kNsPerS  = 1'000'000'000;

struct TimeRange {
    SimTime begin;
    SimTime end;
};

} // namespace simforge
```

Inteiro de 64 bits elimina acumulação de erro e torna a ordenação exata. Toda
conversão para `double` acontece apenas em modelos físicos e instrumentação.

### 1.2 Relógios

| Relógio | Dono | Uso |
|---|---|---|
| `VirtualClock` | engine | tempo autoritativo da simulação |
| `GuestClock` | runtime | visão do firmware (`millis`, `micros`) |
| `WallClock` | host | throttle de tempo real, timeouts, UI |
| `WorldClock` | integração | sincronização com mundos externos |

O `GuestClock` é derivado do `VirtualClock`; nunca o contrário. O `WallClock`
nunca entra em decisões de simulação.

### 1.3 Modos de execução

| Modo | Descrição | Uso |
|---|---|---|
| `Deterministic` | sem wall clock, single-thread por ilha, RNG semeado | testes, replay, CI |
| `RealTime` | throttle para 1× (ou N×) | uso interativo |
| `FastForward` | o mais rápido possível, sem throttle | análise e batch |
| `Step` | avança N eventos ou N ns por comando | depuração |
| `Replay` | entradas gravadas, sem runtime novo | regressão |

O modo é uma propriedade da sessão, não do projeto.

---

## 2. Fila de eventos e scheduler

### 2.1 Tipos de evento

```cpp
enum class EventKind : std::uint16_t {
    Command,          // comando da API aplicado no engine
    PinDrive,         // dispositivo mudou o driver de um pino
    NetResolved,      // net estabilizou
    BusTransfer,      // transação de barramento
    TimerExpired,     // timer de dispositivo/placa
    AnalogStep,       // passo máximo do solver analógico
    RuntimeWake,      // firmware pode executar
    RuntimeYield,     // firmware devolveu o controle
    WorldInput,       // evento de mundo externo
    InstrumentSample, // probe de instrumentação
    TraceCheckpoint,  // snapshot para replay
};
```

### 2.2 Chave de ordenação

```
key = (time_ns, phase, seq)
```

- `time_ns`: instante do evento.
- `phase`: ordena efeitos dentro do mesmo instante.
- `seq`: contador monotônico global; desempate estável e determinístico.

Fases:

```
PreInput → Input → Evaluate → Settle → Commit → PostCommit
```

Eventos gerados durante uma fase são inseridos com a fase apropriada; eventos
`PostCommit` só rodam depois que todas as nets estabilizaram.

### 2.3 Estrutura

```
┌────────────────────────────────────────────────────────────┐
│ EventQueue                                                 │
│  ├─ near_heap   : min-heap para t < now + 10 ms            │
│  ├─ wheel       : timing wheel de 1024 buckets (10 ms)     │
│  └─ far_heap    : heap para t >= now + ~10 s               │
└────────────────────────────────────────────────────────────┘
```

- `near_heap` cobre a maioria dos eventos (timers de firmware, barramentos).
- `wheel` reduz custo de timers de milissegundos.
- `far_heap` evita varredura de buckets distantes (timeouts longos).
- Eventos são alocados em pool por tamanho; payloads grandes usam arena.

### 2.4 Loop principal

```
while (running) {
    t = queue.peekTime()
    if (t > stopTime) break

    // 1. Todos os eventos do mesmo instante, em ordem de fase/seq
    while (queue.peekTime() == t) {
        e = queue.pop()
        dispatch(e)
    }

    // 2. Delta cycles: efeitos combinacionais no mesmo instante
    for (delta = 0; delta < kMaxDelta; ++delta) {
        if (!dirtyNets.empty()) { resolveDirtyNets(); continue }
        if (!pendingZeroDelay.empty()) { dispatchZeroDelay(); continue }
        break
    }
    if (delta == kMaxDelta) report(Oscillation, t)

    // 3. Analógico: converge até o próximo passo máximo
    if (analogDirty) solver.solveUntil(min(t + analogStep, nextEventTime))

    // 4. Commit: aplica estados, publica frame, grava trace
    commit(t)

    // 5. Runtime: executa até o próximo wake ou orçamento
    if (runtimeWantsCpu && t >= runtime.nextWake)
        runtime.runUntil(min(t + slice, nextEventTime))

    clock.set(t)
}
```

### 2.5 Delta cycles

Delta cycle é a resolução de efeitos sem avanço de tempo. Ele existe porque um
pino pode mudar, alterar uma net, notificar um dispositivo, que muda outro pino,
tudo no mesmo instante.

```
t = 1000 ns
  ├─ GPIO2 sobe
  ├─ net (GPIO2, R1) recalcula
  ├─ LED recebe NetResolved e agenda atualização de brilho
  ├─ sensor óptico lê o novo estado
  └─ net estabiliza → commit
```

Limite padrão: 64 iterações. Se o estado se repetir ciclicamente, o engine
detecta oscilação (hash do conjunto de nets) e emite `sim.oscillation` com os
nets envolvidos. O usuário vê um erro de projeto (realimentação combinacional),
não um travamento.

### 2.6 Avanço de tempo

```
next = min(
    queue.nextTime(),
    runtime.nextWake(),
    analog.nextStep(),
    world.nextDeadline()
)
```

O tempo só avança para um instante com trabalho pendente. Não existe "tick fixo"
global; o passo de 60 Hz é apenas a taxa de publicação de frames para a UI.

### 2.7 Determinismo

Checklist obrigatório no caminho de simulação:

- Nenhuma chamada a `system_clock`, `steady_clock` ou `rand()`.
- Nenhuma iteração sobre `unordered_*` que afete ordem de eventos.
- Nenhum `std::thread` dentro de uma ilha.
- Nenhum `fast-math`, FMA implícito ou `long double`.
- RNG por dispositivo, semeado por `(project_seed, device_id, purpose)`.
- Ordenação de nets e dispositivos por id estável, nunca por endereço.
- Floats formatados de forma canônica no trace (hex ou `max_digits10`).
- `EIGEN_DONT_PARALLELIZE` e equivalentes em bibliotecas numéricas.

O trace grava `engine_version`, `project_hash`, `seed`, `runtime_artifact_hash` e
um hash incremental de eventos. Duas execuções determinísticas devem produzir o
mesmo hash; o CI falha se não produzir.

### 2.8 Sincronização com o runtime

```cpp
enum class RuntimeStatus {
    Yielded,        // devolveu o controle voluntariamente
    Sleeping,       // dormindo até nextWake
    Halted,         // setup/loop terminou
    Crashed,        // sandbox falhou
    BudgetExceeded, // watchdog
};

struct RuntimeSlice {
    SimTime deadline;
    std::uint64_t instruction_budget;
    std::uint32_t event_budget;
};

class IRuntime {
public:
    virtual ~IRuntime() = default;
    virtual RuntimeStatus load(const FirmwareArtifact&) = 0;
    virtual RuntimeStatus runUntil(const RuntimeSlice&) = 0;
    virtual RuntimeStatus reset() = 0;
    virtual SimTime nextWake() const = 0;
    virtual void setPin(GpioId, PinState) = 0;
    virtual PinState getPin(GpioId) = 0;
    virtual void onBusEvent(const BusEvent&) = 0;
    virtual RuntimeStats stats() const = 0;
};

```

Regras:

1. O engine chama `runUntil` somente quando `now >= nextWake`.
2. O runtime não pode avançar o tempo sozinho; `delay` agenda `nextWake`.
3. Leituras de pino dentro da fatia usam o estado latched do último commit
   (sample-and-hold), salvo instrumentação explícita de interpolação.
4. Interrupções são enfileiradas e executadas no início da próxima fatia, com
   latência configurável (padrão 2 µs).
5. Um runtime que não devolve o controle dentro do orçamento é interrompido
   (`BudgetExceeded`), com relatório de hang.

---

## 3. Modelo elétrico

### 3.1 Abstrações

```
┌──────────┐   1..n   ┌──────────┐   n..m   ┌──────────┐
│  Device  │─────────►│   Pin    │─────────►│   Net    │
└──────────┘          └──────────┘          └────┬─────┘
      │                                        │
      │ pertence a                             │ pertence a
┌─────▼─────┐                            ┌─────▼─────┐
│  Board    │                            │   Bus     │
└───────────┘                            └───────────┘
```

| Conceito | Definição | Identidade |
|---|---|---|
| **Pin** | Terminal elétrico de um dispositivo, com características e direção | `(device_id, pin_index)` |
| **Net** | Conjunto de pinos no mesmo potencial | `NetId` estável |
| **Device** | Modelo comportamental/elétrico de um componente | `DeviceId` (ULID) |
| **Board** | Composição de MCU + pinagem + periféricos + alimentação | `BoardId` |
| **Bus** | Protocolo sobre um conjunto de nets (I2C, SPI, UART, 1-Wire) | `BusId` |

### 3.2 Estado de pino

```cpp
enum class Level : std::uint8_t { Low, High, HighZ, Unknown };
enum class Strength : std::uint8_t { Strong, Pull, Weak };

struct PinState {
    Level level{Level::HighZ};
    Strength strength{Strength::Weak};
    double analog_v{0.0};   // válido quando o solver analógico está ativo
};

struct PinElectrical {
    double v_high{3.3};
    double v_low{0.0};
    double r_out{25.0};        // ohms
    double r_in{1e6};          // ohms
    double c_in{5e-12};        // farads
    double leakage_a{1e-9};
    bool open_drain{false};
    bool pull_up{false};
    bool pull_down{false};
    double pull_r{45000.0};    // ohms (interno típico ESP32)
    bool input_only{false};
    bool five_volt_tolerant{false};
};

struct PinDescriptor {
    std::string_view name;
    PinElectrical electrical;
    PinCapabilities caps;      // ADC, PWM, I2C, SPI, strapping...
};
```

### 3.3 Resolução de net

A net agrega os drivers de todos os pinos e resolve para um estado único.

```
resolve(net):
  strong_high = any(pin.level==High && pin.strength==Strong)
  strong_low  = any(pin.level==Low  && pin.strength==Strong)

  if strong_high && strong_low:
      return { Unknown, Strong, conflict=true }   // curto / contenção
  if strong_high: return { High, Strong }
  if strong_low:  return { Low,  Strong }

  pulls_high = any(pull-up ativo ou driver Pull High)
  pulls_low  = any(pull-down ativo ou driver Pull Low)

  if pulls_high && pulls_low: return { Unknown, Pull }
  if pulls_high: return { High, Pull }
  if pulls_low:  return { Low,  Pull }

  if any Weak High && any Weak Low: return { Unknown, Weak }
  if any Weak High: return { High, Weak }
  if any Weak Low:  return { Low,  Weak }

  return { HighZ, Weak }   // flutuante
```

Consequências:

- Dois push-pull opostos geram **contenção**: `Unknown`, erro ERC `E_SHORT`,
  corrente estimada e destaque visual na UI.
- Pull-up vence floating; strong vence pull.
- Net flutuante é `HighZ`; `digitalRead` devolve o último valor latched com
  política configurável (`last`, `low`, `high`, `noisy`) e emite warning
  `W_FLOATING_PIN`.
- Open-drain só puxa para baixo; o alto vem do pull-up. É assim que I2C
  funciona.

### 3.4 Propagação

```
digitalWrite(GPIO2, HIGH)
        │
        ▼
[Device GPIO] agenda PinDrive(t, GPIO2, High, Strong)
        │
        ▼
[Engine] marca net N7 dirty
        │
        ▼
[NetResolver] N7: GPIO2=High(strong), R1=HighZ → High(strong)
        │
        ├─► [LED] onNetResolved(N7) → calcula corrente/brilho
        ├─► [Probe] registra transição
        └─► [Runtime] latch para digitalRead
        │
        ▼
commit: UI recebe evento + frame
```

Em net com vários dispositivos, a resolução ocorre uma vez por delta cycle e
todos os listeners recebem o mesmo `NetState`. Não há ordem de dispositivo
afetando o resultado.

### 3.5 Exemplo: GPIO2 → LED

```
ESP32-S3                                         Hardware
┌──────────┐        ┌─────────┐          ┌──────────────┐
│ GPIO2    ├───N1───┤ R1 330Ω ├────N2────┤ LED A        │
│ (push-   │        └─────────┘          │        K ────┼── GND
│  pull)   │                             └──────────────┘
└──────────┘
```

1. Firmware chama `digitalWrite(2, HIGH)`.
2. GPIO2 dirige N1 a 3,3 V (forte).
3. N1 inclui R1; o solver calcula a corrente:
   `I = (3,3 − Vf) / (R1 + Rds_on)`.
4. N2 fica em `Vf + I·R_dynamic` (modelo de diodo) ou, no modo lógico,
   `High` com corrente nominal.
5. O LED converte corrente em brilho (`luminance = f(I)`), com curva
   configurável e gamma para a UI.
6. Com PWM, o brilho é a média temporal da corrente; o solver integra no tempo
   ou usa duty diretamente no modo funcional.

No `LogicSolver` (MVP), o LED não calcula corrente real: usa a tabela
`(net level, PWM duty) → brilho`. No `MnaSolver`, o mesmo dispositivo usa o
modelo de diodo e o resultado é fisicamente consistente.

### 3.6 Barramentos compartilhados

| Barramento | Nets | Elétrica | Modelo |
|---|---|---|---|
| I2C | SDA, SCL | open-drain + pull-ups, wired-AND | controller/target, endereço, ACK, clock stretch |
| SPI | SCLK, MOSI, MISO, CS | push-pull, CS ativo | modos 0–3, transferência full-duplex |
| UART | TX, RX | push-pull idle high | framing, baud, FIFO |
| 1-Wire | DQ | open-drain + pull-up | reset/presence, slots de leitura/escrita |
| PWM | 1 net | push-pull | frequência, duty, dead time |
| WS2812 | 1 net | push-pull | trem de pulsos 800 kHz |

I2C como exemplo:

```
        VCC
         │
        ┌┴┐ pull-up 4.7k
        └┬┘
   SDA ──┼──────────┬──────────┬─────────
         │          │          │
      ┌──┴──┐    ┌──┴──┐    ┌──┴──┐
      │ MCU │    │OLED │    │Sensor│
      └──┬──┘    └──┬──┘    └──┬──┘
   SCL ──┴──────────┴──────────┴─────────
         │
        open-drain em todos os participantes
```

- Todos os pinos são open-drain; o nível alto vem do pull-up.
- Dois participantes puxando baixo ao mesmo tempo é normal (wired-AND), não é
  curto.
- O target pode segurar SCL em baixo (clock stretching); o controller modela o
  tempo e o firmware vê a transferência mais longa.
- Endereço inexistente gera NACK e erro de transação, não travamento.

### 3.7 Alimentação e domínios

- Nets especiais `VCC`, `GND`, `VBUS`, `3V3`, `5V` com `PowerNet`.
- `PowerTree` da placa: USB → regulador → rails; corrente agregada e queda.
- Domínios de tensão; level shifter quando necessário; aviso de 5 V em pino
  não tolerante.
- GND comum entre placas; sem GND comum, integração elétrica é inválida
  (`E_NO_COMMON_GROUND`).

---

## 4. Solvers

### 4.1 `LogicSolver` (MVP)

- Resolve nets por níveis e forças (tabela da seção 3.3).
- Dispositivos expõem `drivers()` e `onNetResolved()`.
- Corrente não é calculada; LEDs, buzzers e motores usam funções
  comportamentais calibradas.
- Custo por evento: dezenas de nanossegundos.
- Cobre 80% dos projetos de ensino e prototipagem.

### 4.2 `MnaSolver` (evolução)

- Análise nodal modificada (MNA): incógnitas = tensões de nó + correntes de
  fontes de tensão.
- Elementos: R, C, L, diodo, BJT (Ebers-Moll), MOSFET (Shichman-Hodges),
  chaves, fontes.
- Integração no tempo: Euler regressivo (padrão), trapezoidal opcional.
- Newton-Raphson com damping, `gmin` stepping e source stepping.
- Passo máximo padrão: 10 µs; adaptativo até 100 ns perto de chaveamento.
- LU esparsa com fatoração simbólica reutilizada; ordem fixa para determinismo.
- Falha de convergência: reduz passo, tenta de novo; se persistir, emite
  `sim.analog_diverged` com nós problemáticos e mantém o último estado válido.

### 4.3 Híbrido

```
┌─────────────────────────────────────────────────────────┐
│ Circuito                                                │
│  ┌───────────────┐   interface    ┌───────────────────┐ │
│  │ Ilha digital  │◄──Thevenin────►│ Ilha analógica    │ │
│  │ LogicSolver   │                │ MnaSolver         │ │
│  └───────────────┘                └───────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

- Nets digitais puras nunca entram na matriz.
- A fronteira é convertida em equivalente de Thévenin (V, R) e atualizada
  apenas quando muda.
- Partição calculada na compilação do circuito e revalidada a cada edição
  estrutural.

### 4.4 Escolha por projeto

`simulation.solver` no `.sforge`:

- `"logic"`: rápido, determinístico, sem corrente.
- `"mna"`: físico, mais lento.
- `"auto"`: lógico até detectar elemento analógico; então promove a ilha.

---

## 5. Framework de dispositivos

### 5.1 Ciclo de vida

```
create → configure(params) → attach(context) → reset
   │                                              │
   │                              ┌───────────────┘
   ▼                              ▼
 onEvent/onNetResolved ──► atualiza estado ──► agenda efeitos
   │
   ├─ serialize(state) / deserialize(state)   (snapshot/replay)
   └─ destroy
```

### 5.2 Interfaces principais

```cpp
namespace simforge {

struct DeviceDescriptor {
    std::string type_id;          // "simforge.led"
    std::uint32_t type_version;   // 1
    std::string display_name;
    std::vector<PinDescriptor> pins;
    std::span<const ParamDescriptor> params;
    SymbolRef symbol;             // SVG/ID referenciado pelo frontend
    std::string category;
    FidelityLevel max_fidelity;   // Functional | Electrical | Timing
};

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual const DeviceDescriptor& descriptor() const = 0;
    virtual void configure(const ParamSet&) = 0;
    virtual void attach(IDeviceContext&) = 0;
    virtual void reset() = 0;

    virtual void onEvent(const DeviceEvent&) = 0;
    virtual void onNetResolved(const NetState&) {}

    virtual void writeState(IStateWriter&) const {}
    virtual void readState(IStateReader&) {}
};

class IDeviceContext {
public:
    virtual SimTime now() const = 0;
    virtual void schedule(SimTime when, EventKind, EventPayload) = 0;
    virtual PinState readPin(PinIndex) const = 0;
    virtual void drivePin(PinIndex, PinState) = 0;
    virtual void emitDisplayUpdate(const DisplayUpdate&) = 0;
    virtual void emitAudio(const AudioUpdate&) = 0;
    virtual IRandom& rng() = 0;
    virtual IProbeSink& probes() = 0;
};

class IBusParticipant {
public:
    virtual ~IBusParticipant() = default;
    virtual void onBusEvent(const BusEvent&) = 0;
};

class IAnalogElement {
public:
    virtual ~IAnalogElement() = default;
    // Stamps contribuições na matriz MNA.
    virtual void stamp(MnaSystem&, const AnalogSolution&) = 0;
    virtual bool converged(const AnalogSolution&) const = 0;
};

class IDeviceFactory {
public:
    virtual ~IDeviceFactory() = default;
    virtual std::string_view type_id() const = 0;
    virtual std::uint32_t type_version() const = 0;
    virtual std::unique_ptr<IDevice> create() = 0;
    virtual const DeviceDescriptor& descriptor() const = 0;
};

} // namespace simforge
```

### 5.3 Registro

```cpp
class DeviceRegistry {
public:
    void add(std::unique_ptr<IDeviceFactory>);
    IDeviceFactory* find(std::string_view type_id, std::uint32_t version);
    std::span<const DeviceDescriptor> all() const;
    void freeze();   // após descoberta de plugins; instâncias usam snapshot
};
```

- Built-ins registrados na inicialização.
- Plugins registram fábricas via ABI C.
- `type_id` é namespace reverso (`simforge.led`, `acme.sensor.temp`).
- Versão permite múltiplas implementações do mesmo componente.
- Built-ins usam exatamente a mesma interface de fábrica; alguns são compilados
  também como plugin para validar a ABI continuamente.

### 5.4 Exemplos de modelos

| Componente | Pinos | Estado | Comportamento | Fidelidade |
|---|---|---|---|---|
| **LED** | A, K | brilho, corrente | `f(V, I)`, gamma, PWM médio | funcional/elétrica |
| **Push button** | 1, 2 | pressionado, bounce | abre/fecha, debounce determinístico | funcional |
| **Clock/PWM** | OUT | nível, duty | bordas agendadas pelo motor (`next_edge`), sem passo do host | timing |
| **SSD1306** | VCC, GND, SDA, SCL | GDDRAM 128×64, controlador | target I2C, comandos, dirty rects | protocolo |
| **LCD I2C** | VCC, GND, SDA, SCL | PCF8574 + HD44780 | nibbles, E pulse, busy flag | protocolo/timing |
| **Servo** | V+, GND, PWM | ângulo, velocidade | pulso 500–2500 µs → ângulo | timing |
| **DHT22** | VCC, GND, DATA | temp, umid, checksum | 1-Wire 40 bits, timing µs | timing |
| **Buzzer** | +, − | ativo, frequência | tone/PWM → som | funcional |
| **NeoPixel** | VCC, GND, DIN | cadeia RGB | trem WS2812, reset | timing crítico |
| **Sensor genérico** | VCC, GND, OUT | valor, ruído | função de transferência | funcional |

#### LED

```cpp
class LedDevice final : public IDevice {
    // params: color, forward_voltage, series_resistance, max_current
    // funcional: brightness = duty * intensity
    // elétrico:  I = (V_net - Vf) / (R_series + R_dynamic)
};
```

#### SSD1306

Máquina de estados resumida:

```
IDLE ──start──► ADDRESS ──ack──► CONTROL ──0x00──► COMMAND
                                     └──0x40──► DATA
COMMAND: 0x00–0x7F → command set; 0x80–0xFF → set contrast
DATA:    escreve em GDDRAM no ponteiro (page, column), auto-incremento
STOP:    latch de dirty rect → emitDisplayUpdate
```

O frontend recebe apenas retângulos sujos (`{x, y, w, h, pixels}`) e desenha no
canvas; o modelo nunca envia um framebuffer inteiro a 60 Hz.

#### NeoPixel (WS2812)

- Protocolo: `T0H ≈ 0.4 µs`, `T1H ≈ 0.8 µs`, período ≈ 1.25 µs, reset > 50 µs.
- Estratégias:
  1. **RMT/periférico**: o firmware usa RMT; o runtime captura o trem de pulsos
     e decodifica. Preferível.
  2. **Bit-bang com modelo de custo**: cada `digitalWrite` avança o tempo virtual
     em `api_cost_ns`; calibração permite gerar timing plausível. Documentar que
     é aproximado.
  3. **Rejeição explícita**: se o timing não puder ser garantido, marcar o
     componente como "requer emulação completa" e falhar com mensagem clara.
- O modelo mantém a cadeia, aplica GRB, latch no reset e atualiza a UI.

#### DHT22

- Host puxa o barramento para baixo por ≥ 1 ms, solta; o sensor responde com
  presença e 40 bits.
- Cada bit: 50 µs baixo + 26–28 µs (0) ou 70 µs (1) alto.
- Com `delayMicroseconds` virtual, o modelo funciona no shim; sem resolução
  temporal suficiente, usar um "sensor transacional" (a biblioteca chama uma
  API de alto nível) e documentar a diferença.

### 5.5 Níveis de fidelidade

| Nível | O que garante | Custo |
|---|---|---|
| `Functional` | lógica e protocolo, sem tensão/corrente | baixo |
| `Electrical` | tensões, correntes, limites | médio/alto |
| `Timing` | bordas e atrasos em µs/ns | alto |

O projeto escolhe o nível por dispositivo; o engine promove automaticamente
quando um elemento analógico aparece.

---

## 6. Framework de placas

### 6.1 Composição

```
BoardInstance
 ├─ McuInstance (runtime + pin mux + memória)
 ├─ PowerTree (USB, regulador, rails)
 ├─ OnboardDevices (LED, BOOT, USB-UART)
 ├─ PinMap (Arduino Dx ↔ GPIOx ↔ periférico)
 ├─ Peripherals (ADC, LEDC, I2C, SPI, UART, RMT, PCNT, timers)
 └─ Strapping / boot config
```

### 6.2 Descritor declarativo

```json
{
  "board_id": "simforge.esp32s3.devkitc",
  "version": 1,
  "mcu": "esp32s3",
  "display_name": "ESP32-S3 DevKitC-1",
  "pins": [
    { "arduino": "D2", "gpio": 2, "caps": ["gpio", "adc1_2", "touch"] },
    { "arduino": "D4", "gpio": 4, "caps": ["gpio", "adc1_3", "strapping"] }
  ],
  "power": { "usb_v": 5.0, "rail_v": 3.3, "max_current_ma": 500 },
  "onboard": [
    { "type": "simforge.led", "pin": "GPIO48", "color": "rgb" },
    { "type": "simforge.button", "pin": "GPIO0", "role": "boot" }
  ],
  "runtime": { "default": "wasm", "cpu_hz": 240000000, "flash_kb": 8192, "psram_kb": 8192 }
}
```

### 6.3 Extensibilidade

```cpp
class IBoardBuilder {
public:
    virtual ~IBoardBuilder() = default;
    virtual const BoardDescriptor& descriptor() const = 0;
    virtual void build(BoardInstance&) const = 0;   // adiciona MCU, pinos, onboard
};
```

- Placa simples: apenas JSON, sem código.
- Placa com periféricos exóticos: builder em plugin.
- Novo MCU: implementa `IMcuRuntime` + `IPeripheralSet`; a placa só referencia.
- Registro idêntico ao de dispositivos (`BoardRegistry`).

### 6.4 Múltiplas placas

```
Board A (ESP32-S3)          Board B (RP2040)
   GPIO4 ──────── UART ──────── GP0
   GND   ──────── GND  ──────── GND
   GPIO5 ──────── I2C SDA ────── GP4
   GPIO6 ──────── I2C SCL ────── GP5
```

- Netlist global; cada placa tem seu runtime e seu `GuestClock`.
- O scheduler global sincroniza os runtimes por fatia.
- Sem GND comum → erro elétrico.
- UART entre placas: TX de A → RX de B, com baud e framing independentes.

---

## 7. ESP32-S3

### 7.1 Estratégia MVP — interceptação de APIs

#### Camadas

```
┌─────────────────────────────────────────────────────────────┐
│ Sketch Arduino (.ino/.cpp)                                  │
│   pinMode, digitalWrite, analogRead, Wire, Serial, delay... │
└───────────────────────────┬─────────────────────────────────┘
                            │ compila contra
┌───────────────────────────▼─────────────────────────────────┐
│ SACL — SimForge Arduino Compatibility Layer                 │
│   headers: Arduino.h, Wire.h, SPI.h, EEPROM.h, ...          │
│   implementação: chamadas para host functions               │
└───────────────────────────┬─────────────────────────────────┘
                            │ imports (WASM) / símbolos (DLL)
┌───────────────────────────▼─────────────────────────────────┐
│ Runtime Bridge                                              │
│   pin latch, timers, UART FIFO, I2C/SPI transactions        │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│ SimForge Engine (nets, dispositivos, tempo virtual)         │
└─────────────────────────────────────────────────────────────┘
```

#### O que o SACL implementa (v1)

| Grupo | APIs |
|---|---|
| GPIO | `pinMode`, `digitalWrite`, `digitalRead`, `attachInterrupt`, `detachInterrupt` |
| Analógico | `analogRead`, `analogReadResolution`, `analogWrite`, `ledcSetup`, `ledcAttachPin`, `ledcWrite` |
| Tempo | `millis`, `micros`, `delay`, `delayMicroseconds`, `yield` |
| Serial | `Serial.begin/print/println/read/available/write`, `Serial1` (UART) |
| I2C | `Wire.begin/beginTransmission/write/endTransmission/requestFrom/read` |
| SPI | `SPI.begin/beginTransaction/transfer/endTransaction` |
| Som | `tone`, `noTone` |
| Utilidades | `map`, `constrain`, `random`, `randomSeed`, `String` (subset) |
| FreeRTOS | `xTaskCreate`, `vTaskDelay`, `xQueue*`, `xSemaphore*` (cooperativo) |
| Preferências | `EEPROM`, `Preferences` (arquivo virtual) |

Fora do escopo v1: Wi-Fi, Bluetooth, I2S, câmera, USB, deep sleep real, acesso
direto a registradores, bibliotecas que dependem do core Xtensa.

#### Compilação

```
PlatformIO / arduino-cli
        │
        ├─ platform: platform-simforge (custom)
        ├─ board:    esp32s3-sim
        ├─ framework: arduino (SACL)
        ├─ toolchain: clang/MSVC (native) ou wasm32-wasi (sandbox)
        └─ libs:     Arduino Library Manager (somente compatíveis)
```

Fluxo:

1. Gerar `platformio.ini`/`sketch.json` efêmero no staging.
2. Resolver bibliotecas por versão (lockfile).
3. Compilar com `-DSIMFORGE=1` e include path do SACL.
4. Linkar com exports `simforge_setup`, `simforge_loop`, `simforge_step`.
5. Validar imports permitidos (host functions).
6. Produzir `.wasm` (padrão) ou `.dll` (dev mode) + `.sym` + `.map`.
7. Cachear por hash de `(fontes, libs, placa, shim, flags)`.

#### Execução e tempo

- `setup()` roda uma vez; `loop()` repete.
- `delay(ms)` agenda `nextWake = now + ms` e devolve o controle.
- `micros()` devolve o tempo virtual.
- Custo de API: cada chamada avança o tempo em `api_cost_ns` (padrão:
  GPIO 100 ns, I2C = tempo real da transação, loop 500 ns). Sem isso, um
  `while (!digitalRead(pin))` nunca avançaria o tempo.
- Watchdog: orçamento de instruções/fuel por fatia (ex.: 2 ms de execução
  virtual) e limite de iterações sem yield.
- Interrupções: callback enfileirado e executado no início da próxima fatia;
  latência padrão 2 µs.

#### Limitações declaradas

- Não é cycle-accurate.
- Não executa binários Xtensa.
- Bibliotecas que usam registradores ou assembly não compilam.
- Wi-Fi/BT não funcionam.
- Dual-core é single-core cooperativo.
- Timing de bit-bang é aproximado e calibrado.

Essas limitações fazem parte do contrato do MVP e aparecem no relatório de
compatibilidade do build.

### 7.2 Evolução — emulação completa

#### Alvo

```
Firmware Xtensa ELF/BIN  ──►  Emulador (QEMU ou ISS próprio)
                                   │
                                   ├─ CPU Xtensa LX7 (dual core)
                                   ├─ memória (flash, SRAM, PSRAM)
                                   ├─ timers, interrupt matrix
                                   ├─ GPIO, LEDC, I2C, SPI, UART, ADC, RMT
                                   ├─ DMA
                                   └─ FreeRTOS / ESP-IDF sem modificação
                                   │
                                   ▼
                         Bridge de co-simulação
                         (pinos, barramentos, tempo)
                                   │
                                   ▼
                         Engine SimForge (nets, devices)
```

#### Caminho recomendado

1. Integrar o fork QEMU da Espressif (Xtensa, máquina ESP32-S3) como processo
   separado.
2. Trocar os periféricos que importam por modelos que conversam com o netlist
   via shared memory/pipe.
3. Sincronizar tempo com `icount` + barreiras por fatia.
4. Expor GDB stub para depuração.
5. Suportar ESP-IDF (ELF + partições + flash image).

#### Vantagens

- Executa firmware real, inclusive bibliotecas com registradores.
- FreeRTOS, interrupções, DMA e dual-core reais.
- ESP-IDF sem shim.
- Timing muito mais próximo do hardware.

#### Dificuldades

- Modelagem de periféricos (RMT, PCNT, USB, I2S, ADC) é trabalhosa.
- Wi-Fi/BT exigem modelo de rádio/MAC — inviável no curto prazo.
- Performance: emulação + solver elétrico pode não sustentar 1× em projetos
  grandes.
- Determinismo com `icount` e I/O externo exige disciplina.
- Manutenção de um fork de QEMU e licenciamento GPL (processo separado).
- Debug e observabilidade precisam de integração com o GDB stub.

#### Escrever um ISS próprio?

| Aspecto | QEMU | ISS próprio |
|---|---|---|
| Tempo até o primeiro firmware | 3–6 meses | 12–24 meses |
| Cobertura de periféricos | média, extensível | total controle |
| Performance | TCG maduro | a construir |
| Licença | GPL (processo separado) | própria |
| Manutenção | fork | total |

Recomendação: QEMU primeiro; ISS próprio apenas se houver requisito de licença
ou de integração que o QEMU não atenda.

### 7.3 Matriz de evolução

| Recurso | MVP (shim) | v1.5 (QEMU) | v2 (completo) |
|---|---|---|---|
| GPIO | ✅ | ✅ | ✅ |
| ADC/PWM | ✅ | ✅ | ✅ |
| I2C/SPI/UART | ✅ | ✅ | ✅ |
| Timers/ISR | aproximado | ✅ | ✅ |
| FreeRTOS | cooperativo | ✅ | ✅ |
| DMA | ❌ | parcial | ✅ |
| Registradores | ❌ | ✅ | ✅ |
| ESP-IDF | ❌ | ✅ | ✅ |
| Wi-Fi/BT | ❌ | ❌ | stub/modelo |
| Dual-core | ❌ | ✅ | ✅ |
| Determinismo | forte | forte com icount | forte |

---

## 8. Compilação de firmware

### 8.1 Contrato

```cpp
struct BuildRequest {
    BoardId board;
    std::filesystem::path project_dir;
    BuildProfile profile;              // Debug | Release
    RuntimeKind runtime;               // Wasm | Native | Qemu
    std::vector<LibraryRef> libraries;
    std::optional<std::string> defines;
};

struct BuildDiagnostic {
    Severity severity;
    std::string file;
    int line, column;
    std::string code;
    std::string message;
};

struct BuildResult {
    BuildStatus status;
    FirmwareArtifact artifact;         // caminho, hash, formato, símbolos
    std::vector<BuildDiagnostic> diagnostics;
    MemoryReport memory;               // flash/RAM estimadas
    std::string log;
};

class ICompilerBackend {
public:
    virtual ~ICompilerBackend() = default;
    virtual std::string_view id() const = 0;
    virtual bool supports(const BuildRequest&) const = 0;
    virtual BuildResult build(const BuildRequest&, IProgressSink&) = 0;
};
```

### 8.2 Pipeline

```
firmware.build
   │
   ├─ 1. validar projeto e placa
   ├─ 2. resolver bibliotecas (lockfile)
   ├─ 3. hash de entrada
   ├─ 4. cache hit? ──► artefato + diagnóstico em cache
   ├─ 5. staging isolado (temp, quota, sem rede)
   ├─ 6. injetar SACL/variant/defines
   ├─ 7. executar backend (subprocesso, timeout)
   ├─ 8. parsear diagnósticos (GCC/Clang/arduino-cli)
   ├─ 9. validar artefato (imports, tamanho, assinatura)
   ├─ 10. persistir cache + evento de conclusão
   └─ 11. devolver artifact_id
```

### 8.3 "Upload" virtual

1. `sim.loadFirmware({artifact_id})` → supervisor valida hash e formato.
2. Runtime carrega o módulo e chama `simforge_setup`.
3. Progresso vira eventos (`firmware.load.progress`).
4. Não há flashing, reset físico ou bootloader; o estado inicial é definido
   pelo runtime.

### 8.4 ESP-IDF (futuro)

- Backend `EspIdfBackend`: `idf.py set-target esp32s3`, `idf.py build`.
- Artefatos: `bootloader.bin`, `partition-table.bin`, `app.bin` → imagem de
  flash.
- Runtime QEMU carrega a imagem e executa o boot real.
- Diagnósticos e tamanho de partição reportados na UI.
- Requer toolchain Xtensa e Python; roda em processo isolado.

### 8.5 Isolamento de build

- Diretório temporário por build; quota de disco.
- Sem rede (exceto resolução explícita de bibliotecas, feita antes).
- Timeout configurável (padrão 120 s).
- Limite de CPU/memória no subprocesso.
- Nenhum script do projeto executa no host; hooks de build são permitidos apenas
  em plugins assinados.
