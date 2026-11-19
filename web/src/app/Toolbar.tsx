import { Button, Chip, cx } from '../components/ui'
import { formatBytes } from '../lib/format'
import { useWorkspace } from '../stores/workspace'

function BrandMark() {
  return (
    <span className="flex size-7 items-center justify-center rounded-md bg-gradient-to-br from-sky-400 to-cyan-600 text-[13px] font-bold text-shell-950 shadow-[0_0_18px_rgba(56,189,248,0.35)]">
      S
    </span>
  )
}

export function Toolbar() {
  const { document, revision, dirty, info, frame, source } = useWorkspace()
  const payload = frame ? JSON.stringify(frame).length : 0

  return (
    <header className="flex h-12 shrink-0 items-center gap-3 border-b border-line bg-shell-900 px-3">
      <div className="flex items-center gap-2">
        <BrandMark />
        <div className="leading-tight">
          <p className="text-[12px] font-semibold text-ink">SimForge</p>
          <p className="font-mono text-[9px] tracking-wider text-ink-dim">
            {info ? `core ${info.version}` : 'carregando'}
          </p>
        </div>
      </div>

      <div className="mx-1 h-6 w-px bg-line" />

      <nav className="flex items-center gap-1">
        <Button variant="ghost" className="h-7">
          Arquivo
        </Button>
        <Button variant="ghost" className="h-7">
          Editar
        </Button>
        <Button variant="ghost" className="h-7">
          Simular
        </Button>
        <Button variant="ghost" className="h-7">
          Mundo
        </Button>
        <Button variant="ghost" className="h-7">
          Ajuda
        </Button>
      </nav>

      <div className="ml-2 flex min-w-0 items-center gap-2 rounded-md bg-shell-850 px-2 py-1 ring-1 ring-inset ring-line">
        <span className="truncate text-[12px] text-ink">
          {document?.project.name ?? 'sem projeto'}
        </span>
        <span className="font-mono text-[10px] text-ink-dim">.sforge</span>
        <span
          className={cx(
            'size-1.5 rounded-full',
            dirty ? 'bg-warn' : 'bg-ok',
          )}
          title={dirty ? 'alterações não salvas' : 'salvo'}
        />
      </div>

      <Chip tone="neutral">rev {revision}</Chip>

      <div className="ml-auto flex items-center gap-2">
        <span className="hidden font-mono text-[10px] text-ink-dim lg:inline">
          frame {formatBytes(payload)}
        </span>
        <span className="hidden items-center gap-1.5 font-mono text-[10px] text-ink-dim xl:flex">
          <span className={cx('size-1.5 rounded-full', source === 'core' ? 'bg-ok' : 'bg-warn')} />
          api v1 · {source}
        </span>
        <Button variant="outline" className="h-7">
          Compilar
        </Button>
        <Button variant="solid" tone="signal" className="h-7">
          Gravar na placa
        </Button>
      </div>
    </header>
  )
}
