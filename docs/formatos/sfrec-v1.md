# `.sfrec` — Gravação e Replay v1

**Status:** proposta congelável
**Extensão:** `.sfrec`
**Formato:** binário, little-endian
**Payload:** CBOR canônico (RFC 8949)
**Integridade:** SHA-256 no rodapé

---

## 1. Objetivo

Gravar uma sessão de simulação de forma compacta e determinística para:

- replay fiel sem firmware/mundo externo;
- regressão visual e de métricas;
- depuração de falhas intermitentes;
- exportação de telemetria.

O replay não reexecuta o firmware nem consulta mundos externos: aplica eventos
e snapshots gravados.

---

## 2. Layout do arquivo

```
┌──────────────────────────────────────────────┐
│ Header                                       │
│  magic       "SFRC"            4 bytes       │
│  version     u16 = 1           2 bytes       │
│  flags       u16               2 bytes       │
│  header_len  u32               4 bytes       │
│  header_json UTF-8             header_len    │
├──────────────────────────────────────────────┤
│ Records (repetidos)                          │
│  type        u8                1 byte        │
│  flags       u8                1 byte        │
│  reserved    u16 = 0           2 bytes       │
│  length      u32               4 bytes       │
│  payload     CBOR              length        │
├──────────────────────────────────────────────┤
│ Footer                                       │
│  record_count u64              8 bytes       │
│  sha256       bytes[32]        32 bytes      │
│  magic_end    "SFRC"           4 bytes       │
└──────────────────────────────────────────────┘
```

- `sha256` cobre todos os bytes anteriores ao campo.
- `header_len` máximo: 1 MB. `length` máximo por record: 16 MB.
- Arquivos com magic/versão inválidos são rejeitados com `rec.invalid_format`.
- O arquivo pode ser truncado; o replay para no último record íntegro e avisa
  `rec.truncated`.

---

## 3. Header

```json
{
  "format": "simforge.recording",
  "schema": 1,
  "engine_version": "1.0.0",
  "project_hash": "sha256:9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d",
  "project_id": "01J9Z6K3V8Q2M4T7R1C5B0N8XA",
  "seed": 12345,
  "runtime": {
    "kind": "wasm",
    "artifact_hash": "sha256:9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d",
    "board": "simforge.esp32s3.devkitc"
  },
  "started_at": "2026-11-14T12:00:00Z",
  "duration_ns": 60000000000,
  "slice_us": 1000,
  "solver": "logic",
  "features": ["world.recorded", "frames.60hz"],
  "notes": "execução de referência"
}
```

- `started_at` é informativo; nunca entra no hash de simulação.
- `project_hash` e `runtime.artifact_hash` impedem replay com projeto/firmware
  diferente.
- `features` lista o que foi gravado (frames, waveforms, serial).
- `seed` e `slice_us` precisam coincidir com o replay.

---

## 4. Records

| Tipo | Nome | Payload |
|---|---|---|
| `0x01` | `event` | `{seq, t_ns, type, data}` |
| `0x02` | `snapshot` | estado completo do mundo em `t_ns` |
| `0x03` | `input` | leitura/evento de mundo externo |
| `0x04` | `marker` | anotação do usuário |
| `0x05` | `stats` | métricas agregadas |
| `0x06` | `frame` | frame visual (opcional) |
| `0x07` | `serial` | chunk de UART |
| `0xFF` | `end` | `{duration_ns, hash}` |

### 4.1 Event

```json
{
  "seq": 1001,
  "t_ns": 250000000,
  "type": "pin.change",
  "data": { "pin": { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "pin": "GPIO2" }, "level": "high" }
}
```

### 4.2 Snapshot

Contém:

- `t_ns`, `seq`;
- estado dos dispositivos (via `write_state`);
- estado das nets e pinos;
- estado do runtime (registradores/stack quando disponível);
- estado dos mundos externos;
- hash parcial do trace.

Snapshots são gravados a cada 1 s de tempo virtual ou 100k eventos,
configurável. Um snapshot no início é obrigatório.

### 4.3 Input

```json
{
  "t_ns": 1000000,
  "world": "pistaforge",
  "entity": "radar.1",
  "value": { "speed_mps": 12.5 },
  "quality": "good"
}
```

### 4.4 End

```json
{
  "duration_ns": 60000000000,
  "event_count": 1234567,
  "trace_hash": "sha256:9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d"
}
```

---

## 5. CBOR canônico

- Mapas com chaves ordenadas (comprimento, depois bytes) — RFC 8949 §4.2.
- Floats em `float64`; `NaN`/`Inf` proibidos em estado persistido.
- Strings UTF-8 normalizadas (NFC) quando representam nomes.
- Inteiros sem sinal para tempo/contadores; `int64` para valores assinados.
- O decoder rejeita tags desconhecidas e profundidade > 32.

---

## 6. Replay

```
1. abrir arquivo, validar magic/versão/hash do rodapé
2. validar project_hash e runtime.artifact_hash
3. carregar snapshot inicial
4. para cada record:
   - event: aplicar no t_ns
   - input: injetar no mundo
   - frame/serial: publicar na UI
   - marker: exibir anotação
5. ao final, comparar trace_hash com o esperado (se fornecido)
```

- O replay é somente leitura: comandos de edição são bloqueados.
- `rec.seek(t)` usa o snapshot mais próximo + reaplicação de eventos.
- `rec.play(speed)` respeita o tempo virtual; `speed=0` = passo a passo.
- Divergência de hash gera `rec.hash_mismatch` com o primeiro `seq` divergente.

---

## 7. Compatibilidade

- `schema` inteiro; versão futura é recusada.
- Records desconhecidos são ignorados e contados em `features`.
- Campos novos em payloads CBOR são ignorados.
- O replay usa o engine da versão gravada ou uma versão que declare
  compatibilidade; caso contrário, avisa `rec.engine_mismatch`.

---

## 8. Segurança e limites

- Sem código executável no arquivo.
- Limite de memória para snapshot: 256 MB.
- Limite de records: 100M.
- Verificação de hash antes de qualquer apply.
- Assinatura opcional (`signature` no header) para bundles de marketplace.
