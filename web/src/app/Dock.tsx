import { useState } from 'react'

import { Panel, Tabs } from '../components/ui'
import { CodePanel } from '../features/code/CodePanel'
import { ScopePanel } from '../features/instruments/ScopePanel'
import { SerialPanel } from '../features/serial/SerialPanel'
import { useWorkspace } from '../stores/workspace'

const TABS = [
  { id: 'console', label: 'Console' },
  { id: 'serial', label: 'Serial' },
  { id: 'code', label: 'Código' },
  { id: 'scope', label: 'Instrumentos' },
]

function ConsolePanel() {
  const { log } = useWorkspace()
  const tone: Record<string, string> = {
    cmd: 'text-sky-300',
    event: 'text-ink-muted',
    error: 'text-err',
    info: 'text-ink-dim',
  }
  return (
    <div className="scroll-slim h-full overflow-y-auto px-3 py-2 font-mono text-[11px] leading-5">
      {log.length === 0 ? <p className="text-ink-dim">sem atividade</p> : null}
      {log.map((line) => (
        <p key={line.id} className="flex gap-2">
          <span className="w-14 shrink-0 text-ink-dim">{line.kind}</span>
          <span className={tone[line.kind] ?? 'text-ink-muted'}>{line.text}</span>
        </p>
      ))}
    </div>
  )
}

export function Dock() {
  const [tab, setTab] = useState('console')
  const { log } = useWorkspace()

  return (
    <Panel className="h-[212px] shrink-0 border-t border-line" bodyClassName="min-h-0">
      <div className="flex h-9 shrink-0 items-center justify-between border-b border-line px-2">
        <Tabs items={TABS} active={tab} onChange={setTab} />
        <span className="pr-1 font-mono text-[10px] text-ink-dim">{log.length} linhas</span>
      </div>
      <div className="min-h-0 flex-1">
        {tab === 'console' ? <ConsolePanel /> : null}
        {tab === 'serial' ? <SerialPanel /> : null}
        {tab === 'code' ? <CodePanel /> : null}
        {tab === 'scope' ? <ScopePanel /> : null}
      </div>
    </Panel>
  )
}
