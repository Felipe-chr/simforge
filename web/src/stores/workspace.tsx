import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
} from 'react'
import type { ReactNode } from 'react'

import { resolveTransport } from '../lib/api'
import type { BridgeStatus, Transport } from '../lib/api/transport'
import type {
  ApiRequest,
  ApiResponse,
  AppInfo,
  DeviceTypeInfo,
  Diagnostic,
  Point,
  SimFrame,
  SimStatus,
  SforgeDocument,
} from '../lib/api/types'

export type Selection =
  | { kind: 'board'; id: string }
  | { kind: 'device'; id: string }
  | null

export interface LogLine {
  id: number
  kind: 'cmd' | 'event' | 'error' | 'info'
  text: string
}

interface WorkspaceValue {
  document: SforgeDocument | null
  revision: number
  dirty: boolean
  info: AppInfo | null
  catalog: DeviceTypeInfo[]
  diagnostics: Diagnostic[]
  sim: SimStatus
  frame: SimFrame | null
  selection: Selection
  log: LogLine[]
  ready: boolean
  source: Transport['kind']
  bridge: BridgeStatus | null
  select: (selection: Selection) => void
  addDevice: (type: string) => Promise<void>
  moveDevice: (deviceId: string, position: Point) => Promise<void>
  rotateDevice: (deviceId: string, rotation: number) => Promise<void>
  renameDevice: (deviceId: string, label: string) => Promise<void>
  setDeviceParam: (deviceId: string, key: string, value: unknown) => Promise<void>
  removeDevice: (deviceId: string) => Promise<void>
  run: () => Promise<void>
  pause: () => Promise<void>
  resume: () => Promise<void>
  stop: () => Promise<void>
  reset: () => Promise<void>
  step: () => Promise<void>
  setSpeed: (factor: number) => Promise<void>
  setPin: (boardId: string, pin: string, level: 'low' | 'high' | 'z') => Promise<void>
  releasePin: (boardId: string, pin: string) => Promise<void>
}

const WorkspaceContext = createContext<WorkspaceValue | null>(null)

let requestCounter = 0
let logCounter = 0

const INITIAL_SIM: SimStatus = {
  state: 'idle',
  t_ns: 0,
  speed: 1,
  revision: 0,
  events_processed: 0,
}

export function WorkspaceProvider({ children }: { children: ReactNode }) {
  const [document, setDocument] = useState<SforgeDocument | null>(null)
  const [revision, setRevision] = useState(0)
  const [dirty, setDirty] = useState(false)
  const [info, setInfo] = useState<AppInfo | null>(null)
  const [catalog, setCatalog] = useState<DeviceTypeInfo[]>([])
  const [diagnostics, setDiagnostics] = useState<Diagnostic[]>([])
  const [sim, setSim] = useState<SimStatus>(INITIAL_SIM)
  const [frame, setFrame] = useState<SimFrame | null>(null)
  const [selection, setSelection] = useState<Selection>(null)
  const [log, setLog] = useState<LogLine[]>([])
  const [ready, setReady] = useState(false)
  const [source, setSource] = useState<Transport['kind']>('mock')
  const [bridge, setBridge] = useState<BridgeStatus | null>(null)
  const booted = useRef(false)
  const transportRef = useRef<Transport | null>(null)
  const sourceRef = useRef<Transport['kind']>('mock')

  const pushLog = useCallback((kind: LogLine['kind'], text: string) => {
    logCounter += 1
    setLog((current) => {
      const next = [...current, { id: logCounter, kind, text }]
      return next.length > 200 ? next.slice(next.length - 200) : next
    })
  }, [])

  const send = useCallback(
    async <T,>(method: ApiRequest['method'], params?: Record<string, unknown>) => {
      const active = transportRef.current
      if (!active) return null

      requestCounter += 1
      const id = `web-${requestCounter}`
      try {
        const response = (await active.execute({
          id,
          method,
          params,
          meta: { api_version: 1, client: 'web/0.1.0' },
        })) as ApiResponse<T>

        if (!response.ok) {
          pushLog('error', `${method} → ${response.error.code}: ${response.error.message}`)
          return null
        }
        return response.result
      } catch (error) {
        const message = error instanceof Error ? error.message : String(error)
        pushLog('error', `${method} → transporte: ${message}`)
        return null
      }
    },
    [pushLog],
  )

  const refreshProject = useCallback(async () => {
    const state = await send<{ document: SforgeDocument; revision: number; dirty: boolean }>(
      'project.state',
    )
    if (!state) return
    setDocument(state.document)
    setRevision(state.revision)
    setDirty(state.dirty)
  }, [send])

  const refreshSim = useCallback(async () => {
    const status = await send<SimStatus>('sim.state')
    if (status) setSim(status)
  }, [send])

  const refreshFrame = useCallback(async () => {
    const result = await send<{ seq: number; t_ns: number; frame: SimFrame }>('sim.frame')
    if (result) setFrame(result.frame)
  }, [send])

  useEffect(() => {
    if (booted.current) return
    booted.current = true
    void (async () => {
      const resolved = await resolveTransport()
      transportRef.current = resolved.transport
      sourceRef.current = resolved.transport.kind
      setSource(resolved.transport.kind)
      setBridge(resolved.bridge)

      const appInfo = await send<AppInfo>('app.info')
      if (appInfo) setInfo(appInfo)
      const catalogResult = await send<{ devices: DeviceTypeInfo[] }>('device.catalog')
      if (catalogResult) setCatalog(catalogResult.devices)
      await send('sim.load')
      await refreshProject()
      await refreshSim()
      await refreshFrame()
      const erc = await send<{ diagnostics: Diagnostic[]; clean: boolean }>('circuit.erc')
      if (erc) setDiagnostics(erc.diagnostics)
      setReady(true)
      pushLog(
        'info',
        resolved.transport.kind === 'core'
          ? `sessão pronta · núcleo C++ via NDJSON · ${resolved.bridge?.project ?? 'projeto'}`
          : 'sessão pronta · mock da api-v1 · cenário blink-led',
      )
    })()
  }, [pushLog, refreshFrame, refreshProject, refreshSim, send])

  // Enquanto roda, a UI só consulta o "servidor" — nada de simular aqui.
  useEffect(() => {
    if (sim.state !== 'running') return
    const timer = window.setInterval(() => {
      void refreshFrame()
      void refreshSim()
    }, 120)
    return () => window.clearInterval(timer)
  }, [sim.state, refreshFrame, refreshSim])

  useEffect(() => {
    if (!selection && document) {
      const first = document.devices[0]
      if (first) setSelection({ kind: 'device', id: first.id })
    }
  }, [document, selection])

  const afterMutation = useCallback(
    async (what: string) => {
      await refreshProject()
      // No núcleo, qualquer mudança no projeto invalida o motor carregado.
      if (sourceRef.current === 'core') {
        await send('sim.load')
        await refreshSim()
        await refreshFrame()
      }
      pushLog('cmd', what)
      const erc = await send<{ diagnostics: Diagnostic[]; clean: boolean }>('circuit.erc')
      if (erc) setDiagnostics(erc.diagnostics)
    },
    [pushLog, refreshProject, refreshFrame, refreshSim, send],
  )

  const value = useMemo<WorkspaceValue>(
    () => ({
      document,
      revision,
      dirty,
      info,
      catalog,
      diagnostics,
      sim,
      frame,
      selection,
      log,
      ready,
      source,
      bridge,
      select: setSelection,

      addDevice: async (type) => {
        const result = await send<{ device_id: string }>('circuit.addDevice', {
          type,
          position: { x: 660, y: 300 },
        })
        if (result) {
          setSelection({ kind: 'device', id: result.device_id })
          await afterMutation(`circuit.addDevice ${type}`)
        }
      },

      moveDevice: async (deviceId, position) => {
        const result = await send('circuit.setDevice', { device_id: deviceId, position })
        if (result) await afterMutation(`circuit.setDevice ${deviceId} → (${position.x}, ${position.y})`)
      },

      rotateDevice: async (deviceId, rotation) => {
        const result = await send('circuit.setDevice', { device_id: deviceId, rotation })
        if (result) await afterMutation(`circuit.setDevice ${deviceId} rotação ${rotation}°`)
      },

      renameDevice: async (deviceId, label) => {
        const result = await send('circuit.setDevice', { device_id: deviceId, label })
        if (result) await afterMutation(`circuit.setDevice ${deviceId} rótulo "${label}"`)
      },

      setDeviceParam: async (deviceId, key, paramValue) => {
        const result = await send('circuit.setDevice', {
          device_id: deviceId,
          params: { [key]: paramValue },
        })
        if (result) await afterMutation(`circuit.setDevice ${deviceId} ${key}=${String(paramValue)}`)
      },

      removeDevice: async (deviceId) => {
        const result = await send('circuit.removeDevice', { device_id: deviceId })
        if (result) {
          setSelection(null)
          await afterMutation(`circuit.removeDevice ${deviceId}`)
        }
      },

      run: async () => {
        const result = await send<{ state: string }>('sim.run')
        if (result) {
          await refreshSim()
          pushLog('cmd', 'sim.run')
        }
      },

      pause: async () => {
        const result = await send('sim.pause')
        if (result) {
          await refreshSim()
          await refreshFrame()
          pushLog('cmd', 'sim.pause')
        }
      },

      resume: async () => {
        const result = await send('sim.resume')
        if (result) {
          await refreshSim()
          pushLog('cmd', 'sim.resume')
        }
      },

      stop: async () => {
        const result = await send('sim.stop')
        if (result) {
          await refreshSim()
          await refreshFrame()
          pushLog('cmd', 'sim.stop')
        }
      },

      reset: async () => {
        const result = await send('sim.reset')
        if (result) {
          await refreshSim()
          await refreshFrame()
          pushLog('cmd', 'sim.reset')
        }
      },

      step: async () => {
        const result = await send('sim.step', { ns: 1_000_000 })
        if (result) {
          await refreshSim()
          await refreshFrame()
          pushLog('cmd', 'sim.step 1 ms')
        }
      },

      setSpeed: async (factor) => {
        const result = await send<{ speed: number }>('sim.setSpeed', { factor })
        if (result) {
          await refreshSim()
          pushLog('cmd', `sim.setSpeed ${factor}x`)
        }
      },

      setPin: async (boardId, pin, level) => {
        const result = await send('sim.debug.setPin', {
          target: { board: boardId, pin },
          level,
        })
        if (result) {
          await refreshFrame()
          pushLog('cmd', `sim.debug.setPin ${pin}=${level}`)
        }
      },

      releasePin: async (boardId, pin) => {
        const result = await send('sim.debug.releasePin', { target: { board: boardId, pin } })
        if (result) {
          await refreshFrame()
          pushLog('cmd', `sim.debug.releasePin ${pin}`)
        }
      },
    }),
    [
      document,
      revision,
      dirty,
      info,
      catalog,
      diagnostics,
      sim,
      frame,
      selection,
      log,
      ready,
      source,
      bridge,
      send,
      afterMutation,
      pushLog,
      refreshFrame,
      refreshSim,
    ],
  )

  return <WorkspaceContext.Provider value={value}>{children}</WorkspaceContext.Provider>
}

export function useWorkspace(): WorkspaceValue {
  const value = useContext(WorkspaceContext)
  if (!value) throw new Error('useWorkspace fora do WorkspaceProvider')
  return value
}
