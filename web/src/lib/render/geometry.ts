import type { BoardInstance, DeviceInstance, Point } from '../api/types'
import {
  BOARD_PIN_SPACING,
  BOARD_PIN_TOP,
  BOARD_PINS_BY_NAME,
  BOARD_SIZE,
  pinCountBySide,
  pinIndex,
} from './board-pins'
import { geometryOf } from './device-pins'

export interface Anchor {
  x: number
  y: number
  /** Direção do stub de saída: -1 esquerda, +1 direita. */
  dir: -1 | 1
}

export function boardBox(board: BoardInstance) {
  return {
    x: board.position.x,
    y: board.position.y,
    width: BOARD_SIZE.width,
    height: BOARD_SIZE.height,
  }
}

export function boardAnchor(board: BoardInstance, pinName: string): Anchor | null {
  const pin = BOARD_PINS_BY_NAME.get(pinName)
  if (!pin) return null
  const index = pinIndex(pin)
  if (index < 0) return null
  const box = boardBox(board)
  const y = box.y + BOARD_PIN_TOP + index * BOARD_PIN_SPACING
  if (pin.side === 'left') return { x: box.x, y, dir: -1 }
  return { x: box.x + box.width, y, dir: 1 }
}

export function deviceBox(device: DeviceInstance) {
  const geometry = geometryOf(device.type)
  return {
    x: device.position.x - geometry.width / 2,
    y: device.position.y - geometry.height / 2,
    width: geometry.width,
    height: geometry.height,
  }
}

export function deviceAnchor(
  device: DeviceInstance,
  pinName: string,
): Anchor | null {
  const geometry = geometryOf(device.type)
  const pin = geometry.pins.find((candidate) => candidate.name === pinName)
  if (!pin) return null
  const y = device.position.y + pin.offsetY
  if (pin.side === 'left') {
    return { x: device.position.x - geometry.width / 2, y, dir: -1 }
  }
  return { x: device.position.x + geometry.width / 2, y, dir: 1 }
}

/** Caminho ortogonal entre dois pinos, com stub e desvio quando preciso. */
export function routeWire(a: Anchor, b: Anchor): string {
  const stub = 18
  const start = { x: a.x + a.dir * stub, y: a.y }
  const end = { x: b.x + b.dir * stub, y: b.y }

  if (a.dir === b.dir) {
    const outer =
      a.dir === 1
        ? Math.max(start.x, end.x) + stub
        : Math.min(start.x, end.x) - stub
    return [
      `M ${a.x} ${a.y}`,
      `L ${start.x} ${start.y}`,
      `L ${outer} ${start.y}`,
      `L ${outer} ${end.y}`,
      `L ${end.x} ${end.y}`,
      `L ${b.x} ${b.y}`,
    ].join(' ')
  }

  const midX = (start.x + end.x) / 2
  return [
    `M ${a.x} ${a.y}`,
    `L ${start.x} ${start.y}`,
    `L ${midX} ${start.y}`,
    `L ${midX} ${end.y}`,
    `L ${end.x} ${end.y}`,
    `L ${b.x} ${b.y}`,
  ].join(' ')
}

export function snapToGrid(point: Point, grid = 12): Point {
  return {
    x: Math.round(point.x / grid) * grid,
    y: Math.round(point.y / grid) * grid,
  }
}

export function pinRowsBySide(side: 'left' | 'right'): number {
  return pinCountBySide(side)
}
