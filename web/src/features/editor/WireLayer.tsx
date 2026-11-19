import type { Net, SimFrame, SforgeDocument } from '../../lib/api/types'
import { boardAnchor, deviceAnchor, routeWire } from '../../lib/render/geometry'
import type { Anchor } from '../../lib/render/geometry'
import { levelsByNet, type NetLevel } from '../../lib/render/levels'
import type { Selection } from '../../stores/workspace'

const WIRE_STROKE: Record<NetLevel, string> = {
  high: '#3f9c6c',
  low: '#46525f',
  z: '#39434f',
  x: '#b04a5a',
}

interface WireLayerProps {
  document: SforgeDocument
  frame: SimFrame | null
  selection: Selection
  interactiveNet: string | null
  onHoverNet: (netId: string | null) => void
}

function anchorsOf(net: Net, document: SforgeDocument): Anchor[] {
  const boards = new Map(document.boards.map((board) => [board.id, board]))
  const devices = new Map(document.devices.map((device) => [device.id, device]))
  const anchors: Anchor[] = []
  for (const ref of net.pins) {
    if (ref.board) {
      const board = boards.get(ref.board)
      const anchor = board ? boardAnchor(board, ref.pin) : null
      if (anchor) anchors.push(anchor)
      continue
    }
    if (ref.device) {
      const device = devices.get(ref.device)
      const anchor = device ? deviceAnchor(device, ref.pin) : null
      if (anchor) anchors.push(anchor)
    }
  }
  return anchors
}

export function WireLayer({
  document,
  frame,
  selection,
  interactiveNet,
  onHoverNet,
}: WireLayerProps) {
  const levels = levelsByNet(frame?.nets ?? [])

  return (
    <g>
      {document.nets.map((net) => {
        const anchors = anchorsOf(net, document)
        if (anchors.length < 2) return null
        const level = levels.get(net.id) ?? 'z'
        const stroke = WIRE_STROKE[level]
        const touchesSelection =
          selection?.kind === 'device' &&
          net.pins.some((pin) => pin.device === selection.id)
        const highlighted = touchesSelection || interactiveNet === net.id

        const origin = anchors[0]
        return (
          <g
            key={net.id}
            onPointerEnter={() => onHoverNet(net.id)}
            onPointerLeave={() => onHoverNet(null)}
          >
            {anchors.slice(1).map((target, index) => (
              <path
                key={`${net.id}-${index}`}
                d={routeWire(origin, target)}
                fill="none"
                stroke={stroke}
                strokeWidth={highlighted ? 2.6 : 1.8}
                strokeLinecap="round"
                opacity={highlighted ? 1 : 0.85}
                style={{ transition: 'stroke-width 120ms ease' }}
              />
            ))}
            {highlighted
              ? anchors.slice(1).map((target, index) => {
                  const mid = {
                    x: (origin.x + target.x) / 2,
                    y: (origin.y + target.y) / 2,
                  }
                  return (
                    <g key={`${net.id}-chip-${index}`}>
                      <rect
                        x={mid.x - (net.name.length * 3.2 + 8)}
                        y={mid.y - 8}
                        width={net.name.length * 6.4 + 16}
                        height={15}
                        rx={4}
                        fill="#0b141d"
                        stroke="#1d2836"
                      />
                      <text
                        x={mid.x}
                        y={mid.y + 3}
                        textAnchor="middle"
                        fontSize={8.5}
                        fill="#8fa2b7"
                        fontFamily="ui-monospace, monospace"
                      >
                        {net.name}
                      </text>
                    </g>
                  )
                })
              : null}
          </g>
        )
      })}
    </g>
  )
}
