// Geometria dos componentes: tamanho e posição dos pinos, só para desenho.

export interface DevicePin {
  name: string
  side: 'left' | 'right'
  /** Deslocamento vertical em relação ao centro do componente. */
  offsetY: number
}

export interface DeviceGeometry {
  width: number
  height: number
  pins: DevicePin[]
}

const twoPins: DeviceGeometry = {
  width: 78,
  height: 54,
  pins: [
    { name: 'A', side: 'left', offsetY: 0 },
    { name: 'B', side: 'right', offsetY: 0 },
  ],
}

export const DEVICE_GEOMETRY: Record<string, DeviceGeometry> = {
  'simforge.led': {
    width: 74,
    height: 54,
    pins: [
      { name: 'K', side: 'left', offsetY: 0 },
      { name: 'A', side: 'right', offsetY: 0 },
    ],
  },
  'simforge.button': {
    width: 78,
    height: 54,
    pins: [
      { name: '1', side: 'right', offsetY: 0 },
      { name: '2', side: 'left', offsetY: 0 },
    ],
  },
  'simforge.resistor': twoPins,
  'simforge.clock': {
    width: 74,
    height: 54,
    pins: [{ name: 'OUT', side: 'right', offsetY: 0 }],
  },
  'simforge.potentiometer': {
    width: 78,
    height: 62,
    pins: [
      { name: 'A', side: 'left', offsetY: 0 },
      { name: 'B', side: 'right', offsetY: 0 },
      { name: 'W', side: 'right', offsetY: 18 },
    ],
  },
  'simforge.buzzer': {
    width: 66,
    height: 66,
    pins: [
      { name: '-', side: 'left', offsetY: 0 },
      { name: '+', side: 'right', offsetY: 0 },
    ],
  },
  'simforge.oled': {
    width: 96,
    height: 74,
    pins: [
      { name: 'GND', side: 'left', offsetY: -18 },
      { name: 'VCC', side: 'left', offsetY: -6 },
      { name: 'SCL', side: 'left', offsetY: 6 },
      { name: 'SDA', side: 'left', offsetY: 18 },
    ],
  },
}

export const FALLBACK_GEOMETRY: DeviceGeometry = twoPins

export function geometryOf(type: string): DeviceGeometry {
  return DEVICE_GEOMETRY[type] ?? FALLBACK_GEOMETRY
}
