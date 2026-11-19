import { useEffect, useState } from 'react'

import { TrashIcon } from '../../components/icons'
import { Button, Chip, EmptyState, Field, Tabs, cx } from '../../components/ui'
import { shortId } from '../../lib/format'
import { levelLabel } from '../../lib/render/levels'
import { useWorkspace } from '../../stores/workspace'

const LED_COLORS = ['red', 'green', 'blue', 'amber', 'white'] as const
const ROTATIONS = [0, 90, 180, 270]

function TextInput({
  value,
  onCommit,
  placeholder,
}: {
  value: string
  onCommit: (next: string) => void
  placeholder?: string
}) {
  const [draft, setDraft] = useState(value)
  useEffect(() => setDraft(value), [value])
  return (
    <input
      value={draft}
      placeholder={placeholder}
      onChange={(event) => setDraft(event.target.value)}
      onBlur={() => {
        if (draft !== value) onCommit(draft)
      }}
      onKeyDown={(event) => {
        if (event.key === 'Enter') event.currentTarget.blur()
        if (event.key === 'Escape') {
          setDraft(value)
          event.currentTarget.blur()
        }
      }}
      className="h-7 w-36 rounded-md bg-shell-900 px-2 font-mono text-[11px] text-ink ring-1 ring-inset ring-line focus:ring-signal/60 focus:outline-none"
    />
  )
}

function PropertiesTab() {
  const {
    document,
    selection,
    rotateDevice,
    renameDevice,
    setDeviceParam,
    removeDevice,
  } = useWorkspace()

  if (!document || !selection) {
    return (
      <EmptyState
        title="Nada selecionado"
        detail="Selecione a placa ou um componente no canvas para ver e editar as propriedades."
      />
    )
  }

  if (selection.kind === 'board') {
    const board = document.boards.find((entry) => entry.id === selection.id)
    if (!board) return null
    const connected = Object.entries(board.pins ?? {}).filter(([, value]) => value.net)
    return (
      <div className="divide-y divide-line-soft">
        <Field label="tipo">
          <span className="font-mono text-[11px] text-ink-muted">ESP32-S3 DevKitC-1</span>
        </Field>
        <Field label="id">
          <span className="font-mono text-[11px] text-ink-muted">{shortId(board.id)}</span>
        </Field>
        <Field label="mcu">
          <span className="font-mono text-[11px] text-ink-muted">ESP32-S3-WROOM-1</span>
        </Field>
        <Field label="pinos ligados">
          <Chip tone="signal">{connected.length}</Chip>
        </Field>
        <div className="px-3 py-2">
          <p className="pb-2 text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
            nets
          </p>
          <ul className="flex flex-col gap-1">
            {connected.map(([pin, value]) => (
              <li key={pin} className="flex items-center justify-between font-mono text-[11px]">
                <span className="text-ink-muted">{pin}</span>
                <span className="text-signal">{value.net}</span>
              </li>
            ))}
            {connected.length === 0 ? (
              <li className="text-[11px] text-ink-dim">nenhum pino conectado</li>
            ) : null}
          </ul>
        </div>
      </div>
    )
  }

  const device = document.devices.find((entry) => entry.id === selection.id)
  if (!device) return null
  const color = String(device.params.color ?? 'red')

  return (
    <div className="divide-y divide-line-soft">
      <div className="flex items-center justify-between px-3 py-2">
        <div className="min-w-0">
          <p className="truncate text-[12px] font-medium text-ink">
            {device.label ?? 'componente'}
          </p>
          <p className="truncate font-mono text-[10px] text-ink-dim">{device.type}</p>
        </div>
        <Button
          variant="ghost"
          tone="danger"
          onClick={() => void removeDevice(device.id)}
          className="h-7 px-2"
        >
          <TrashIcon className="size-3.5" />
        </Button>
      </div>

      <Field label="rótulo">
        <TextInput
          value={device.label ?? ''}
          placeholder="D1"
          onCommit={(next) => void renameDevice(device.id, next)}
        />
      </Field>

      <Field label="id">
        <span className="font-mono text-[11px] text-ink-muted">{shortId(device.id)}</span>
      </Field>

      <Field label="posição">
        <span className="font-mono text-[11px] text-ink-muted">
          {Math.round(device.position.x)}, {Math.round(device.position.y)}
        </span>
      </Field>

      <div className="px-3 py-2">
        <p className="pb-1.5 text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
          rotação
        </p>
        <div className="grid grid-cols-4 gap-1">
          {ROTATIONS.map((rotation) => (
            <button
              key={rotation}
              type="button"
              onClick={() => void rotateDevice(device.id, rotation)}
              className={cx(
                'h-7 rounded-md font-mono text-[11px] ring-1 ring-inset transition-colors',
                device.rotation === rotation
                  ? 'bg-signal/12 text-signal ring-signal/40'
                  : 'text-ink-muted ring-line hover:bg-shell-800',
              )}
            >
              {rotation}°
            </button>
          ))}
        </div>
      </div>

      {device.type === 'simforge.led' ? (
        <div className="px-3 py-2">
          <p className="pb-1.5 text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
            cor
          </p>
          <div className="flex flex-wrap gap-1.5">
            {LED_COLORS.map((option) => (
              <button
                key={option}
                type="button"
                onClick={() => void setDeviceParam(device.id, 'color', option)}
                className={cx(
                  'h-7 rounded-md px-2 text-[11px] ring-1 ring-inset transition-colors',
                  color === option
                    ? 'bg-signal/12 text-signal ring-signal/40'
                    : 'text-ink-muted ring-line hover:bg-shell-800',
                )}
              >
                {option}
              </button>
            ))}
          </div>
        </div>
      ) : null}

      {device.type === 'simforge.resistor' ? (
        <Field label="resistência">
          <span className="font-mono text-[11px] text-ink-muted">
            {Number(device.params.ohms ?? 1000)} Ω
          </span>
        </Field>
      ) : null}

      <div className="px-3 py-2">
        <p className="pb-1.5 text-[10px] font-semibold tracking-widest text-ink-dim uppercase">
          pinos
        </p>
        <ul className="flex flex-col gap-1">
          {Object.entries(device.pins ?? {}).map(([pin, binding]) => (
            <li key={pin} className="flex items-center justify-between font-mono text-[11px]">
              <span className="text-ink-muted">{pin}</span>
              <span className={binding.net ? 'text-signal' : 'text-ink-dim'}>
                {binding.net ?? 'livre'}
              </span>
            </li>
          ))}
          {Object.keys(device.pins ?? {}).length === 0 ? (
            <li className="text-[11px] text-ink-dim">sem conexões</li>
          ) : null}
        </ul>
      </div>
    </div>
  )
}

function ErcTab() {
  const { diagnostics } = useWorkspace()
  const checks = [
    { code: 'E_SHORT', label: 'curto entre nets' },
    { code: 'W_DANGLING_NET', label: 'nets sem destino' },
    { code: 'E_UNKNOWN_OWNER', label: 'referências quebradas' },
    { code: 'W_UNKNOWN_TYPE', label: 'tipos não registrados' },
  ]
  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b border-line px-3 py-2">
        <span className="text-[11px] text-ink-muted">verificação elétrica</span>
        <Chip tone={diagnostics.length === 0 ? 'ok' : 'warn'}>
          {diagnostics.length === 0 ? 'limpo' : `${diagnostics.length} avisos`}
        </Chip>
      </div>
      <ul className="divide-y divide-line-soft">
        {checks.map((check) => (
          <li key={check.code} className="flex items-center justify-between px-3 py-2">
            <span className="text-[11px] text-ink-muted">{check.label}</span>
            <span className="font-mono text-[10px] text-ok">ok</span>
          </li>
        ))}
      </ul>
      {diagnostics.length > 0 ? (
        <ul className="scroll-slim min-h-0 flex-1 divide-y divide-line-soft overflow-y-auto">
          {diagnostics.map((diagnostic, index) => (
            <li key={`${diagnostic.code}-${index}`} className="px-3 py-2">
              <p className="font-mono text-[10px] text-warn">{diagnostic.code}</p>
              <p className="text-[11px] text-ink-muted">{diagnostic.message}</p>
            </li>
          ))}
        </ul>
      ) : null}
    </div>
  )
}

function NetsTab() {
  const { document, frame } = useWorkspace()
  if (!document) return null
  const levels = new Map((frame?.nets ?? []).map((net) => [net.id, net.level]))
  return (
    <ul className="divide-y divide-line-soft">
      {document.nets.map((net) => {
        const level = levels.get(net.id)
        return (
          <li key={net.id} className="flex items-center justify-between px-3 py-2">
            <span className="flex min-w-0 items-center gap-2">
              <span className="font-mono text-[11px] text-ink">{net.id}</span>
              <span className="truncate text-[11px] text-ink-dim">{net.name}</span>
            </span>
            <span className="flex items-center gap-2">
              <span className="font-mono text-[10px] text-ink-dim">{net.pins.length} pinos</span>
              <Chip tone={level === 'high' ? 'ok' : level === 'x' ? 'err' : 'neutral'}>
                {level ? levelLabel(level) : '—'}
              </Chip>
            </span>
          </li>
        )
      })}
    </ul>
  )
}

function BenchTab() {
  const { document, frame, sim, setPin, releasePin } = useWorkspace()
  if (!document) return null
  const levels = new Map((frame?.nets ?? []).map((net) => [net.id, net.level]))
  const boards = document.boards

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center justify-between border-b border-line px-3 py-2">
        <span className="text-[11px] text-ink-muted">estímulo elétrico direto</span>
        <Chip tone={sim.state === 'error' ? 'err' : sim.state === 'idle' ? 'warn' : 'ok'}>
          {sim.state}
        </Chip>
      </div>

      <div className="scroll-slim min-h-0 flex-1 divide-y divide-line-soft overflow-y-auto">
        {boards.length === 0 ? (
          <p className="px-3 py-6 text-center text-[11px] text-ink-dim">nenhuma placa no projeto</p>
        ) : null}

        {boards.map((board) => {
          const pins = Object.entries(board.pins ?? {}).filter(([, binding]) => binding.net)
          return (
            <div key={board.id} className="px-3 py-2">
              <p className="flex items-center justify-between pb-2">
                <span className="text-[11px] font-medium text-ink">
                  {board.label ?? board.id.slice(0, 6)}
                </span>
                <span className="font-mono text-[10px] text-ink-dim">
                  {board.type.split('.').at(-1)}
                </span>
              </p>

              {pins.length === 0 ? (
                <p className="text-[11px] text-ink-dim">nenhum pino ligado a net</p>
              ) : null}

              <ul className="flex flex-col gap-1.5">
                {pins.map(([pin, binding]) => {
                  const netId = binding.net ?? ''
                  const level = levels.get(netId)
                  return (
                    <li key={pin} className="flex items-center gap-2">
                      <span className="w-14 shrink-0 font-mono text-[10px] text-ink">{pin}</span>
                      <span className="min-w-0 flex-1 truncate font-mono text-[10px] text-ink-dim">
                        {netId}
                      </span>
                      <span
                        className={cx(
                          'size-1.5 rounded-full',
                          level === 'high'
                            ? 'bg-ok'
                            : level === 'x'
                              ? 'bg-err'
                              : level === 'low'
                                ? 'bg-ink-dim'
                                : 'bg-shell-700',
                        )}
                        title={level ?? 'desconhecido'}
                      />
                      <div className="flex items-center gap-0.5 rounded-md ring-1 ring-line">
                        {(['low', 'high', 'z'] as const).map((option) => (
                          <button
                            key={option}
                            type="button"
                            onClick={() =>
                              option === 'z'
                                ? void releasePin(board.id, pin)
                                : void setPin(board.id, pin, option)
                            }
                            title={option === 'z' ? 'liberar (alta-Z)' : `forçar ${option}`}
                            className={cx(
                              'h-6 w-6 rounded font-mono text-[10px] transition-colors',
                              level === option
                                ? 'bg-signal/15 text-signal'
                                : 'text-ink-dim hover:bg-shell-800 hover:text-ink-muted',
                            )}
                          >
                            {option === 'low' ? 'L' : option === 'high' ? 'H' : 'Z'}
                          </button>
                        ))}
                      </div>
                    </li>
                  )
                })}
              </ul>
            </div>
          )
        })}
      </div>

      <p className="border-t border-line px-3 py-2 text-[10px] leading-relaxed text-ink-dim">
        L = low · H = high · Z = liberar. Enviado como sim.debug.setPin/releasePin para o
        transporte ativo.
      </p>
    </div>
  )
}

export function Inspector() {
  const [tab, setTab] = useState('props')
  return (
    <div className="flex h-full flex-col">
      <div className="border-b border-line px-2 py-1">
        <Tabs
          active={tab}
          onChange={setTab}
          items={[
            { id: 'props', label: 'Propriedades' },
            { id: 'erc', label: 'ERC' },
            { id: 'nets', label: 'Nets' },
            { id: 'bench', label: 'Bancada' },
          ]}
        />
      </div>
      <div className="scroll-slim min-h-0 flex-1 overflow-y-auto">
        {tab === 'props' ? <PropertiesTab /> : null}
        {tab === 'erc' ? <ErcTab /> : null}
        {tab === 'nets' ? <NetsTab /> : null}
        {tab === 'bench' ? <BenchTab /> : null}
      </div>
    </div>
  )
}
