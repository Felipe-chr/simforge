// Transporte para o núcleo C++ via ponte do dev server (server/simforge-cli.ts).
//
// O cliente envia o envelope da api-v1 como veio e devolve a resposta do núcleo
// sem interpretar nada: mesma forma do MockTransport.

import type { ApiRequest, ApiResponse } from './types'
import type { BridgeStatus, Transport } from './transport'

export class CliTransport implements Transport {
  readonly kind = 'core' as const
  private readonly endpoint: string

  constructor(endpoint = '/sf/command') {
    this.endpoint = endpoint
  }

  async execute<T = unknown>(request: ApiRequest): Promise<ApiResponse<T>> {
    const response = await fetch(this.endpoint, {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify(request),
    })

    const payload = (await response.json()) as ApiResponse<T> & {
      available?: boolean
      error?: { code: string; message: string }
    }

    if (!response.ok && payload.ok !== false) {
      throw new Error(payload.error?.message ?? `ponte respondeu ${response.status}`)
    }
    return payload
  }

  async status(): Promise<BridgeStatus | null> {
    const response = await fetch('/sf/status')
    if (!response.ok) return null
    return (await response.json()) as BridgeStatus
  }
}
