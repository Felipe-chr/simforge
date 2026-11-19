import { PauseIcon, PlayIcon, ResetIcon, StepIcon, StopIcon } from '../../components/icons'
import { Button, Chip, IconButton, cx } from '../../components/ui'
import { formatVirtualTime } from '../../lib/format'
import { useWorkspace } from '../../stores/workspace'

const SPEEDS = [0.25, 0.5, 1, 2, 4, 8]

export function SimControls() {
  const { sim, run, pause, resume, stop, reset, step, setSpeed, frame } = useWorkspace()
  const running = sim.state === 'running'
  const ledOn = Boolean(frame?.devices.find((device) => device.type === 'simforge.led')?.on)

  const stateTone =
    running ? 'ok' : sim.state === 'paused' ? 'warn' : sim.state === 'error' ? 'err' : 'neutral'

  return (
    <div className="flex h-11 shrink-0 items-center gap-2 border-b border-line bg-shell-900 px-3">
      <div className="flex items-center gap-0.5 rounded-lg bg-shell-850 p-0.5 ring-1 ring-inset ring-line">
        {running ? (
          <IconButton label="Pausar" onClick={() => void pause()} className="text-ok">
            <PauseIcon />
          </IconButton>
        ) : (
          <IconButton
            label="Executar"
            onClick={() => void (sim.state === 'paused' ? resume() : run())}
            className="text-ok"
          >
            <PlayIcon />
          </IconButton>
        )}
        <IconButton label="Parar" onClick={() => void stop()}>
          <StopIcon />
        </IconButton>
        <IconButton label="Passo de 1 ms" onClick={() => void step()}>
          <StepIcon />
        </IconButton>
        <IconButton label="Reiniciar" onClick={() => void reset()}>
          <ResetIcon />
        </IconButton>
      </div>

      <div className="flex items-baseline gap-2 rounded-lg bg-shell-850 px-3 py-1 ring-1 ring-inset ring-line">
        <span className="text-[10px] tracking-widest text-ink-dim uppercase">t</span>
        <span className="font-mono text-[13px] tabular-nums text-ink">
          {formatVirtualTime(sim.t_ns)}
        </span>
      </div>

      <div className="flex items-center gap-1">
        {SPEEDS.map((speed) => (
          <button
            key={speed}
            type="button"
            onClick={() => void setSpeed(speed)}
            className={cx(
              'h-7 rounded-md px-1.5 font-mono text-[11px] ring-1 ring-inset transition-colors',
              sim.speed === speed
                ? 'bg-signal/12 text-signal ring-signal/40'
                : 'text-ink-dim ring-transparent hover:bg-shell-800 hover:text-ink-muted',
            )}
          >
            {speed}x
          </button>
        ))}
      </div>

      <div className="ml-auto flex items-center gap-2">
        <span className="flex items-center gap-1.5 font-mono text-[10px] text-ink-dim">
          <span
            className={cx(
              'size-2 rounded-full',
              ledOn ? 'bg-red-500 shadow-[0_0_10px_2px_rgba(239,68,68,0.55)]' : 'bg-shell-700',
            )}
          />
          D1
        </span>
        <Chip tone={stateTone}>
          <span className={cx('size-1.5 rounded-full bg-current', running && 'animate-pulse-dot')} />
          {sim.state}
        </Chip>
        <Button
          variant="outline"
          onClick={() => void (sim.state === 'paused' ? resume() : run())}
          className="h-7"
        >
          {running ? 'ao vivo' : 'simular'}
        </Button>
      </div>
    </div>
  )
}
