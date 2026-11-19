import { useMemo, useState } from 'react'

import { DEVICE_ICONS, PlusIcon, SearchIcon } from '../../components/icons'
import { Chip, cx } from '../../components/ui'
import { useWorkspace } from '../../stores/workspace'

const CATEGORY_LABEL: Record<string, string> = {
  component: 'Componentes',
  entrada: 'Entrada',
  atuador: 'Atuador',
  passivo: 'Passivo',
  display: 'Display',
}

const CATEGORY_ORDER = ['component', 'entrada', 'atuador', 'passivo', 'display']

export function ComponentLibrary() {
  const { catalog, addDevice } = useWorkspace()
  const [query, setQuery] = useState('')

  const groups = useMemo(() => {
    const term = query.trim().toLowerCase()
    const filtered = catalog.filter(
      (item) =>
        !term ||
        item.name.toLowerCase().includes(term) ||
        item.type.toLowerCase().includes(term),
    )
    const map = new Map<string, typeof filtered>()
    for (const item of filtered) {
      const list = map.get(item.category) ?? []
      list.push(item)
      map.set(item.category, list)
    }
    const known = CATEGORY_ORDER.filter((category) => map.has(category))
    const extra = [...map.keys()].filter((category) => !CATEGORY_ORDER.includes(category))
    return [...known, ...extra].map((category) => ({
      category,
      items: map.get(category) ?? [],
    }))
  }, [catalog, query])

  return (
    <div className="flex h-full flex-col">
      <div className="border-b border-line p-2">
        <div className="flex items-center gap-2 rounded-md bg-shell-900 px-2 ring-1 ring-inset ring-line">
          <SearchIcon className="size-3.5 text-ink-dim" />
          <input
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            placeholder="buscar componente"
            className="h-8 w-full bg-transparent text-[12px] text-ink placeholder:text-ink-dim focus:outline-none"
          />
        </div>
      </div>

      <div className="scroll-slim min-h-0 flex-1 overflow-y-auto px-2 py-2">
        {groups.length === 0 ? (
          <p className="px-2 py-6 text-center text-[11px] text-ink-dim">
            nenhum componente encontrado
          </p>
        ) : null}

        {groups.map((group) => (
          <div key={group.category} className="mb-3">
            <p className="px-1 pb-1 text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
              {CATEGORY_LABEL[group.category] ?? group.category}
            </p>
            <ul className="flex flex-col gap-0.5">
              {group.items.map((item) => {
                const Icon = DEVICE_ICONS[item.type]
                return (
                  <li key={item.type}>
                    <button
                      type="button"
                      onClick={() => void addDevice(item.type)}
                      title={`Inserir ${item.name}`}
                      className={cx(
                        'group flex w-full items-center gap-2 rounded-md px-2 py-1.5 text-left transition-colors',
                        'hover:bg-shell-800',
                      )}
                    >
                      <span className="flex size-7 items-center justify-center rounded-md bg-shell-900 text-ink-muted ring-1 ring-inset ring-line group-hover:text-signal">
                        {Icon ? <Icon className="size-4" /> : null}
                      </span>
                      <span className="min-w-0 flex-1">
                        <span className="block truncate text-[12px] text-ink">{item.name}</span>
                        <span className="block truncate font-mono text-[10px] text-ink-dim">
                          {item.pins.length} pinos
                        </span>
                      </span>
                      <PlusIcon className="size-3.5 text-ink-dim opacity-0 transition-opacity group-hover:opacity-100" />
                    </button>
                  </li>
                )
              })}
            </ul>
          </div>
        ))}
      </div>

      <div className="border-t border-line px-3 py-2">
        <Chip tone="signal">clique para inserir</Chip>
      </div>
    </div>
  )
}
