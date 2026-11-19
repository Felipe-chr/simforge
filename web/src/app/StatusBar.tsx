import { formatVirtualTime } from '../lib/format'
import { useWorkspace } from '../stores/workspace'

export function StatusBar() {
  const { document, sim, info, frame, diagnostics, source, bridge } = useWorkspace()

  const boards = document?.boards.length ?? 0
  const devices = document?.devices.length ?? 0
  const nets = document?.nets.length ?? 0
  const project = bridge?.project ? bridge.project.split(/[\\/]/).pop() : null

  return (
    <footer className="flex h-6 shrink-0 items-center gap-4 border-t border-line bg-shell-900 px-3 font-mono text-[10px] text-ink-dim">
      <span className="text-ink-muted">{sim.state}</span>
      <span>t {formatVirtualTime(sim.t_ns)}</span>
      <span>{sim.speed}x</span>
      <span>{boards} placa</span>
      <span>{devices} disp.</span>
      <span>{nets} nets</span>
      <span className={diagnostics.length === 0 ? 'text-ok' : 'text-warn'}>
        ERC {diagnostics.length === 0 ? 'limpo' : `${diagnostics.length}`}
      </span>
      <span className="ml-auto">
        frame seq {frame?.seq ?? 0}
      </span>
      <span className={source === 'core' ? 'text-ok' : 'text-warn'}>
        {source === 'core' ? 'núcleo C++' : 'mock'}
      </span>
      {project ? <span className="truncate">{project}</span> : null}
      <span>{info?.platform ?? '—'}</span>
      <span>{info ? `engine ${info.engine_version}` : ''}</span>
    </footer>
  )
}
