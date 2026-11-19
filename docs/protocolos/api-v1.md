# SimForge API v1 — Contrato de Comandos e Eventos

**Status:** proposta congelável
**Versão:** 1.0
**Transportes:** WebSocket, C ABI, bindings WASM, CLI (NDJSON)
**Schema do envelope:** [`api-v1.schema.json`](api-v1.schema.json)

---

## 1. Princípios

1. Toda mutação passa por um comando nomeado; não há acesso direto a arquivos
   ou estado interno.
2. O servidor é a autoridade. O cliente envia comandos e projeta o resultado.
3. Comandos são serializados por sessão. Leituras podem ser concorrentes.
4. Eventos são push (WebSocket) ou pull (`events.poll`) nos demais transportes.
5. Unidades: tempo em **ns inteiros**, tensão em **V**, corrente em **A**,
   resistência em **Ω**, esquemático em **unidades de grade** (`grid_mm`).
6. Toda resposta carrega `revision` quando o comando altera o projeto.
7. IDs são strings ULID; nunca números (evita perda de precisão em JS).
8. Campos desconhecidos em `params` são rejeitados; campos novos em `result`
   devem ser ignorados pelo cliente.

> Nos exemplos, alguns IDs aparecem abreviados para legibilidade. Em produção,
> IDs de projeto, placa, dispositivo e job são ULIDs de 26 caracteres.

---

## 2. Envelope

### 2.1 Requisição

```json
{
  "id": "01J9Z8A1B2C3D4E5F6G7H8J9K0",
  "method": "circuit.connect",
  "params": {
    "a": { "board": "01J9Z6M0A1", "pin": "GPIO2" },
    "b": { "device": "01J9Z6N7B2", "pin": "A" },
    "base_revision": 41
  },
  "meta": {
    "api_version": 1,
    "client": "web/0.1.0",
    "idempotency_key": "connect-7f3a"
  }
}
```

| Campo | Tipo | Obrigatório | Regra |
|---|---|---|---|
| `id` | string | sim | ULID ou UUID; ecoado na resposta |
| `method` | string | sim | `^[a-z][a-zA-Z0-9]*(\.[a-zA-Z][a-zA-Z0-9]*)+$` |
| `params` | object | não | padrão `{}` |
| `meta.api_version` | integer | não | padrão `1`; major diferente é recusado |
| `meta.idempotency_key` | string | não | mutações; cache de 60 s |
| `meta.client` | string | não | telemetria |

### 2.2 Resposta de sucesso

```json
{
  "id": "01J9Z8A1B2C3D4E5F6G7H8J9K0",
  "ok": true,
  "result": { "net_id": "N1", "revision": 42 },
  "meta": { "revision": 42, "t_ns": 0, "trace_id": "tr_7f3a" }
}
```

### 2.3 Resposta de erro

```json
{
  "id": "01J9Z8A1B2C3D4E5F6G7H8J9K0",
  "ok": false,
  "error": {
    "code": "conflict",
    "message": "A revisão do projeto mudou.",
    "details": { "expected": 41, "current": 43 },
    "retryable": false,
    "trace_id": "tr_7f3a"
  }
}
```

- `code` é estável e documentado; `message` é para humanos e pode mudar.
- `details` é específico do código.
- `retryable=true` indica que repetir com o mesmo `idempotency_key` é seguro.

### 2.4 Evento

```json
{
  "seq": 1001,
  "t_ns": 250000000,
  "type": "sim.state",
  "payload": { "state": "running", "t_ns": 250000000, "revision": 42 }
}
```

- `seq` é monotônico por sessão; lacunas indicam eventos descartados por
  backpressure (`events.poll` informa `dropped`).
- `t_ns` é o tempo virtual do evento (não o tempo de parede).

### 2.5 Job assíncrono

```json
{ "job_id": "job_01J9Z8A1B2C3D4E5F6G7H8J9K0", "status": "running", "progress": 0.42 }
```

Comandos lentos (`firmware.build`, `ai.propose`, `project.open`) retornam
`job_id`. O resultado final chega por `job.finished` ou `jobs.get`.

---

## 3. Concorrência e idempotência

- Toda mutação de projeto aceita `base_revision`.
  - igual à revisão atual: aplica;
  - diferente: `409 conflict` com `details.current`;
  - ausente: aplica sobre a revisão atual (last-write-wins) e retorna aviso em
    `meta.warnings`.
- `idempotency_key` é escopado à sessão e ao método; repetição devolve o mesmo
  resultado sem reaplicar.
- Comandos de simulação (`sim.*`) são aplicados no próximo ponto seguro do
  engine; a resposta pode ser `accepted` antes da execução.
- `sim.run` não bloqueia; o estado evolui por eventos.

---

## 4. Catálogo de métodos

### 4.1 `app.*`

| Método | Params | Result |
|---|---|---|
| `app.ping` | `{nonce?}` | `{nonce?, t_ns}` |
| `app.info` | `{}` | `{product, version, api_version, engine_version, platform, capabilities[]}` |
| `app.authenticate` | `{token}` | `{session_id, permissions[]}` |
| `app.settings.get` | `{key?}` | `{values:{}}` |
| `app.settings.set` | `{values:{}}` | `{values:{}}` |
| `app.shutdown` | `{force?}` | `{ok}` |

`app.info.capabilities` exemplos: `sim.wasm`, `sim.qemu`, `compiler.arduino`,
`plugin.hot_reload`, `world.websocket`, `rec.replay`, `ai.local`.

### 4.2 `project.*`

| Método | Params | Result |
|---|---|---|
| `project.new` | `{name?, template?, board?}` | `{project_id, revision, path?}` |
| `project.open` | `{path}` | `{job_id}` ou `{project_id, revision, name, diagnostics[]}` |
| `project.save` | `{path?}` | `{path, revision}` |
| `project.saveAs` | `{path}` | `{path, revision}` |
| `project.close` | `{discard?}` | `{closed}` |
| `project.state` | `{}` | `{document, revision, path, dirty}` |
| `project.applyPatch` | `{base_revision, patch[]}` | `{revision, applied, diagnostics[]}` |
| `project.undo` | `{}` | `{revision, can_undo, can_redo}` |
| `project.redo` | `{}` | `{revision, can_undo, can_redo}` |
| `project.validate` | `{}` | `{diagnostics[]}` |
| `project.recent` | `{}` | `{items:[{path,name,opened_at}]}` |
| `project.export` | `{format, path?}` | `{files[], format}` |

`project.applyPatch` usa JSON Patch (RFC 6902) restrito a caminhos do documento.
Operações que alteram topologia são validadas como um único commit; se qualquer
uma falhar, nada é aplicado.

Todo `path` no protocolo é **UTF-8**, independente de plataforma. No Windows o
núcleo converte para `std::filesystem::path` via `simforge/paths.hpp`; sem isso,
pastas com acento (“Área de Trabalho”) não abrem. Valide caminhos com
`project.open` enviando o JSON em UTF-8 — nunca por argumento de linha de
comando, que no Windows passa pelo code page ANSI.

### 4.3 `circuit.*`

| Método | Params | Result |
|---|---|---|
| `circuit.addBoard` | `{type, type_version?, position, rotation?, params?}` | `{board_id, revision}` |
| `circuit.removeBoard` | `{board_id, base_revision?}` | `{revision}` |
| `circuit.addDevice` | `{type, type_version?, position, rotation?, params?, id?}` | `{device_id, revision}` |
| `circuit.removeDevice` | `{device_id, base_revision?}` | `{revision}` |
| `circuit.setDevice` | `{device_id, position?, rotation?, params?, merge?, base_revision?}` | `{device_id, revision}` |
| `circuit.connect` | `{a, b, net?:{id?,name?,kind?}, base_revision?}` | `{net_id, revision}` |
| `circuit.disconnect` | `{a, base_revision?}` | `{removed, revision}` |
| `circuit.addNet` | `{name?, kind, base_revision?}` | `{net_id, revision}` |
| `circuit.removeNet` | `{net_id, base_revision?}` | `{revision}` |
| `circuit.setNet` | `{net_id, name?, kind?, base_revision?}` | `{net_id, revision}` |
| `circuit.attachPin` | `{net_id, pin, base_revision?}` | `{revision}` |
| `circuit.detachPin` | `{pin, base_revision?}` | `{revision}` |
| `circuit.addWire` | `{net_id, points, color?, base_revision?}` | `{wire_id, revision}` |
| `circuit.removeWire` | `{wire_id, base_revision?}` | `{revision}` |
| `circuit.addBus` | `{kind, controller, signals, clock_hz?, targets?, base_revision?}` | `{bus_id, revision}` |
| `circuit.removeBus` | `{bus_id, base_revision?}` | `{revision}` |
| `circuit.pick` | `{point, tolerance?}` | `{hit:{kind,id,pin?,distance}}` |
| `circuit.bounds` | `{}` | `{min:{x,y}, max:{x,y}}` |
| `circuit.erc` | `{strict?}` | `{diagnostics[], clean}` |

`pin` é sempre `{ "board": "<id>", "pin": "<nome>" }` ou
`{ "device": "<id>", "pin": "<nome>" }`.

Em `circuit.addBus`, `signals` é um mapa papel → net, por exemplo
`{ "sda": "N10", "scl": "N11" }` para I2C ou
`{ "sclk": "N20", "mosi": "N21", "miso": "N22" }` para SPI. Os nomes de papel
são os mesmos do formato `.sforge`.

`circuit.connect` cria a net se ela não existir; se `a` ou `b` já estiverem em
outra net, retorna `conflict` com `details.pins` (o cliente deve desconectar
primeiro, explicitamente).

`merge=true` agrupa alterações contínuas em um único passo de undo (arrastos).

### 4.4 `sim.*`

| Método | Params | Result |
|---|---|---|
| `sim.load` | `{revision?}` | `{session_id, state, diagnostics[]}` |
| `sim.run` | `{mode?, until_ns?, real_time?}` | `{state, t_ns}` |
| `sim.pause` | `{}` | `{state, t_ns}` |
| `sim.resume` | `{}` | `{state, t_ns}` |
| `sim.stop` | `{}` | `{state, t_ns}` |
| `sim.reset` | `{keep_firmware?}` | `{state, t_ns}` |
| `sim.step` | `{ns?, events?}` | `{t_ns, events_processed}` |
| `sim.state` | `{}` | `{state, t_ns, speed, revision, runtime, stats}` |
| `sim.frame` | `{}` | `{seq, t_ns, frame}` |
| `sim.setSpeed` | `{factor}` | `{speed}` |
| `sim.setSeed` | `{seed}` | `{seed}` |
| `sim.probe.add` | `{target, kind, name?}` | `{probe_id}` |
| `sim.probe.remove` | `{probe_id}` | `{removed}` |
| `sim.probe.read` | `{probe_id, since_ns?}` | `{samples:[{t_ns,value}]}` |
| `sim.debug.setPin` | `{target, level}` | `{t_ns}` |
| `sim.debug.releasePin` | `{target}` | `{t_ns}` |

`sim.debug.*` injeta estímulo elétrico direto em um pino de placa
(`target` = `{ "board": "<id>", "pin": "<nome>" }`), simulando a bancada e o
runtime sem firmware. `level` ∈ `low`, `high`, `z`, `x`. É uma ferramenta de
teste/CI: não persiste no projeto e não substitui `firmware.*`.

`sim.state.state`: `idle`, `loading`, `ready`, `running`, `paused`, `stopped`,
`error`.

`sim.frame` é um snapshot compacto para renderização: posições, estados de
pinos, brilho de LEDs, displays sujos, texto serial. Nunca contém regra de
negócio.

### 4.5 `firmware.*`

| Método | Params | Result |
|---|---|---|
| `firmware.build` | `{board_id, entry?, backend?, libraries?, defines?, profile?}` | `{job_id}` |
| `firmware.cancel` | `{job_id}` | `{cancelled}` |
| `firmware.load` | `{artifact_id}` | `{state, artifact}` |
| `firmware.status` | `{}` | `{state, artifact, diagnostics[], memory}` |
| `firmware.serial.write` | `{data, encoding?}` | `{written}` |
| `firmware.serial.read` | `{max_bytes?}` | `{data, encoding, dropped}` |
| `firmware.symbols` | `{query?}` | `{symbols[]}` |

`firmware.serial.write.data` é string; `encoding` = `utf8` (padrão) ou `base64`.

### 4.6 `device.*` e `board.*`

| Método | Params | Result |
|---|---|---|
| `device.catalog` | `{query?, category?, source?}` | `{devices:[{type,type_version,name,category,pins[]}]}` |
| `device.describe` | `{type, version?}` | `{descriptor}` |
| `board.catalog` | `{}` | `{boards:[{type,type_version,name,mcu,capabilities[]}]}` |
| `board.describe` | `{type, version?}` | `{descriptor}` |

### 4.7 `plugin.*`

| Método | Params | Result |
|---|---|---|
| `plugin.list` | `{}` | `{plugins:[{id,version,kind,state,trust}]}` |
| `plugin.install` | `{path, trust?}` | `{job_id}` |
| `plugin.uninstall` | `{id, version?}` | `{removed}` |
| `plugin.enable` | `{id}` | `{state}` |
| `plugin.disable` | `{id}` | `{state}` |
| `plugin.reload` | `{id}` | `{state}` |

### 4.8 `world.*`

| Método | Params | Result |
|---|---|---|
| `world.connect` | `{endpoint, protocol?, token?, timeout_ms?}` | `{world_id, info, entities[]}` |
| `world.disconnect` | `{world_id}` | `{disconnected}` |
| `world.list` | `{}` | `{worlds[]}` |
| `world.entities` | `{world_id}` | `{entities[]}` |
| `world.read` | `{world_id, entity}` | `{value, valid_at, quality}` |
| `world.write` | `{world_id, entity, value}` | `{applied, effective_at}` |
| `world.subscribe` | `{world_id, entities?}` | `{subscription_id}` |
| `world.unsubscribe` | `{subscription_id}` | `{removed}` |
| `world.status` | `{world_id}` | `{state, latency_ms, last_error}` |

Detalhes do protocolo em [`world-v1.md`](world-v1.md).

### 4.9 `rec.*`

| Método | Params | Result |
|---|---|---|
| `rec.start` | `{path?, include_frames?}` | `{recording_id}` |
| `rec.stop` | `{}` | `{recording_id, path, duration_ns, hash}` |
| `rec.load` | `{path}` | `{recording_id, info}` |
| `rec.play` | `{speed?}` | `{state, t_ns}` |
| `rec.pause` | `{}` | `{state, t_ns}` |
| `rec.resume` | `{}` | `{state, t_ns}` |
| `rec.stopPlayback` | `{}` | `{state}` |
| `rec.seek` | `{t_ns}` | `{t_ns}` |
| `rec.state` | `{}` | `{state, t_ns, duration_ns, speed}` |
| `rec.export` | `{format, path, probes?}` | `{files[]}` |

### 4.10 `events.*`

| Método | Params | Result |
|---|---|---|
| `events.subscribe` | `{topics[], max_rate_hz?}` | `{subscription_id}` |
| `events.unsubscribe` | `{subscription_id}` | `{removed}` |
| `events.poll` | `{after_seq?, max?, timeout_ms?}` | `{events[], next_seq, dropped}` |

Tópicos: `project.*`, `circuit.*`, `sim.*`, `firmware.*`, `plugin.*`,
`world.*`, `rec.*`, `jobs.*`, `ai.*`.

### 4.11 `jobs.*` e `ai.*`

| Método | Params | Result |
|---|---|---|
| `jobs.get` | `{job_id}` | `{job_id, status, progress, result?, error?}` |
| `jobs.cancel` | `{job_id}` | `{cancelled}` |
| `jobs.list` | `{}` | `{jobs[]}` |
| `ai.status` | `{}` | `{providers[], local_available, policy}` |
| `ai.propose` | `{task, prompt, context?}` | `{job_id}` |
| `ai.apply` | `{proposal_id, ops?}` | `{revision, applied}` |
| `ai.cancel` | `{job_id}` | `{cancelled}` |
| `ai.explain` | `{trace_id?, range?}` | `{job_id}` |

`ai.propose` nunca altera o projeto. Retorna `proposal_id`; `ai.apply` aplica
o patch aprovado com validação e `base_revision`.

---

## 5. Eventos

| Tipo | Payload | Frequência |
|---|---|---|
| `project.changed` | `{revision, kinds[]}` | por commit |
| `circuit.changed` | `{revision, added[], removed[], updated[]}` | por commit |
| `sim.state` | `{state, t_ns, revision}` | transições |
| `sim.tick` | `{t_ns, seq, stats}` | limitada por `max_rate_hz` |
| `sim.frame` | `{seq, t_ns, frame}` | 60 Hz (coalescido) |
| `sim.error` | `{code, message, t_ns, details}` | por ocorrência |
| `firmware.build.progress` | `{job_id, phase, percent}` | por fase |
| `firmware.build.finished` | `{job_id, status, artifact_id, diagnostics[]}` | fim |
| `firmware.state` | `{state}` | transições |
| `firmware.serial.data` | `{stream, data, encoding}` | por chunk |
| `plugin.changed` | `{id, state}` | instalação/reload |
| `world.event` | `{world_id, entity, value, t_ns}` | por evento |
| `world.state` | `{world_id, state, latency_ms}` | transições |
| `rec.state` | `{state, t_ns}` | transições |
| `job.progress` | `{job_id, progress}` | por job |
| `job.finished` | `{job_id, status, result?, error?}` | fim |
| `ai.proposal` | `{proposal_id, summary, patch, diagnostics[]}` | fim |

Backpressure:

- `sim.tick` e `sim.frame` são coalescidos: o cliente recebe o mais recente.
- Eventos de estado nunca são descartados; se a fila encher, a sessão pausa a
  UI até o cliente consumir (`sim.state=throttled`).
- `events.poll` retorna `dropped` quando houve descarte.

---

## 6. Códigos de erro

| Código | HTTP* | Retryable | Quando |
|---|---|---|---|
| `invalid_request` | 400 | não | envelope inválido |
| `unknown_method` | 404 | não | método não registrado |
| `invalid_params` | 400 | não | schema/valores inválidos |
| `not_found` | 404 | não | ID inexistente |
| `conflict` | 409 | não | revisão/pino já conectado |
| `invalid_state` | 409 | não | comando incompatível com estado |
| `busy` | 409 | sim | simulação/compilação em andamento |
| `unsupported` | 422 | não | recurso não suportado no host |
| `schema_unsupported` | 422 | não | schema/ABI futuro |
| `project_invalid` | 422 | não | projeto corrompido |
| `erc_failed` | 422 | não | erros elétricos bloqueantes |
| `sim_not_running` | 409 | não | step/pause sem run |
| `sim_oscillation` | 422 | não | realimentação combinacional |
| `sim_analog_diverged` | 422 | não | solver não convergiu |
| `firmware_build_failed` | 422 | não | build com erros |
| `firmware_hang` | 422 | não | watchdog |
| `runtime_crashed` | 500 | sim | sandbox falhou |
| `plugin_rejected` | 422 | não | ABI/assinatura inválida |
| `plugin_crashed` | 500 | sim | plugin falhou |
| `world_unavailable` | 503 | sim | mundo offline |
| `world_timeout` | 504 | sim | sync/read timeout |
| `io_error` | 500 | sim | disco/rede |
| `permission_denied` | 403 | não | token/permissão |
| `resource_exhausted` | 429 | sim | memória/fuel/quota |
| `internal_error` | 500 | sim | bug |

\* HTTP é usado apenas pelo gateway; WebSocket e C ABI usam o mesmo `code`.

---

## 7. Transportes

### 7.1 WebSocket

- Endpoint: `ws://127.0.0.1:<port>/v1`.
- `Authorization: Bearer <token>` no handshake ou `app.authenticate`.
- Origin restrito ao app; conexões de outras origens são recusadas.
- Mensagens de texto = JSON; mensagens binárias = frames de alta frequência.
- Ping/pong a cada 15 s; sessão encerrada após 60 s sem resposta.

### 7.2 C ABI

```c
typedef struct sf_session sf_session;

int sf_session_create(const char* config_json, sf_session** out);
int sf_session_execute(sf_session*, const char* request_json,
                       char* response, size_t capacity, size_t* written);
int sf_session_subscribe(sf_session*, const char* topics,
                         void (*cb)(void*, const char* event_json), void* user);
void sf_session_destroy(sf_session*);
```

- `sf_session_execute` é thread-safe; comandos são serializados internamente.
- `response` é UTF-8, terminado em `\0`; se `capacity` for insuficiente,
  retorna `SF_ERR_BUFFER` e `written` com o tamanho necessário.
- A ABI é C; a implementação pode ser C++.

### 7.3 WASM

- Mesmos métodos, expostos por `simforge_execute(ptr, len)`.
- Eventos via callback `simforge_emit`.
- Sem fs/rede; o host fornece assets e persistência por imports explícitos.

### 7.4 CLI

`simforge-cli --api-stdio` lê NDJSON no stdin e escreve NDJSON no stdout.
`simforge-cli run --project x.sforge --script test.js` para CI.

---

## 8. Canal binário de frames

Para 60 Hz com muitos dispositivos, `sim.frame` pode ser enviado como binário:

```
offset  size  campo
0       4     magic "SFFR"
4       2     version = 1
6       2     kind (1=frame, 2=waveform, 3=serial)
8       8     seq
16      8     t_ns
24      4     payload_len
28      ...   payload (flatbuffers ou arrays tipados)
```

- O payload é descrito por `sim.frame.schema` (publicado em `app.info`).
- O cliente pode ignorar o binário e usar JSON; ambos contêm o mesmo estado.
- Não há regra no cliente: ele apenas copia valores para o renderer.

---

## 9. Compatibilidade

- `api_version` é major. O servidor recusa major diferente.
- Métodos novos podem ser adicionados em minor; clientes ignoram desconhecidos.
- Campos novos em `result`/payload são aditivos.
- Remoção ou mudança de semântica exige major novo e período de depreciação.
- `app.info` publica `capabilities` para o cliente adaptar a UI.
- Contratos têm testes de schema e golden files no CI.
