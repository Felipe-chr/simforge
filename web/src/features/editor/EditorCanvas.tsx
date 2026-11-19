import { useMemo, useRef, useState } from 'react'
import type { PointerEvent as ReactPointerEvent } from 'react'

import { IconButton } from '../../components/ui'
import { ZoomInIcon, ZoomOutIcon } from '../../components/icons'
import { snapToGrid } from '../../lib/render/geometry'
import { useWorkspace } from '../../stores/workspace'
import { BoardNode } from './BoardNode'
import { DeviceNode } from './DeviceNode'
import { WireLayer } from './WireLayer'

interface DragState {
  id: string
  pointerId: number
  start: { x: number; y: number }
  origin: { x: number; y: number }
  position: { x: number; y: number }
}

const ZOOM_STEPS = [0.75, 0.9, 1, 1.15, 1.35]

export function EditorCanvas() {
  const { document, frame, selection, select, moveDevice } = useWorkspace()
  const svgRef = useRef<SVGSVGElement | null>(null)
  const [zoomIndex, setZoomIndex] = useState(2)
  const [drag, setDrag] = useState<DragState | null>(null)
  const [hoverNet, setHoverNet] = useState<string | null>(null)

  const runtimes = useMemo(
    () => new Map((frame?.devices ?? []).map((device) => [device.id, device])),
    [frame],
  )

  const pointOf = (event: ReactPointerEvent): { x: number; y: number } | null => {
    const svg = svgRef.current
    if (!svg) return null
    const matrix = svg.getScreenCTM()
    if (!matrix) return null
    const point = svg.createSVGPoint()
    point.x = event.clientX
    point.y = event.clientY
    const world = point.matrixTransform(matrix.inverse())
    return { x: world.x, y: world.y }
  }

  if (!document) {
    return (
      <div className="grid-canvas flex h-full items-center justify-center">
        <p className="text-[12px] text-ink-dim">abrindo projeto…</p>
      </div>
    )
  }

  const zoom = ZOOM_STEPS[zoomIndex] ?? 1

  return (
    <div className="relative h-full overflow-hidden">
      <svg
        ref={svgRef}
        viewBox={`${(980 - 980 / zoom) / 2} ${(640 - 640 / zoom) / 2} ${980 / zoom} ${640 / zoom}`}
        className="grid-canvas h-full w-full touch-none"
        onPointerMove={(event) => {
          if (!drag) return
          const point = pointOf(event)
          if (!point) return
          setDrag((current) =>
            current
              ? {
                  ...current,
                  position: snapToGrid({
                    x: current.origin.x + (point.x - current.start.x),
                    y: current.origin.y + (point.y - current.start.y),
                  }),
                }
              : current,
          )
        }}
        onPointerUp={() => {
          if (!drag) return
          void moveDevice(drag.id, drag.position)
          setDrag(null)
        }}
        onPointerCancel={() => setDrag(null)}
        onClick={() => select(null)}
      >
        <defs>
          <filter id="ledGlow" x="-80%" y="-80%" width="260%" height="260%">
            <feGaussianBlur stdDeviation="9" />
          </filter>
          <linearGradient id="boardSheen" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stopColor="#16222e" />
            <stop offset="100%" stopColor="#0c151d" />
          </linearGradient>
        </defs>

        <WireLayer
          document={document}
          frame={frame}
          selection={selection}
          interactiveNet={hoverNet}
          onHoverNet={setHoverNet}
        />

        {document.boards.map((board) => (
          <BoardNode
            key={board.id}
            board={board}
            frame={frame}
            selected={selection?.kind === 'board' && selection.id === board.id}
            onSelect={() => select({ kind: 'board', id: board.id })}
          />
        ))}

        {document.devices.map((device) => {
          const position = drag?.id === device.id ? drag.position : device.position
          return (
            <DeviceNode
              key={device.id}
              device={{ ...device, position }}
              runtime={runtimes.get(device.id)}
              selected={selection?.kind === 'device' && selection.id === device.id}
              dragging={drag?.id === device.id}
              onSelect={() => select({ kind: 'device', id: device.id })}
              onPointerDown={(event) => {
                const point = pointOf(event)
                if (!point) return
                event.stopPropagation()
                try {
                  svgRef.current?.setPointerCapture(event.pointerId)
                } catch {
                  // sintético ou ponteiro já liberado: arrasto continua sem captura
                }
                select({ kind: 'device', id: device.id })
                setDrag({
                  id: device.id,
                  pointerId: event.pointerId,
                  start: point,
                  origin: device.position,
                  position: device.position,
                })
              }}
            />
          )
        })}
      </svg>

      <div className="pointer-events-none absolute inset-x-0 bottom-3 flex items-center justify-between px-3">
        <div className="pointer-events-auto flex items-center gap-1 rounded-lg border border-line bg-shell-900/90 px-1 py-1 backdrop-blur">
          <IconButton
            label="Diminuir zoom"
            onClick={() => setZoomIndex((index) => Math.max(0, index - 1))}
          >
            <ZoomOutIcon />
          </IconButton>
          <span className="w-12 text-center font-mono text-[11px] text-ink-muted">
            {Math.round(zoom * 100)}%
          </span>
          <IconButton
            label="Aumentar zoom"
            onClick={() =>
              setZoomIndex((index) => Math.min(ZOOM_STEPS.length - 1, index + 1))
            }
          >
            <ZoomInIcon />
          </IconButton>
        </div>

        <div className="pointer-events-auto rounded-lg border border-line bg-shell-900/90 px-2.5 py-1.5 font-mono text-[10px] text-ink-dim backdrop-blur">
          grid 1.27 mm · {document.boards.length} placa · {document.devices.length}{' '}
          dispositivos · {document.nets.length} nets
        </div>
      </div>

      <div className="pointer-events-none absolute top-3 left-3 flex flex-col gap-1">
        <span className="rounded-md border border-line bg-shell-900/85 px-2 py-1 text-[10px] text-ink-dim backdrop-blur">
          arraste para mover · clique para selecionar
        </span>
      </div>
    </div>
  )
}
