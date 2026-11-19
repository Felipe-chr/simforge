# SimForge World Protocol v1

**Status:** proposta congelável
**Transporte:** WebSocket, pipe local ou shared memory (mesmas mensagens)
**Schema:** [`world-v1.schema.json`](world-v1.schema.json)
**Codificação:** JSON UTF-8 para controle; binário opcional para streams
**Semântica:** tempo virtual, barreira de sincronização, sample-and-hold

---

## 1. Objetivo

Permitir que o SimForge leia sensores, acione atuadores e troque eventos com
qualquer mundo externo — sem conhecer o produto do outro lado.

O protocolo é simétrico o suficiente para dois modos:

1. **SimForge cliente:** o simulador conecta em um mundo externo (ex.: uma
   cena de trânsito) e consome/aciona entidades.
2. **SimForge servidor:** outro produto conecta no SimForge headless e usa os
   pinos/dispositivos do projeto como entidades externas.

O SimForge nunca importa código do outro produto; o adaptador é sempre externo.

---

## 2. Modelo de entidades

```json
{
  "id": "semaforo.norte",
  "kind": "actuator",
  "type": "traffic_light",
  "description": "Semáforo do cruzamento norte",
  "unit": null,
  "schema": {
    "type": "string",
    "enum": ["red", "yellow", "green", "off"]
  }
}
```

| Campo | Regra |
|---|---|
| `id` | único no mundo; estável entre sessões |
| `kind` | `sensor`, `actuator`, `bidirectional` |
| `type` | tipo semântico genérico (`traffic_light`, `barrier`, `speed_sensor`...) |
| `schema` | JSON Schema do valor |
| `unit` | unidade SI quando numérico (`m/s`, `mm`, `V`) |

Tipos recomendados:

| Tipo | Kind | Valor |
|---|---|---|
| `traffic_light` | actuator | `"red" \| "yellow" \| "green" \| "off"` |
| `barrier` | bidirectional | `"open" \| "closed" \| "moving"` + `position_mm` |
| `speed_sensor` | sensor | `{ "speed_mps": 12.5, "valid": true }` |
| `proximity` | sensor | `{ "distance_mm": 320 }` |
| `passage_detector` | sensor/event | `{ "vehicle_id": "...", "direction": "north" }` |
| `button` | sensor | `true`/`false` |
| `generic_sensor` | sensor | número + unidade |

---

## 3. Envelope

```json
{
  "type": "world.read",
  "seq": 42,
  "t_ns": 1000000000,
  "request_id": "r_7f3a",
  "entity": "radar.1"
}
```

| Campo | Obrigatório | Descrição |
|---|---|---|
| `type` | sim | discriminador |
| `seq` | sim | monotônico por direção |
| `t_ns` | sim | tempo virtual da mensagem |
| `request_id` | não | correlaciona request/result |

Mensagens desconhecidas devem ser ignoradas (forward compatibility) e
registradas em log; mensagens com `type` obrigatório inválido geram
`world.error`.

---

## 4. Handshake

```
SimForge                                  Mundo
   │ world.hello {protocol:1, session, t0}    │
   ├──────────────────────────────────────────►│
   │ world.caps {world_id, name, entities[]}   │
   │◄──────────────────────────────────────────┤
   │ world.ready {t0, rates, capabilities}     │
   ├──────────────────────────────────────────►│
   │                                           │
```

- `world.hello` informa versão de protocolo, ID de sessão, `t0_ns` e versão do
  SimForge.
- `world.caps` lista entidades e limites.
- `world.ready` confirma o início; a sessão só avança depois disso.
- Versão major incompatível → `world.error {code:"protocol_unsupported"}` e
  encerramento.

---

## 5. Sincronização temporal

### 5.1 Barreira

```
SimForge                                  Mundo
   │ world.sync {t_ns: 1000000}               │
   ├──────────────────────────────────────────►│
   │                                           │ processa até t
   │ world.synced {t_ns: 1000000}              │
   │◄──────────────────────────────────────────┤
```

- Em modo determinístico, `world.sync(t)` é obrigatório antes de qualquer
  leitura/escrita em `t`.
- O mundo deve responder `world.synced` com o mesmo `t_ns` ou `world.error`.
- Timeout padrão: 50 ms de tempo de parede por barreira, configurável.
- Se o mundo não sincronizar, a sessão entra em `world.state=stale` e o
  firmware continua com valores seguros (sample-and-hold).

### 5.2 Leitura

```json
{ "type": "world.read", "seq": 10, "t_ns": 1000000,
  "request_id": "r1", "entity": "radar.1" }
```

```json
{ "type": "world.read.result", "seq": 11, "t_ns": 1000000,
  "request_id": "r1", "value": { "speed_mps": 12.5, "valid": true },
  "valid_at": 999000, "quality": "good" }
```

- `valid_at` é o instante em que o valor foi observado.
- `quality`: `good`, `stale`, `unavailable`.
- O SimForge pode usar o valor em `t_ns` mesmo que `valid_at < t_ns`,
  respeitando a política de stale do dispositivo.
- Leituras não bloqueiam o sim thread; o engine usa o último valor e agenda
  nova leitura.

### 5.3 Escrita

```json
{ "type": "world.write", "seq": 12, "t_ns": 1000000,
  "request_id": "w1", "entity": "semaforo.norte",
  "value": "green", "effective_at": 1000000 }
```

```json
{ "type": "world.write.result", "seq": 13, "t_ns": 1000000,
  "request_id": "w1", "applied": true, "effective_at": 1000000 }
```

- `effective_at` permite agendamento; se omitido, aplica em `t_ns`.
- `applied=false` + `reason` indica recusa (estado inválido, segurança).
- Escritas são idempotentes por `request_id` dentro da sessão.

### 5.4 Eventos

```json
{ "type": "world.event", "seq": 14, "t_ns": 1200000,
  "entity": "detector.1",
  "value": { "vehicle_id": "car-7", "direction": "north" },
  "quality": "good" }
```

- Eventos entram na fila do engine como `WorldInput` no `t_ns` informado.
- O mundo não pode enviar eventos com `t_ns` no passado além da janela
  configurada (padrão 10 ms); caso ocorra, o evento é aplicado no instante
  atual e marcado `late=true`.
- Ordem de eventos com o mesmo `t_ns`: `(t_ns, world_id, seq)`.

---

## 6. Erros e recuperação

```json
{ "type": "world.error", "seq": 15, "t_ns": 1200000,
  "request_id": "r1", "code": "entity_not_found",
  "message": "Entidade inexistente", "retryable": false }
```

| Código | Quando |
|---|---|
| `protocol_unsupported` | versão major incompatível |
| `invalid_message` | schema inválido |
| `entity_not_found` | ID desconhecido |
| `value_rejected` | valor fora do schema/estado |
| `busy` | mundo ocupado; tentar novamente |
| `timeout` | operação excedeu o limite |
| `internal_error` | falha do mundo |

- Após 3 timeouts consecutivos, o SimForge marca `world.state=degraded`.
- Após 10, desconecta e aplica valores seguros; o usuário pode reconectar.
- O firmware nunca vê exceções; vê `unavailable` e o valor seguro configurado.

---

## 7. Determinismo e replay

- Em modo determinístico, toda leitura/evento usado pela simulação é gravado no
  trace `.sfrec` com `entity`, `t_ns`, `value`, `quality`.
- Replay não consulta o mundo; usa os valores gravados.
- `world.sync` não é necessário no replay.
- Escritas também são gravadas para auditoria, mas não reexecutadas.
- O hash do trace inclui os inputs externos.

---

## 8. Transporte e segurança

### 8.1 WebSocket

- `ws://127.0.0.1:<port>/world/v1` (loopback) ou `wss://` para remoto.
- Token no handshake (`Authorization: Bearer`) ou primeira mensagem
  `world.hello.token`.
- Origin restrito; CORS não é usado (não é browser).
- Ping/pong a cada 10 s; timeout de 30 s.

### 8.2 Pipe/shared memory

- Mesmas mensagens JSON delimitadas por `\n` em pipe.
- Shared memory para streams de alta frequência (opcional): header de 32 bytes
  + ring buffer SPSC; controle continua por JSON.
- O receptor valida tamanho, sequência e checksum antes de aplicar.

### 8.3 Limites

| Limite | Padrão |
|---|---|
| Entidades | 10.000 |
| Mensagens/s | 10.000 |
| Tamanho de mensagem | 1 MB |
| Valor aninhado | profundidade 16 |
| Timeout de sync | 50 ms |
| Eventos atrasados | 10 ms |

---

## 9. Adaptador PistaForge (externo)

O adaptador não faz parte do SimForge. Ele implementa o protocolo e traduz:

| Entidade externa | Objeto do PistaForge | Conversão |
|---|---|---|
| `traffic_light` | semáforo da cena | estado |
| `barrier` | cancela | `open/closed` + posição |
| `speed_sensor` | radar | `m/s` |
| `proximity` | sensor de presença | `mm` |
| `passage_detector` | detector de passagem | evento com direção |

```
SimForge ── world.* ──► Adaptador ── API do PistaForge ──► Cena
```

O SimForge não sabe que o outro lado é o PistaForge; o adaptador pode ser
substituído por qualquer outro mundo compatível.

---

## 10. Compatibilidade

- `protocol_version` é major; minor é aditivo.
- Entidades desconhecidas são aceitas e ignoradas se não usadas.
- Campos novos em mensagens são ignorados.
- `world.caps` publica `features` para negociação (ex.: `scheduled_write`,
  `binary_stream`).
- Testes de contrato validam cada mensagem contra o schema.
