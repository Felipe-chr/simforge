import type { BoardInstance, SimFrame } from '../../lib/api/types'
import { BOARD_PINS, BOARD_PIN_SPACING, BOARD_PIN_TOP } from '../../lib/render/board-pins'
import { boardBox } from '../../lib/render/geometry'
import type { NetLevel } from '../../lib/render/levels'
import { levelColor } from '../../lib/render/levels'

const PIN_TINT: Record<string, string> = {
  power: '#c98b4b',
  gnd: '#7d8fa3',
  io: '#5d7c99',
  adc: '#8f7fc4',
  usb: '#7c8ff0',
  boot: '#b07cd6',
}

interface BoardNodeProps {
  board: BoardInstance
  frame: SimFrame | null
  selected: boolean
  onSelect: () => void
}

export function BoardNode({ board, frame, selected, onSelect }: BoardNodeProps) {
  const box = boardBox(board)
  const levels = new Map<string, NetLevel>(
    (frame?.nets ?? []).map((net) => [net.id, net.level]),
  )

  const levelOfPin = (pinName: string): NetLevel | undefined => {
    const netId = board.pins?.[pinName]?.net
    if (!netId) return undefined
    return levels.get(netId)
  }

  const leftPins = BOARD_PINS.filter((pin) => pin.side === 'left')
  const rightPins = BOARD_PINS.filter((pin) => pin.side === 'right')

  return (
    <g onClick={onSelect} className="cursor-pointer">
      <rect
        x={box.x}
        y={box.y}
        width={box.width}
        height={box.height}
        rx={18}
        fill="#0e1822"
        stroke={selected ? '#38bdf8' : '#26374a'}
        strokeWidth={selected ? 1.6 : 1}
      />

      {/* módulo ESP32-S3 */}
      <g>
        <rect
          x={box.x + box.width / 2 - 88}
          y={box.y + 104}
          width={176}
          height={112}
          rx={8}
          fill="#16232f"
          stroke="#2b3f52"
        />
        <g stroke="#33475c" strokeWidth={2} fill="none">
          <path d={`M ${box.x + box.width / 2 - 34} ${box.y + 122} h 68`} />
          <path d={`M ${box.x + box.width / 2 - 28} ${box.y + 128} h 56`} />
        </g>
        <rect
          x={box.x + box.width / 2 - 42}
          y={box.y + 140}
          width={84}
          height={58}
          rx={4}
          fill="#1b2b39"
          stroke="#2b3f52"
        />
        <text
          x={box.x + box.width / 2}
          y={box.y + 165}
          textAnchor="middle"
          fontSize={12}
          fontWeight={600}
          fill="#c7d6e5"
          fontFamily="ui-monospace, monospace"
        >
          ESP32-S3
        </text>
        <text
          x={box.x + box.width / 2}
          y={box.y + 181}
          textAnchor="middle"
          fontSize={8}
          fill="#6f8398"
          letterSpacing={1.2}
        >
          WROOM-1 · N16R8
        </text>
      </g>

      {/* USB-C na borda inferior */}
      <g>
        <rect
          x={box.x + box.width / 2 - 22}
          y={box.y + box.height - 16}
          width={44}
          height={16}
          rx={4}
          fill="#1b2b39"
          stroke="#33475c"
        />
        <rect
          x={box.x + box.width / 2 - 15}
          y={box.y + box.height - 12}
          width={30}
          height={8}
          rx={3}
          fill="#2a3d50"
        />
      </g>

      {BOARD_PINS.map((pin) => {
        const list = pin.side === 'left' ? leftPins : rightPins
        const index = list.findIndex((candidate) => candidate.name === pin.name)
        const y = box.y + BOARD_PIN_TOP + index * BOARD_PIN_SPACING
        const isLeft = pin.side === 'left'
        const x = isLeft ? box.x : box.x + box.width
        const level = levelOfPin(pin.name)
        const tint = PIN_TINT[pin.kind] ?? '#5d7c99'
        return (
          <g key={pin.name}>
            <rect
              x={x - 5}
              y={y - 5}
              width={10}
              height={10}
              rx={2}
              fill="#0b131b"
              stroke={tint}
              strokeWidth={1}
            />
            <rect x={x - 2.5} y={y - 2.5} width={5} height={5} rx={1} fill={tint} opacity={0.8} />
            <text
              x={isLeft ? x + 10 : x - 10}
              y={y + 3}
              textAnchor={isLeft ? 'start' : 'end'}
              fontSize={8.5}
              fill="#7b8ea3"
              fontFamily="ui-monospace, monospace"
            >
              {pin.name}
            </text>
            {level ? (
              <circle cx={x} cy={y} r={7} fill={levelColor(level)} opacity={0.28} />
            ) : null}
          </g>
        )
      })}

      <text
        x={box.x + 18}
        y={box.y + 28}
        fontSize={10}
        fontWeight={600}
        fill="#9db0c5"
        letterSpacing={1.4}
      >
        {board.label ?? 'U1'}
      </text>
      <text
        x={box.x + box.width - 18}
        y={box.y + 28}
        textAnchor="end"
        fontSize={9}
        fill="#5f7288"
        letterSpacing={0.8}
      >
        DevKitC-1
      </text>

      {selected ? (
        <rect
          x={box.x - 8}
          y={box.y - 8}
          width={box.width + 16}
          height={box.height + 16}
          rx={22}
          fill="none"
          stroke="#38bdf8"
          strokeWidth={1}
          strokeDasharray="4 6"
          opacity={0.7}
        />
      ) : null}
    </g>
  )
}
