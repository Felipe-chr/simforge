// Geometria da placa ESP32-S3 DevKitC-1, apenas para desenho.
// A fonte de verdade dos pinos é o plugin da placa; aqui é uma lista visual.

export type PinKind = 'power' | 'gnd' | 'io' | 'adc' | 'usb' | 'boot'

export interface BoardPin {
  name: string
  side: 'left' | 'right'
  kind: PinKind
}

export const BOARD_SIZE = { width: 420, height: 372 }
export const BOARD_PIN_SPACING = 24
export const BOARD_PIN_TOP = 48

const LEFT: BoardPin[] = [
  { name: '3V3', side: 'left', kind: 'power' },
  { name: 'EN', side: 'left', kind: 'boot' },
  { name: 'GPIO4', side: 'left', kind: 'io' },
  { name: 'GPIO5', side: 'left', kind: 'io' },
  { name: 'GPIO6', side: 'left', kind: 'io' },
  { name: 'GPIO7', side: 'left', kind: 'io' },
  { name: 'GPIO15', side: 'left', kind: 'io' },
  { name: 'GPIO16', side: 'left', kind: 'io' },
  { name: 'GPIO17', side: 'left', kind: 'io' },
  { name: 'GPIO18', side: 'left', kind: 'io' },
  { name: 'GPIO8', side: 'left', kind: 'adc' },
  { name: 'GPIO3', side: 'left', kind: 'adc' },
  { name: 'GPIO0', side: 'left', kind: 'boot' },
]

const RIGHT: BoardPin[] = [
  { name: 'GND', side: 'right', kind: 'gnd' },
  { name: '5V', side: 'right', kind: 'power' },
  { name: 'GPIO43', side: 'right', kind: 'usb' },
  { name: 'GPIO44', side: 'right', kind: 'usb' },
  { name: 'GPIO1', side: 'right', kind: 'adc' },
  { name: 'GPIO2', side: 'right', kind: 'io' },
  { name: 'GPIO42', side: 'right', kind: 'io' },
  { name: 'GPIO41', side: 'right', kind: 'io' },
  { name: 'GPIO40', side: 'right', kind: 'io' },
  { name: 'GPIO39', side: 'right', kind: 'io' },
  { name: 'GPIO38', side: 'right', kind: 'io' },
  { name: 'GPIO37', side: 'right', kind: 'io' },
  { name: 'GPIO36', side: 'right', kind: 'io' },
]

export const BOARD_PINS: BoardPin[] = [...LEFT, ...RIGHT]

export const BOARD_PINS_BY_NAME = new Map(BOARD_PINS.map((pin) => [pin.name, pin]))

export function pinIndex(pin: BoardPin): number {
  const list = pin.side === 'left' ? LEFT : RIGHT
  return list.findIndex((candidate) => candidate.name === pin.name)
}

export function pinCountBySide(side: 'left' | 'right'): number {
  return side === 'left' ? LEFT.length : RIGHT.length
}
