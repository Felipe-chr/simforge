// Ponte entre o dev server do Vite e o núcleo C++.
//
// O `simforge-cli api` fala NDJSON no stdin/stdout usando o mesmo CommandBus da
// ABI C (docs/protocolos/api-v1.md). Este plugin:
//   - sobe um processo por dev server e reusa a mesma sessão;
//   - expõe POST /sf/command  → repassa a requisição e devolve a resposta;
//   - expõe GET  /sf/status   → informa se o núcleo está disponível;
//   - avança o tempo virtual enquanto a simulação está em execução.
//
// Nada aqui conhece semântica elétrica: só transporte e cadência.

import { spawn } from 'node:child_process'
import type { ChildProcessWithoutNullStreams } from 'node:child_process'
import { existsSync } from 'node:fs'
import { createInterface } from 'node:readline'
import { resolve } from 'node:path'
import type { IncomingMessage, ServerResponse } from 'node:http'
import type { Plugin, ViteDevServer } from 'vite'

export interface SimforgeCliOptions {
  /** Projeto aberto automaticamente ao subir o processo. */
  project?: string
  /** Caminho explícito do executável (vence a busca automática). */
  cli?: string
  /** Intervalo de parede entre passos, em ms. */
  tickMs?: number
  /** Tempo virtual avançado por passo, em ns (× velocidade). */
  sliceNs?: number
}

interface Pending {
  resolve: (line: string) => void
  reject: (error: Error) => void
  timer: NodeJS.Timeout
}

const DEFAULT_TICK_MS = 40
const DEFAULT_SLICE_NS = 20_000_000
const REQUEST_TIMEOUT_MS = 8_000

function candidates(repoRoot: string, explicit?: string): string[] {
  if (explicit) return [explicit]
  const names = ['simforge-cli.exe', 'simforge-cli']
  const dirs = [
    'build/release/src',
    'build/dev/src',
    'build/release',
    'build/dev',
  ]
  return dirs.flatMap((dir) => names.map((name) => resolve(repoRoot, dir, name)))
}

class Bridge {
  private child: ChildProcessWithoutNullStreams | null = null
  private pending: Pending[] = []
  private counter = 0
  private timer: NodeJS.Timeout | null = null
  private speed = 1
  private running = false
  private starting: Promise<void> | null = null

  readonly cliPath: string
  readonly projectPath: string
  private readonly tickMs: number
  private readonly sliceNs: number
  private readonly repoRoot: string
  private lastError: string | null = null

  constructor(
    cliPath: string,
    projectPath: string,
    repoRoot: string,
    tickMs: number,
    sliceNs: number,
  ) {
    this.cliPath = cliPath
    this.projectPath = projectPath
    this.repoRoot = repoRoot
    this.tickMs = tickMs
    this.sliceNs = sliceNs
  }

  available(): boolean {
    return existsSync(this.cliPath)
  }

  get error(): string | null {
    return this.lastError
  }

  status() {
    return {
      ok: this.available() && this.lastError === null,
      started: this.child !== null,
      available: this.available(),
      cli: this.cliPath,
      project: this.projectPath,
      running: this.running,
      speed: this.speed,
      error: this.lastError,
    }
  }

  async ensureStarted(): Promise<void> {
    if (this.child) return
    if (!this.starting) {
      this.starting = this.start().finally(() => {
        this.starting = null
      })
    }
    return this.starting
  }

  private async spawnChild(): Promise<ChildProcessWithoutNullStreams> {
    // O Windows pode recusar o spawn logo após o link (antivírus/Smart App
    // Control/OneDrive hidratando o .exe). Node devolve um erro síncrono
    // `spawn UNKNOWN`; algumas tentativas resolvem sem reiniciar o dev server.
    let lastError: unknown = null
    for (let attempt = 0; attempt < 4; attempt += 1) {
      try {
        return spawn(this.cliPath, ['api'], {
          cwd: this.repoRoot,
          stdio: ['pipe', 'pipe', 'pipe'],
          windowsHide: true,
        })
      } catch (error) {
        lastError = error
        await new Promise((resolveDelay) => setTimeout(resolveDelay, 200 * (attempt + 1)))
      }
    }
    throw lastError instanceof Error ? lastError : new Error(String(lastError))
  }

  private async start(): Promise<void> {
    if (!this.available()) {
      this.lastError = `executável não encontrado: ${this.cliPath}`
      throw new Error(this.lastError)
    }

    let child: ChildProcessWithoutNullStreams
    try {
      child = await this.spawnChild()
    } catch (error) {
      this.lastError = error instanceof Error ? error.message : String(error)
      throw new Error(this.lastError)
    }
    this.child = child
    this.lastError = null

    createInterface({ input: child.stdout }).on('line', (line) => {
      const next = this.pending.shift()
      if (!next) return
      clearTimeout(next.timer)
      next.resolve(line)
    })

    createInterface({ input: child.stderr }).on('line', (line) => {
      if (line.trim()) console.warn(`[simforge-cli] ${line}`)
    })

    child.on('exit', (code, signal) => {
      const reason = `simforge-cli terminou (code=${code ?? 'null'}, signal=${signal ?? 'null'})`
      this.lastError = reason
      this.child = null
      this.stopPacing()
      for (const pending of this.pending.splice(0)) {
        clearTimeout(pending.timer)
        pending.reject(new Error(reason))
      }
    })

    child.on('error', (error) => {
      this.lastError = error.message
    })

    const opened = (await this.request('project.open', { path: this.projectPath })) as string
    const parsed = JSON.parse(opened) as {
      ok?: boolean
      error?: { code?: string; message?: string }
    }
    if (parsed.ok !== true) {
      this.lastError = `falha ao abrir o projeto: ${parsed.error?.code ?? '?'} ${
        parsed.error?.message ?? ''
      }`.trim()
      this.dispose()
      throw new Error(this.lastError)
    }
  }

  request(method: string, params: Record<string, unknown>): Promise<string> {
    if (!this.child) return Promise.reject(new Error('núcleo não iniciado'))
    this.counter += 1
    const envelope = {
      id: `web-srv-${this.counter}`,
      method,
      params,
      meta: { api_version: 1, client: 'simforge-web/dev-bridge' },
    }

    return new Promise<string>((resolveLine, rejectLine) => {
      const timer = setTimeout(() => {
        const index = this.pending.findIndex((entry) => entry.timer === timer)
        if (index >= 0) this.pending.splice(index, 1)
        rejectLine(new Error(`tempo esgotado em ${method}`))
      }, REQUEST_TIMEOUT_MS)

      this.pending.push({ resolve: resolveLine, reject: rejectLine, timer })
      this.child?.stdin.write(`${JSON.stringify(envelope)}\n`)
    })
  }

  /** Executa um comando e já o interpreta como envelope api-v1. */
  async command(method: string, params: Record<string, unknown>): Promise<unknown> {
    await this.ensureStarted()
    const line = await this.request(method, params)
    return JSON.parse(line)
  }

  private startPacing(): void {
    if (this.timer) return
    this.running = true
    this.timer = setInterval(() => {
      if (!this.child) {
        this.stopPacing()
        return
      }
      const ns = Math.max(1, Math.round(this.sliceNs * this.speed))
      void this.request('sim.step', { ns }).catch(() => this.stopPacing())
    }, this.tickMs)
  }

  private stopPacing(): void {
    this.running = false
    if (this.timer) {
      clearInterval(this.timer)
      this.timer = null
    }
  }

  /** Aplica efeitos colaterais de cadência depois de cada comando. */
  observe(method: string, params: Record<string, unknown>, response: unknown): void {
    const envelope = response as { ok?: boolean }
    if (!envelope || envelope.ok !== true) return

    if (method === 'sim.run' || method === 'sim.resume') this.startPacing()
    if (method === 'sim.pause' || method === 'sim.stop' || method === 'sim.reset') {
      this.stopPacing()
    }
    if (method === 'sim.setSpeed') {
      const factor = Number(params.factor)
      if (Number.isFinite(factor) && factor > 0) this.speed = factor
    }
  }

  dispose(): void {
    this.stopPacing()
    this.child?.stdin.end()
    this.child?.kill()
    this.child = null
  }
}

async function readBody(request: IncomingMessage): Promise<string> {
  const chunks: Buffer[] = []
  for await (const chunk of request) chunks.push(chunk as Buffer)
  return Buffer.concat(chunks).toString('utf8')
}

function sendJson(response: ServerResponse, status: number, payload: unknown): void {
  response.statusCode = status
  response.setHeader('content-type', 'application/json; charset=utf-8')
  response.setHeader('cache-control', 'no-store')
  response.end(JSON.stringify(payload))
}

function unavailable(error: string) {
  return {
    ok: false,
    available: false,
    error,
  }
}

export function simforgeCli(options: SimforgeCliOptions = {}): Plugin {
  let bridge: Bridge | null = null

  return {
    name: 'simforge-cli',
    apply: 'serve',
    configResolved(config) {
      const repoRoot = resolve(config.root, '..')
      const cliPath =
        candidates(repoRoot, options.cli ?? process.env.SIMFORGE_CLI).find((path) =>
          existsSync(path),
        ) ?? resolve(repoRoot, 'build', 'release', 'src', 'simforge-cli.exe')
      const projectPath = resolve(
        repoRoot,
        options.project ?? process.env.SIMFORGE_PROJECT ?? 'examples/blink-led.sforge',
      )
      bridge = new Bridge(
        cliPath,
        projectPath,
        repoRoot,
        options.tickMs ?? DEFAULT_TICK_MS,
        options.sliceNs ?? DEFAULT_SLICE_NS,
      )
    },
    configureServer(server: ViteDevServer) {
      server.httpServer?.on('close', () => bridge?.dispose())

      // Aquece o processo para o primeiro comando já encontrar a sessão pronta.
      if (bridge?.available()) {
        void bridge.ensureStarted().catch((error: unknown) => {
          console.warn(`[simforge] ${error instanceof Error ? error.message : String(error)}`)
        })
      }

      server.middlewares.use((request, response, next) => {
        const url = request.url ?? ''
        if (!url.startsWith('/sf/')) {
          next()
          return
        }
        if (!bridge) {
          sendJson(response, 503, unavailable('ponte não inicializada'))
          return
        }

        if (url.startsWith('/sf/status')) {
          sendJson(response, 200, bridge.status())
          return
        }

        if (url.startsWith('/sf/command') && request.method === 'POST') {
          void (async () => {
            try {
              const raw = await readBody(request)
              const envelope = JSON.parse(raw) as {
                id?: string
                method?: string
                params?: Record<string, unknown>
              }
              const method = String(envelope.method ?? '')
              if (!method) {
                sendJson(response, 400, {
                  id: envelope.id ?? null,
                  ok: false,
                  error: { code: 'invalid_params', message: 'method ausente' },
                })
                return
              }
              const params = envelope.params ?? {}
              await bridge.ensureStarted()
              const line = await bridge.request(method, params)
              const parsed = JSON.parse(line) as Record<string, unknown>
              bridge.observe(method, params, parsed)

              // O id do envelope é do cliente; o núcleo responde com o id da ponte.
              if (envelope.id) parsed.id = envelope.id
              sendJson(response, 200, parsed)
            } catch (error) {
              const message = error instanceof Error ? error.message : String(error)
              sendJson(response, 503, {
                id: null,
                ok: false,
                error: {
                  code: bridge?.available() ? 'core_unavailable' : 'core_not_built',
                  message,
                  retryable: true,
                },
              })
            }
          })()
          return
        }

        sendJson(response, 404, unavailable(`rota desconhecida: ${url}`))
      })
    },
  }
}
