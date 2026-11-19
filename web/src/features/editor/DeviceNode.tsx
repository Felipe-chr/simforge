import type { PointerEvent as ReactPointerEvent } from 'react'

import type { DeviceInstance, DeviceRuntime } from '../../lib/api/types'
import { deviceAnchor, deviceBox } from '../../lib/render/geometry'
import { geometryOf } from '../../lib/render/device-pins'

const LED_COLORS: Record<string, string> = {
  red: '#ef4444',
  green: '#22c55e',
  blue: '#3b82f6',
  amber: '#f59e0b',
  white: '#e2e8f0',
}

interface DeviceNodeProps {
  device: DeviceInstance
  runtime?: DeviceRuntime
  selected: boolean
  dragging: boolean
  onSelect: () => void
  onPointerDown: (event: ReactPointerEvent<SVGGElement>) => void
}

export function DeviceNode({
  device,
  runtime,
  selected,
  dragging,
  onSelect,
  onPointerDown,
}: DeviceNodeProps) {
  const box = deviceBox(device)
  const geometry = geometryOf(device.type)
  const color = LED_COLORS[String(device.params.color ?? 'red')] ?? LED_COLORS.red
  const on = Boolean(runtime?.on)
  const busy = Boolean(runtime?.pressed)

  const stubs = geometry.pins.map((pin) => {
    const anchor = deviceAnchor(device, pin.name)
    if (!anchor) return null
    const innerX = anchor.x + anchor.dir * -12
    return (
      <line
        key={pin.name}
        x1={anchor.x}
        y1={anchor.y}
        x2={innerX}
        y2={anchor.y}
        stroke="#4a6076"
        strokeWidth={1.6}
      />
    )
  })

  return (
    <g
      onClick={(event) => {
        event.stopPropagation()
        onSelect()
      }}
      onPointerDown={onPointerDown}
      className={dragging ? 'cursor-grabbing' : 'cursor-grab'}
    >
      {on && device.type === 'simforge.led' ? (
        <circle
          cx={device.position.x}
          cy={device.position.y}
          r={30}
          fill={color}
          opacity={0.22}
          filter="url(#ledGlow)"
        />
      ) : null}

      {stubs}

      <g transform={`rotate(${device.rotation} ${device.position.x} ${device.position.y})`}>
        {device.type === 'simforge.led' ? (
          <g>
            <circle
              cx={device.position.x}
              cy={device.position.y}
              r={16}
              fill={on ? color : '#1a2530'}
              stroke={on ? color : '#3a4c5f'}
              strokeWidth={1.6}
            />
            <circle
              cx={device.position.x - 5}
              cy={device.position.y - 5}
              r={5}
              fill="#ffffff"
              opacity={on ? 0.45 : 0.08}
            />
          </g>
        ) : null}

        {device.type === 'simforge.button' ? (
          <g>
            <rect
              x={box.x}
              y={box.y}
              width={box.width}
              height={box.height}
              rx={8}
              fill={busy ? '#1d2b36' : '#151f29'}
              stroke="#3a4c5f"
              strokeWidth={1.5}
            />
            <circle
              cx={device.position.x}
              cy={busy ? device.position.y + 1 : device.position.y}
              r={busy ? 10 : 12}
              fill="#22303d"
              stroke="#4a6076"
            />
            <circle cx={device.position.x} cy={device.position.y} r={6} fill="#2d3d4c" />
          </g>
        ) : null}

        {device.type === 'simforge.resistor' ? (
          <path
            d={`M ${box.x} ${device.position.y} l 10 0 l 5 -9 l 10 18 l 10 -18 l 10 18 l 10 -18 l 5 9 l 10 0`}
            fill="none"
            stroke="#c9a227"
            strokeWidth={1.8}
            strokeLinejoin="round"
          />
        ) : null}

        {device.type === 'simforge.clock' ? (
          <g>
            <rect
              x={box.x}
              y={box.y}
              width={box.width}
              height={box.height}
              rx={6}
              fill="#151f29"
              stroke={on ? '#38bdf8' : '#3a4c5f'}
              strokeWidth={1.5}
            />
            <path
              d={`M ${device.position.x - 22} ${device.position.y + 8} h 11 v -16 h 11 v 16 h 11 v -16 h 11`}
              fill="none"
              stroke={on ? '#38bdf8' : '#4a6076'}
              strokeWidth={1.6}
              strokeLinejoin="miter"
            />
          </g>
        ) : null}

        {device.type === 'simforge.potentiometer' ? (
          <g>
            <rect
              x={box.x}
              y={box.y}
              width={box.width}
              height={box.height}
              rx={6}
              fill="#151f29"
              stroke="#3a4c5f"
              strokeWidth={1.5}
            />
            <circle
              cx={device.position.x}
              cy={device.position.y - 4}
              r={12}
              fill="#22303d"
              stroke="#4a6076"
            />
            <line
              x1={device.position.x}
              y1={device.position.y - 12}
              x2={device.position.x + 7}
              y2={device.position.y - 2}
              stroke="#8fa7bd"
              strokeWidth={1.6}
            />
          </g>
        ) : null}

        {device.type === 'simforge.buzzer' ? (
          <g>
            <circle
              cx={device.position.x}
              cy={device.position.y}
              r={24}
              fill="#151f29"
              stroke="#3a4c5f"
              strokeWidth={1.5}
            />
            <path
              d={`M ${device.position.x - 8} ${device.position.y - 6} h 16 v 12 h -16 z`}
              fill="#22303d"
              stroke="#4a6076"
            />
            <path
              d={`M ${device.position.x + 14} ${device.position.y - 10} a 18 18 0 0 1 0 20`}
              fill="none"
              stroke="#4a6076"
            />
          </g>
        ) : null}

        {device.type === 'simforge.oled' ? (
          <g>
            <rect
              x={box.x}
              y={box.y}
              width={box.width}
              height={box.height}
              rx={5}
              fill="#131d26"
              stroke="#3a4c5f"
              strokeWidth={1.5}
            />
            <rect
              x={box.x + 8}
              y={box.y + 8}
              width={box.width - 16}
              height={box.height - 24}
              rx={2}
              fill="#0a1219"
              stroke="#22303d"
            />
            <text
              x={device.position.x}
              y={device.position.y + 2}
              textAnchor="middle"
              fontSize={8}
              fill="#4f9bb8"
              fontFamily="ui-monospace, monospace"
            >
              SSD1306
            </text>
          </g>
        ) : null}
      </g>

      {selected ? (
        <rect
          x={box.x - 10}
          y={box.y - 10}
          width={box.width + 20}
          height={box.height + 20}
          rx={10}
          fill="none"
          stroke="#38bdf8"
          strokeWidth={1.2}
          strokeDasharray="4 5"
        />
      ) : null}

      <text
        x={device.position.x}
        y={box.y + box.height + 20}
        textAnchor="middle"
        fontSize={10}
        fontWeight={600}
        fill={selected ? '#cfe3f5' : '#8fa2b7'}
        fontFamily="ui-monospace, monospace"
      >
        {device.label ?? device.type.split('.').at(-1)}
      </text>
    </g>
  )
}
