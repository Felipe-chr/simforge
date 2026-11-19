import { useMemo } from 'react'

import { useWorkspace } from '../../stores/workspace'

const WIDTH = 900
const HEIGHT = 160

function sample(channel: number, index: number, points: number): number {
  const t = (index / points) * 4
  if (channel === 0) {
    // onda quadrada de 2 Hz (GPIO2)
    return t % 1 < 0.5 ? 0.22 : 0.78
  }
  if (channel === 1) {
    // ruído suave (leitura analógica)
    return 0.5 + 0.22 * Math.sin(t * 6.1) + 0.06 * Math.sin(t * 23)
  }
  return 0.5
}

function tracePath(channel: number, points = 240): string {
  const step = WIDTH / points
  const commands: string[] = []
  for (let index = 0; index <= points; index += 1) {
    const value = sample(channel, index, points)
    const x = index * step
    const y = HEIGHT - 8 - value * (HEIGHT - 16)
    commands.push(`${index === 0 ? 'M' : 'L'} ${x.toFixed(1)} ${y.toFixed(1)}`)
  }
  return commands.join(' ')
}

export function ScopePanel() {
  const { frame, sim } = useWorkspace()
  const nets = frame?.nets ?? []
  const paths = useMemo(() => [tracePath(0), tracePath(1)], [])
  const running = sim.state === 'running'

  return (
    <div className="flex h-full">
      <div className="flex w-44 shrink-0 flex-col gap-1 border-r border-line px-3 py-2">
        <p className="text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
          canais
        </p>
        <div className="flex items-center gap-2 rounded-md bg-shell-900 px-2 py-1.5 ring-1 ring-inset ring-line">
          <span className="size-2 rounded-full bg-emerald-400" />
          <span className="font-mono text-[10px] text-ink-muted">CH1 · GPIO2</span>
        </div>
        <div className="flex items-center gap-2 rounded-md bg-shell-900 px-2 py-1.5 ring-1 ring-inset ring-line">
          <span className="size-2 rounded-full bg-sky-400" />
          <span className="font-mono text-[10px] text-ink-muted">CH2 · ADC1</span>
        </div>
        <p className="pt-2 font-mono text-[10px] text-ink-dim">
          1 V/div · 2 ms/div
        </p>
        <p className="font-mono text-[10px] text-ink-dim">
          {running ? 'aquisição ativa' : 'parado'}
        </p>
      </div>

      <div className="relative min-w-0 flex-1 bg-shell-950">
        <svg viewBox={`0 0 ${WIDTH} ${HEIGHT}`} className="h-full w-full" preserveAspectRatio="none">
          <defs>
            <pattern id="scopeGrid" width="36" height="32" patternUnits="userSpaceOnUse">
              <path d="M 36 0 L 0 0 0 32" fill="none" stroke="#141e29" strokeWidth="1" />
            </pattern>
          </defs>
          <rect width={WIDTH} height={HEIGHT} fill="#080d13" />
          <rect width={WIDTH} height={HEIGHT} fill="url(#scopeGrid)" />
          <line x1="0" y1={HEIGHT / 2} x2={WIDTH} y2={HEIGHT / 2} stroke="#1b2634" />
          <path d={paths[1]} fill="none" stroke="#38bdf8" strokeWidth="1.2" opacity="0.75" />
          <path d={paths[0]} fill="none" stroke="#34d399" strokeWidth="1.6" />
        </svg>
        <div className="pointer-events-none absolute top-2 right-3 flex gap-3 font-mono text-[10px] text-ink-dim">
          <span>
            t = {((frame?.t_ns ?? 0) / 1e6).toFixed(1)} ms
          </span>
          <span>{nets.length} nets</span>
        </div>
      </div>
    </div>
  )
}
