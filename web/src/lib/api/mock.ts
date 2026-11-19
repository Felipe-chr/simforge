// Servidor de mentira do protótipo visual.
//
// Implementa o mesmo envelope da api-v1 (docs/protocolos/api-v1.md) para que a
// interface seja construída contra o contrato real. Nenhum cálculo elétrico
// acontece aqui: os estados vêm de um cenário fixo (`blink-led`), apenas para
// desenhar a tela. Trocar `MockTransport` por WebSocket/C ABI é o único passo
// para ligar a UI ao núcleo de verdade.

import type {
  ApiEvent,
  ApiRequest,
  ApiResponse,
  AppInfo,
  BoardTypeInfo,
  DeviceTypeInfo,
  Diagnostic,
  EventBatch,
  NetKind,
  PinRef,
  ProjectState,
  SimFrame,
  SimState,
  SimStatus,
  SforgeDocument,
  DeviceRuntime,
} from './types'
import type { Transport } from './transport'

const ALPHABET = '0123456789ABCDEFGHJKMNPQRSTVWXYZ'

function encodeBase32(value: number, length: number): string {
  let rest = Math.trunc(value)
  let out = ''
  for (let i = 0; i < length; i += 1) {
    out = ALPHABET[rest % 32] + out
    rest = Math.floor(rest / 32)
  }
  return out
}

let counter = 0

/** ULID aproximado: timestamp de 48 bits + 80 bits pseudoaleatórios. */
function nextId(): string {
  counter = (counter + 1) % 65536
  const time = encodeBase32(Date.now(), 10)
  const random = Math.floor(Math.random() * 0xffffffff)
  const tail = encodeBase32(random, 6) + encodeBase32(counter, 4) + encodeBase32(random % 99991, 6)
  return (time + tail).slice(0, 26)
}

export const BOARD_CATALOG: BoardTypeInfo[] = [
  {
    type: 'simforge.esp32s3.devkitc',
    type_version: 1,
    name: 'ESP32-S3 DevKitC-1',
    mcu: 'ESP32-S3-WROOM-1',
    capabilities: ['wifi', 'ble', 'usb-serial', 'jtag', 'psram'],
  },
]

export const DEVICE_CATALOG: DeviceTypeInfo[] = [
  {
    type: 'simforge.led',
    type_version: 1,
    name: 'LED',
    category: 'atuador',
    pins: [
      { name: 'A', direction: 'input', description: 'anodo' },
      { name: 'K', direction: 'input', description: 'catodo' },
    ],
  },
  {
    type: 'simforge.button',
    type_version: 1,
    name: 'Botão',
    category: 'entrada',
    pins: [
      { name: '1', direction: 'bidirectional', description: 'terminal 1' },
      { name: '2', direction: 'bidirectional', description: 'terminal 2' },
    ],
  },
  {
    type: 'simforge.resistor',
    type_version: 1,
    name: 'Resistor',
    category: 'passivo',
    pins: [
      { name: 'A', direction: 'bidirectional', description: 'terminal A' },
      { name: 'B', direction: 'bidirectional', description: 'terminal B' },
    ],
  },
  {
    type: 'simforge.clock',
    type_version: 1,
    name: 'Clock/PWM',
    category: 'component',
    pins: [{ name: 'OUT', direction: 'output', description: 'saída digital' }],
  },
  {
    type: 'simforge.potentiometer',
    type_version: 1,
    name: 'Potenciômetro',
    category: 'entrada',    pins: [
      { name: 'A', direction: 'bidirectional', description: 'terminal A' },
      { name: 'B', direction: 'bidirectional', description: 'terminal B' },
      { name: 'W', direction: 'output', description: 'cursor' },
    ],
  },
  {
    type: 'simforge.buzzer',
    type_version: 1,
    name: 'Buzzer',
    category: 'atuador',
    pins: [
      { name: '-', direction: 'input', description: 'negativo' },
      { name: '+', direction: 'input', description: 'positivo' },
    ],
  },
  {
    type: 'simforge.oled',
    type_version: 1,
    name: 'OLED 128x64 (I2C)',
    category: 'display',
    pins: [
      { name: 'GND', direction: 'power', description: 'terra' },
      { name: 'VCC', direction: 'power', description: '3V3' },
      { name: 'SCL', direction: 'input', description: 'clock I2C' },
      { name: 'SDA', direction: 'bidirectional', description: 'dados I2C' },
    ],
  },
]

const APP_INFO: AppInfo = {
  product: 'SimForge',
  version: '0.1.0',
  api_version: 1,
  engine_version: '0.1.0',
  platform: 'windows-x64',
  capabilities: [
    'sim.native',
    'compiler.arduino',
    'plugin.hot_reload',
    'world.websocket',
    'rec.replay',
  ],
}

function initialDocument(): SforgeDocument {
  const boardId = nextId()
  const ledId = nextId()
  const buttonId = nextId()
  const gnd = { board: boardId, pin: 'GND' }
  const gpio2 = { board: boardId, pin: 'GPIO2' }
  const gpio0 = { board: boardId, pin: 'GPIO0' }

  return {
    format: 'simforge.project',
    schema: 1,
    project: { id: nextId(), name: 'Blink LED', revision: 1 },
    units: {
      length: 'mm',
      grid_mm: 1.27,
      voltage: 'V',
      current: 'A',
      resistance: 'ohm',
      time: 'ns',
    },
    boards: [
      {
        id: boardId,
        type: 'simforge.esp32s3.devkitc',
        type_version: 1,
        position: { x: 118, y: 128 },
        rotation: 0,
        label: 'U1',
        params: {},
        pins: { GPIO2: { net: 'N1' }, GPIO0: { net: 'N3' }, GND: { net: 'N2' } },
      },
    ],
    devices: [
      {
        id: ledId,
        type: 'simforge.led',
        type_version: 1,
        position: { x: 744, y: 232 },
        rotation: 0,
        label: 'D1',
        params: { color: 'red', vf: 1.9 },
        pins: { A: { net: 'N1' }, K: { net: 'N2' } },
      },
      {
        id: buttonId,
        type: 'simforge.button',
        type_version: 1,
        position: { x: 744, y: 392 },
        rotation: 0,
        label: 'SW1',
        params: { pull: 'up' },
        pins: { '1': { net: 'N3' }, '2': { net: 'N2' } },
      },
    ],
    nets: [
      {
        id: 'N1',
        name: 'LED_A',
        kind: 'digital',
        pins: [gpio2, { device: ledId, pin: 'A' }],
      },
      {
        id: 'N2',
        name: 'GND',
        kind: 'ground',
        pins: [gnd, { device: ledId, pin: 'K' }, { device: buttonId, pin: '2' }],
      },
      {
        id: 'N3',
        name: 'BTN',
        kind: 'digital',
        pins: [gpio0, { device: buttonId, pin: '1' }],
      },
    ],
    wires: [],
    firmware: {
      entry: 'src/main.cpp',
      backend: 'platformio',
      framework: 'arduino',
      libraries: [],
    },
    simulation: {
      solver: 'auto',
      seed: 0,
      slice_us: 1000,
      analog_step_us: 10,
      real_time: false,
    },
  }
}

const BLINK_PERIOD_NS = 500_000_000

export class MockTransport implements Transport {
  readonly kind = 'mock' as const

  private document: SforgeDocument = initialDocument()
  private revision = 1
  private state: SimState = 'ready'
  private speed = 1
  private baseTNs = 0
  private startedAt: number | null = null
  private events: ApiEvent[] = []
  private seq = 1
  private frameSeq = 1
  /** Estímulos de bancada por net (sim.debug.*). */
  private pinLevels = new Map<string, 'low' | 'high' | 'z'>()

  private nowNs(): number {
    if (this.state === 'running' && this.startedAt !== null) {
      const elapsedMs = performance.now() - this.startedAt
      return this.baseTNs + Math.round(elapsedMs * 1e6 * this.speed)
    }
    return this.baseTNs
  }

  private emit(type: string, payload: Record<string, unknown>): void {
    this.events.push({ seq: this.seq, t_ns: this.nowNs(), type, payload })
    this.seq += 1
    if (this.events.length > 200) this.events.splice(0, this.events.length - 200)
  }

  private ok<T>(id: string, result: T): ApiResponse<T> {
    return {
      id,
      ok: true,
      result,
      meta: { revision: this.revision, t_ns: this.nowNs() },
    }
  }

  private fail(id: string, code: string, message: string): ApiResponse<never> {
    return {
      id,
      ok: false,
      error: { code, message, retryable: false, details: {} },
    }
  }

  private runStatus(): SimStatus {
    return {
      state: this.state,
      t_ns: this.nowNs(),
      speed: this.speed,
      revision: this.revision,
      events_processed: this.seq - 1,
    }
  }

  private buildFrame(): SimFrame {
    const t = this.nowNs()
    const running = this.state === 'running'
    const lit = running && Math.floor(t / BLINK_PERIOD_NS) % 2 === 0

    const nets = this.document.nets.map((net) => {
      const name = net.name.toLowerCase()
      const forced = this.pinLevels.get(net.id)
      // Clock: a saída acompanha o tempo virtual do mock (não há solver).
      const clock = this.document.devices.find(
        (entry) => entry.type === 'simforge.clock' && entry.pins?.OUT?.net === net.id,
      )
      let driven: 'low' | 'high' | 'z' | undefined
      if (clock) {
        const freq = Number(clock.params.freq_hz ?? 1000)
        const duty = Number(clock.params.duty ?? 0.5)
        if (!running) driven = 'z'
        else if (!(freq > 0) || duty <= 0) driven = 'low'
        else if (duty >= 1) driven = 'high'
        else {
          const period = 1e9 / freq
          driven = (t % period) / period < duty ? 'high' : 'low'
        }
      }
      // Sem driver (parado) o net aparece em alta-Z, como no núcleo. O pino
      // com pull-up (botão em N3) fica em high fraco mesmo parado.
      const idle = net.id === 'N3' ? 'high' : 'z'
      const active = net.id === 'N3' ? 'high' : lit ? 'high' : 'low'
      const level =
        forced ??
        driven ??
        (net.kind === 'ground' || name === 'gnd' ? 'low' : running ? active : idle)
      return { id: net.id, name: net.name, kind: net.kind, level }
    })
    const levels = new Map(nets.map((net) => [net.id, net.level]))

    const levelOf = (deviceId: string, pin: string) => {
      const device = this.document.devices.find((entry) => entry.id === deviceId)
      const netId = device?.pins?.[pin]?.net
      return netId ? levels.get(netId) : undefined
    }

    const devices: DeviceRuntime[] = this.document.devices.map((device) => {
      if (device.type === 'simforge.led') {
        const on = (levelOf(device.id, 'A') ?? (lit ? 'high' : 'low')) === 'high'
        return {
          id: device.id,
          type: device.type,
          on,
          brightness: on ? 1 : 0,
          color: typeof device.params.color === 'string' ? device.params.color : 'red',
        }
      }
      if (device.type === 'simforge.button') {
        return { id: device.id, type: device.type, pressed: false }
      }
      if (device.type === 'simforge.resistor') {
        return { id: device.id, type: device.type, ohms: Number(device.params.ohms ?? 1000) }
      }
      if (device.type === 'simforge.clock') {
        const freq = Number(device.params.freq_hz ?? 1000)
        const duty = Number(device.params.duty ?? 0.5)
        const period = freq > 0 ? 1e9 / freq : 0
        const on =
          running && duty > 0 && (duty >= 1 || (period > 0 && (t % period) / period < duty))
        return {
          id: device.id,
          type: device.type,
          on,
          freq_hz: freq,
          duty,
          period_ns: period,
        }
      }
      return { id: device.id, type: device.type }
    })

    return {
      seq: this.frameSeq,
      t_ns: t,
      state: this.state,
      devices,
      nets,
    }
  }

  async execute<T = unknown>(request: ApiRequest): Promise<ApiResponse<T>> {
    const { id, method, params = {} } = request
    // Latência mínima para a UI exercitar estados de carregamento.
    await new Promise((resolve) => setTimeout(resolve, 8))

    switch (method) {
      case 'app.ping':
        return this.ok(id, { nonce: params.nonce ?? null, t_ns: this.nowNs() } as T)

      case 'app.info':
        return this.ok(id, APP_INFO as T)

      case 'project.state': {
        const state: ProjectState = {
          document: structuredClone(this.document),
          revision: this.revision,
          path: null,
          dirty: true,
        }
        return this.ok(id, state as T)
      }

      case 'project.validate':
        return this.ok(id, { diagnostics: [] as Diagnostic[] } as T)

      case 'circuit.erc':
        return this.ok(id, { diagnostics: [] as Diagnostic[], clean: true } as T)

      case 'device.catalog':
        return this.ok(id, { devices: DEVICE_CATALOG } as T)

      case 'board.catalog':
        return this.ok(id, { boards: BOARD_CATALOG } as T)

      case 'circuit.addDevice': {
        const type = String(params.type ?? '')
        const info = DEVICE_CATALOG.find((entry) => entry.type === type)
        if (!info) return this.fail(id, 'invalid_params', `tipo desconhecido: ${type}`)
        const position = (params.position as { x: number; y: number } | undefined) ?? {
          x: 640,
          y: 300,
        }
        const device = {
          id: nextId(),
          type,
          type_version: info.type_version,
          position,
          rotation: 0,
          label: info.name,
          params: (params.params as Record<string, unknown> | undefined) ?? {},
          pins: {},
        }
        this.document.devices.push(device)
        this.revision += 1
        this.document.project.revision = this.revision
        this.emit('circuit.changed', { kinds: ['device'], revision: this.revision })
        return this.ok(id, { device_id: device.id, revision: this.revision } as T)
      }

      case 'circuit.setDevice': {
        const deviceId = String(params.device_id ?? '')
        const device = this.document.devices.find((entry) => entry.id === deviceId)
        if (!device) return this.fail(id, 'not_found', `dispositivo inexistente: ${deviceId}`)
        if (params.position) device.position = params.position as { x: number; y: number }
        if (typeof params.rotation === 'number') device.rotation = params.rotation
        if (typeof params.label === 'string') device.label = params.label
        if (params.params) {
          device.params = { ...device.params, ...(params.params as Record<string, unknown>) }
        }
        this.revision += 1
        this.document.project.revision = this.revision
        this.emit('circuit.changed', { kinds: ['device'], revision: this.revision })
        return this.ok(id, { device_id: device.id, revision: this.revision } as T)
      }

      case 'circuit.removeDevice': {
        const deviceId = String(params.device_id ?? '')
        const before = this.document.devices.length
        this.document.devices = this.document.devices.filter((entry) => entry.id !== deviceId)
        if (this.document.devices.length === before) {
          return this.fail(id, 'not_found', `dispositivo inexistente: ${deviceId}`)
        }
        this.document.nets = this.document.nets
          .map((net) => ({
            ...net,
            pins: net.pins.filter((pin: PinRef) => pin.device !== deviceId),
          }))
          .filter((net) => net.pins.length > 0)
        this.revision += 1
        this.document.project.revision = this.revision
        this.emit('circuit.changed', { kinds: ['device'], revision: this.revision })
        return this.ok(id, { revision: this.revision } as T)
      }

      case 'sim.load':
        this.state = 'ready'
        this.baseTNs = 0
        this.startedAt = null
        this.pinLevels.clear()
        this.emit('sim.state', { state: this.state, t_ns: 0, revision: this.revision })
        return this.ok(id, {
          session_id: nextId(),
          state: this.state,
          diagnostics: [],
        } as T)

      case 'sim.run':
        this.state = 'running'
        this.startedAt = performance.now()
        this.emit('sim.state', {
          state: this.state,
          t_ns: this.baseTNs,
          revision: this.revision,
        })
        return this.ok(id, { state: this.state, t_ns: this.nowNs() } as T)

      case 'sim.pause': {
        if (this.state === 'running') this.baseTNs = this.nowNs()
        this.state = 'paused'
        this.startedAt = null
        this.emit('sim.state', { state: this.state, t_ns: this.baseTNs, revision: this.revision })
        return this.ok(id, { state: this.state, t_ns: this.baseTNs } as T)
      }

      case 'sim.resume':
        this.state = 'running'
        this.startedAt = performance.now()
        this.emit('sim.state', { state: this.state, t_ns: this.baseTNs, revision: this.revision })
        return this.ok(id, { state: this.state, t_ns: this.nowNs() } as T)

      case 'sim.stop':
        this.state = 'stopped'
        this.startedAt = null
        this.emit('sim.state', { state: this.state, t_ns: this.baseTNs, revision: this.revision })
        return this.ok(id, { state: this.state, t_ns: this.baseTNs } as T)

      case 'sim.reset':
        this.state = 'ready'
        this.baseTNs = 0
        this.startedAt = null
        this.emit('sim.state', { state: this.state, t_ns: 0, revision: this.revision })
        return this.ok(id, { state: this.state, t_ns: 0 } as T)

      case 'sim.step': {
        const ns = typeof params.ns === 'number' ? params.ns : 1_000_000
        this.baseTNs += Math.max(1, ns)
        this.emit('sim.state', { state: this.state, t_ns: this.baseTNs, revision: this.revision })
        return this.ok(id, { t_ns: this.baseTNs, events_processed: 2 } as T)
      }

      case 'sim.state':
        return this.ok(id, this.runStatus() as T)

      case 'sim.frame':
        this.frameSeq += 1
        return this.ok(id, { seq: this.frameSeq, t_ns: this.nowNs(), frame: this.buildFrame() } as T)

      case 'sim.setSpeed': {
        const factor = Number(params.factor ?? 1)
        if (!Number.isFinite(factor) || factor <= 0) {
          return this.fail(id, 'invalid_params', 'factor deve ser positivo')
        }
        this.baseTNs = this.nowNs()
        this.startedAt = this.state === 'running' ? performance.now() : null
        this.speed = factor
        return this.ok(id, { speed: this.speed } as T)
      }

      case 'sim.debug.setPin': {
        const target = params.target as { board?: string; pin?: string } | undefined
        if (!target?.board || !target.pin) {
          return this.fail(id, 'invalid_params', 'target.board e target.pin são obrigatórios')
        }
        const board = this.document.boards.find((entry) => entry.id === target.board)
        if (!board) return this.fail(id, 'not_found', `placa inexistente: ${target.board}`)
        const netId = board.pins?.[target.pin]?.net
        if (!netId) return this.fail(id, 'not_found', `pino sem net: ${target.pin}`)
        const level = String(params.level ?? 'z')
        if (level !== 'low' && level !== 'high' && level !== 'z') {
          return this.fail(id, 'invalid_params', 'level deve ser low, high ou z')
        }
        this.pinLevels.set(netId, level)
        this.emit('sim.pin', { board: target.board, pin: target.pin, level, net: netId })
        return this.ok(id, { t_ns: this.nowNs() } as T)
      }

      case 'sim.debug.releasePin': {
        const target = params.target as { board?: string; pin?: string } | undefined
        if (!target?.board || !target.pin) {
          return this.fail(id, 'invalid_params', 'target.board e target.pin são obrigatórios')
        }
        const board = this.document.boards.find((entry) => entry.id === target.board)
        const netId = board?.pins?.[target.pin]?.net
        if (!netId) return this.fail(id, 'not_found', `pino sem net: ${target.pin}`)
        this.pinLevels.delete(netId)
        this.emit('sim.pin', { board: target.board, pin: target.pin, level: 'z', net: netId })
        return this.ok(id, { t_ns: this.nowNs() } as T)
      }

      case 'events.poll': {
        const after = Number(params.after_seq ?? 0)
        const max = Number(params.max ?? 256)
        const batch: EventBatch = {
          events: this.events.filter((event) => event.seq > after).slice(0, max),
          next_seq: this.seq,
          dropped: 0,
        }
        return this.ok(id, batch as T)
      }

      default:
        return this.fail(id, 'unknown_method', `método não registrado: ${method}`)
    }
  }

  /** Usado só pelo cenário de exemplo ao trocar a cor do LED. */
  netKindOf(netId: string): NetKind | undefined {
    return this.document.nets.find((net) => net.id === netId)?.kind
  }
}

export const transport = new MockTransport()
