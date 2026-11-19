// Tipos do contrato api-v1 (docs/protocolos/api-v1.md) e do documento
// `.sforge` (docs/formatos/sforge-v1.md).
//
// O frontend só desenha o que o servidor devolve: nada aqui implementa
// semântica de simulação.

export type Method =
  | 'app.ping'
  | 'app.info'
  | 'project.state'
  | 'project.validate'
  | 'circuit.addDevice'
  | 'circuit.setDevice'
  | 'circuit.removeDevice'
  | 'circuit.erc'
  | 'device.catalog'
  | 'board.catalog'
  | 'sim.load'
  | 'sim.run'
  | 'sim.pause'
  | 'sim.resume'
  | 'sim.stop'
  | 'sim.reset'
  | 'sim.step'
  | 'sim.state'
  | 'sim.frame'
  | 'sim.setSpeed'
  | 'sim.debug.setPin'
  | 'sim.debug.releasePin'
  | 'events.poll'

export interface ApiRequest {
  id: string
  method: Method
  params?: Record<string, unknown>
  meta?: {
    api_version?: number
    client?: string
    idempotency_key?: string
  }
}

export interface ApiMeta {
  revision?: number
  t_ns?: number
  trace_id?: string
  warnings?: string[]
}

export interface ApiError {
  code: string
  message: string
  details?: Record<string, unknown>
  retryable?: boolean
  trace_id?: string
}

export interface ApiSuccess<T> {
  id: string
  ok: true
  result: T
  meta?: ApiMeta
}

export interface ApiFailure {
  id: string
  ok: false
  error: ApiError
}

export type ApiResponse<T> = ApiSuccess<T> | ApiFailure

export interface ApiEvent {
  seq: number
  t_ns: number
  type: string
  payload: Record<string, unknown>
}

// --- documento .sforge -----------------------------------------------------

export interface Point {
  x: number
  y: number
}

export interface PinRef {
  board?: string
  device?: string
  pin: string
}

export const NET_KINDS = ['digital', 'analog', 'power', 'ground', 'bus'] as const
export type NetKind = (typeof NET_KINDS)[number]

export interface PinBinding {
  net?: string
}

export interface BoardInstance {
  id: string
  type: string
  type_version: number
  position: Point
  rotation: number
  label?: string
  params: Record<string, unknown>
  /** Preenchido pelo projeto; a placa recém-criada pode vir sem vínculos. */
  pins?: Record<string, PinBinding>
}

export interface DeviceInstance {
  id: string
  type: string
  type_version: number
  position: Point
  rotation: number
  label?: string
  params: Record<string, unknown>
  pins?: Record<string, PinBinding>
}

export interface Net {
  id: string
  name: string
  kind: NetKind
  pins: PinRef[]
}

export interface Wire {
  id: string
  net: string
  points: Point[]
  color?: string
}

export interface SforgeDocument {
  format: 'simforge.project'
  schema: 1
  project: { id: string; name: string; revision: number }
  units: Record<string, string | number>
  boards: BoardInstance[]
  devices: DeviceInstance[]
  nets: Net[]
  wires: Wire[]
  firmware: {
    entry: string
    backend: string
    framework: string
    libraries: string[]
  }
  simulation: Record<string, unknown>
}

// --- catálogos -------------------------------------------------------------

export type PinDirection = 'input' | 'output' | 'bidirectional' | 'power' | 'ground'

export interface TypePin {
  name: string
  direction: PinDirection
  description?: string
}

export interface DeviceTypeInfo {
  type: string
  type_version: number
  name: string
  category: string
  pins: TypePin[]
}

export interface BoardTypeInfo {
  type: string
  type_version: number
  name: string
  mcu: string
  capabilities: string[]
}

// --- respostas -------------------------------------------------------------

export interface AppInfo {
  product: string
  version: string
  api_version: number
  engine_version: string
  platform: string
  capabilities: string[]
}

export interface ProjectState {
  document: SforgeDocument
  revision: number
  path: string | null
  dirty: boolean
}

export interface Diagnostic {
  severity: 'info' | 'warning' | 'error'
  code: string
  message: string
  path?: string
  net?: string
  fix?: string
}

export type SimState =
  | 'idle'
  | 'loading'
  | 'ready'
  | 'running'
  | 'paused'
  | 'stopped'
  | 'error'

export interface SimStatus {
  state: SimState
  t_ns: number
  speed: number
  revision: number
  events_processed?: number
  runtime?: string
  stats?: { events?: number; trace_hash?: number }
}

export interface DeviceRuntime {
  id: string
  type: string
  on?: boolean
  brightness?: number
  pressed?: boolean
  color?: string
  ohms?: number
  freq_hz?: number
  duty?: number
  period_ns?: number
}

export interface NetRuntime {
  id: string
  /** Só o mock manda nome/tipo; o núcleo devolve id, nível e força. */
  name?: string
  kind?: NetKind
  level: 'low' | 'high' | 'z' | 'x'
  strength?: 'none' | 'weak' | 'strong'
}

export interface SimFrame {
  seq: number
  t_ns: number
  state: SimState
  devices: DeviceRuntime[]
  nets: NetRuntime[]
}

export interface EventBatch {
  events: ApiEvent[]
  next_seq: number
  dropped: number
}
