import type { ButtonHTMLAttributes, ReactNode } from 'react'

export function cx(...values: Array<string | false | null | undefined>): string {
  return values.filter(Boolean).join(' ')
}

type Tone = 'neutral' | 'signal' | 'ok' | 'warn' | 'err'

const TONE_CLASS: Record<Tone, string> = {
  neutral: 'bg-shell-700/70 text-ink-muted ring-line',
  signal: 'bg-signal/12 text-signal ring-signal/30',
  ok: 'bg-ok/12 text-ok ring-ok/30',
  warn: 'bg-warn/12 text-warn ring-warn/30',
  err: 'bg-err/12 text-err ring-err/30',
}

export function Chip({ tone = 'neutral', children }: { tone?: Tone; children: ReactNode }) {
  return (
    <span
      className={cx(
        'inline-flex items-center gap-1.5 rounded-full px-2 py-0.5 text-[11px] font-medium ring-1 ring-inset',
        TONE_CLASS[tone],
      )}
    >
      {children}
    </span>
  )
}

export function Dot({ tone = 'neutral' }: { tone?: Tone }) {
  const color: Record<Tone, string> = {
    neutral: 'bg-ink-dim',
    signal: 'bg-signal',
    ok: 'bg-ok',
    warn: 'bg-warn',
    err: 'bg-err',
  }
  return <span className={cx('size-1.5 rounded-full', color[tone])} />
}

interface ButtonProps extends ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: 'solid' | 'ghost' | 'outline'
  tone?: 'neutral' | 'signal' | 'danger'
}

export function Button({
  variant = 'ghost',
  tone = 'neutral',
  className,
  children,
  ...rest
}: ButtonProps) {
  const base =
    'inline-flex items-center justify-center gap-2 rounded-md text-[12px] font-medium transition-colors disabled:opacity-40 disabled:pointer-events-none'
  const variants = {
    solid: {
      neutral: 'bg-shell-700 text-ink hover:bg-shell-700/80 ring-1 ring-line',
      signal: 'bg-signal text-shell-950 hover:bg-sky-300',
      danger: 'bg-err/15 text-err hover:bg-err/25 ring-1 ring-err/30',
    },
    outline: {
      neutral: 'ring-1 ring-line text-ink-muted hover:text-ink hover:bg-shell-800',
      signal: 'ring-1 ring-signal/40 text-signal hover:bg-signal/10',
      danger: 'ring-1 ring-err/40 text-err hover:bg-err/10',
    },
    ghost: {
      neutral: 'text-ink-muted hover:text-ink hover:bg-shell-800',
      signal: 'text-signal hover:bg-signal/10',
      danger: 'text-err hover:bg-err/10',
    },
  } as const
  return (
    <button type="button" className={cx(base, 'h-8 px-3', variants[variant][tone], className)} {...rest}>
      {children}
    </button>
  )
}

export function IconButton({
  label,
  className,
  children,
  ...rest
}: ButtonHTMLAttributes<HTMLButtonElement> & { label: string }) {
  return (
    <button
      type="button"
      title={label}
      aria-label={label}
      className={cx(
        'inline-flex size-8 items-center justify-center rounded-md text-ink-muted transition-colors hover:bg-shell-800 hover:text-ink disabled:pointer-events-none disabled:opacity-40',
        className,
      )}
      {...rest}
    >
      {children}
    </button>
  )
}

export function Panel({
  title,
  actions,
  children,
  className,
  bodyClassName,
}: {
  title?: ReactNode
  actions?: ReactNode
  children: ReactNode
  className?: string
  bodyClassName?: string
}) {
  return (
    <section className={cx('flex min-h-0 flex-col bg-shell-850', className)}>
      {title ? (
        <header className="flex h-9 shrink-0 items-center justify-between border-b border-line px-3">
          <span className="text-[11px] font-semibold tracking-widest text-ink-dim uppercase">
            {title}
          </span>
          {actions}
        </header>
      ) : null}
      <div className={cx('min-h-0 flex-1', bodyClassName)}>{children}</div>
    </section>
  )
}

export interface TabItem {
  id: string
  label: string
  badge?: ReactNode
}

export function Tabs({
  items,
  active,
  onChange,
  className,
}: {
  items: TabItem[]
  active: string
  onChange: (id: string) => void
  className?: string
}) {
  return (
    <div className={cx('flex items-center gap-1', className)}>
      {items.map((item) => {
        const isActive = item.id === active
        return (
          <button
            key={item.id}
            type="button"
            onClick={() => onChange(item.id)}
            className={cx(
              'relative h-8 rounded-md px-2.5 text-[12px] font-medium transition-colors',
              isActive ? 'bg-shell-800 text-ink' : 'text-ink-dim hover:text-ink-muted',
            )}
          >
            {item.label}
            {item.badge !== undefined ? (
              <span className="ml-1.5 text-[10px] text-ink-dim">{item.badge}</span>
            ) : null}
          </button>
        )
      })}
    </div>
  )
}

export function Field({
  label,
  children,
  hint,
}: {
  label: string
  children: ReactNode
  hint?: string
}) {
  return (
    <label className="flex items-center justify-between gap-3 px-3 py-2">
      <span className="text-[11px] text-ink-dim">{label}</span>
      <span className="flex items-center gap-2">
        {children}
        {hint ? <span className="text-[10px] text-ink-dim">{hint}</span> : null}
      </span>
    </label>
  )
}

export function EmptyState({ title, detail }: { title: string; detail: string }) {
  return (
    <div className="flex h-full flex-col items-center justify-center gap-1 px-6 text-center">
      <p className="text-[12px] font-medium text-ink-muted">{title}</p>
      <p className="max-w-[26ch] text-[11px] leading-relaxed text-ink-dim">{detail}</p>
    </div>
  )
}
