// Escolha do transporte em tempo de execução.
//
// Ordem:
//   1. `VITE_SIMFORGE_TRANSPORT=core|mock` força um dos dois;
//   2. se a ponte `/sf/status` responde `ok`, usa o núcleo C++;
//   3. caso contrário (build de produção, núcleo não compilado), usa o mock.

import { CliTransport } from './cli'
import { transport as mockTransport } from './mock'
import { probeBridge } from './transport'
import type { BridgeStatus, Transport } from './transport'

export interface ResolvedTransport {
  transport: Transport
  bridge: BridgeStatus | null
}

export async function resolveTransport(): Promise<ResolvedTransport> {
  const forced = import.meta.env.VITE_SIMFORGE_TRANSPORT
  const bridge = await probeBridge()

  if (forced === 'mock') return { transport: mockTransport, bridge }
  if (forced === 'core' || bridge?.ok) {
    return { transport: new CliTransport(), bridge }
  }
  return { transport: mockTransport, bridge }
}
