// Níveis elétricos vindos do frame, apenas para colorir a tela.

import type { NetRuntime } from '../api/types'

export type NetLevel = NetRuntime['level']

const LEVEL_COLOR: Record<NetLevel, string> = {
  high: '#34d399',
  low: '#64748b',
  z: '#334155',
  x: '#fb7185',
}

export function levelColor(level: NetLevel): string {
  return LEVEL_COLOR[level]
}

export function levelLabel(level: NetLevel): string {
  switch (level) {
    case 'high':
      return 'alto'
    case 'low':
      return 'baixo'
    case 'z':
      return 'alta-Z'
    case 'x':
      return 'indefinido'
  }
}

export function levelsByNet(nets: NetRuntime[]): Map<string, NetLevel> {
  return new Map(nets.map((net) => [net.id, net.level]))
}
