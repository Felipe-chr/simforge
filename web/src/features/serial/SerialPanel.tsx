import { useEffect, useRef, useState } from 'react'

const BOOT_LINES = [
  'ets Jul 29 2019 12:21:46',
  '',
  'rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)',
  'configsip: 0, SPIWP:0xee',
  'mode:DIO, clock div:1',
  'entry 0x403c88ac',
  '[    12][I][esp32-hal-uart.c:114] uartBegin(): UART0 baud(115200)',
  'SimForge ESP32-S3 DevKitC-1 · blink-led',
  'LED em GPIO2, botão em GPIO0',
  'loop: led=1 t=0 ms',
  'loop: led=0 t=500 ms',
]

export function SerialPanel() {
  const [lines, setLines] = useState<string[]>(BOOT_LINES)
  const [draft, setDraft] = useState('')
  const bodyRef = useRef<HTMLDivElement | null>(null)

  useEffect(() => {
    const node = bodyRef.current
    if (node) node.scrollTop = node.scrollHeight
  }, [lines])

  return (
    <div className="flex h-full flex-col">
      <div className="flex items-center gap-2 border-b border-line px-3 py-1.5">
        <span className="font-mono text-[10px] text-ink-dim">/dev/ttyUSB0</span>
        <span className="font-mono text-[10px] text-ink-dim">115200 8N1</span>
        <button
          type="button"
          onClick={() => setLines([])}
          className="ml-auto rounded px-1.5 py-0.5 text-[10px] text-ink-dim hover:bg-shell-800 hover:text-ink-muted"
        >
          limpar
        </button>
      </div>
      <div ref={bodyRef} className="scroll-slim min-h-0 flex-1 overflow-y-auto px-3 py-2 font-mono text-[11px] leading-5">
        {lines.length === 0 ? (
          <p className="text-ink-dim">monitor vazio</p>
        ) : (
          lines.map((line, index) => (
            <p key={`${index}-${line}`} className="whitespace-pre text-ink-muted">
              <span className="mr-2 inline-block w-5 text-right text-ink-dim">{index + 1}</span>
              {line}
            </p>
          ))
        )}
      </div>
      <div className="border-t border-line px-2 py-1.5">
        <input
          value={draft}
          onChange={(event) => setDraft(event.target.value)}
          onKeyDown={(event) => {
            if (event.key !== 'Enter' || draft.trim().length === 0) return
            setLines((current) => [...current, `> ${draft}`])
            setDraft('')
          }}
          placeholder="enviar para a serial (firmware.serial.write)"
          className="h-7 w-full rounded-md bg-shell-900 px-2 font-mono text-[11px] text-ink ring-1 ring-inset ring-line placeholder:text-ink-dim focus:ring-signal/60 focus:outline-none"
        />
      </div>
    </div>
  )
}
