# ADR 0006 — Firmware em WASM como sandbox padrão

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0007, ADR 0015

## Contexto

Firmware de usuário pode entrar em loop infinito, consumir memória ou tentar
acessar o host. Executar DLL nativa no processo principal é inseguro e
dificulta portabilidade.

## Decisão

- O runtime padrão do MVP é WASM (wasmtime), com imports explícitos para GPIO,
  barramentos, serial e tempo.
- Memória limitada, fuel/epoch por fatia e watchdog.
- Sem filesystem, rede ou threads por padrão.
- Runtime nativo fica disponível apenas em modo desenvolvedor.

## Consequências

- Isolamento e determinismo melhores.
- Pequeno custo de performance e toolchain adicional.
- Bibliotecas que dependem de registradores não funcionam no shim.
- O caminho para QEMU continua aberto (ADR 0018).

## Alternativas

- DLL nativa: rápida, mas insegura e não portátil.
- Processo separado nativo: mais pesado para cada sessão.
