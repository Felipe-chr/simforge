// Formatação de valores do contrato para exibição.

export function formatVirtualTime(ns: number): string {
  if (!Number.isFinite(ns)) return '—'
  const abs = Math.abs(ns)
  if (abs < 1_000) return `${ns.toFixed(0)} ns`
  if (abs < 1_000_000) return `${(ns / 1_000).toFixed(1)} µs`
  if (abs < 1_000_000_000) return `${(ns / 1_000_000).toFixed(2)} ms`
  return `${(ns / 1_000_000_000).toFixed(3)} s`
}

export function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(2)} MB`
}

export function shortId(id: string): string {
  return id.length <= 10 ? id : `${id.slice(0, 6)}…${id.slice(-3)}`
}
