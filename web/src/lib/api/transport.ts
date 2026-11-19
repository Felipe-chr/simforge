// Contrato de transporte da UI.
//
// A interface só conversa com um `Transport`: hoje há o mock em processo e a
// ponte NDJSON para o núcleo C++ (`simforge-cli api`). Trocar um pelo outro não
// muda nenhum componente.

import type { ApiRequest, ApiResponse } from './types'

export type TransportKind = 'core' | 'mock'

export interface Transport {
  readonly kind: TransportKind
  execute<T = unknown>(request: ApiRequest): Promise<ApiResponse<T>>
}

export interface BridgeStatus {
  ok: boolean
  available?: boolean
  cli?: string
  project?: string
  running?: boolean
  speed?: number
  error?: string | null
}

/** Consulta a ponte do dev server. Ausência de resposta = sem núcleo. */
export async function probeBridge(timeoutMs = 900): Promise<BridgeStatus | null> {
  if (typeof fetch !== 'function') return null
  const controller = new AbortController()
  const timer = window.setTimeout(() => controller.abort(), timeoutMs)
  try {
    const response = await fetch('/sf/status', { signal: controller.signal })
    if (!response.ok) return null
    return (await response.json()) as BridgeStatus
  } catch {
    return null
  } finally {
    window.clearTimeout(timer)
  }
}
