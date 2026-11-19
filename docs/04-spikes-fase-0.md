# Spikes da Fase 0 — validação técnica antes do MVP

**Status:** em execução
**Duração-alvo:** 5 semanas (1 dev) ou 3 semanas (2 devs)
**Objetivo:** eliminar as incertezas que podem invalidar a arquitetura antes de
investir no MVP de 90 dias.
**Regra:** spike não é produto. O código pode ser descartado; o que fica é a
medição, o aprendizado e, quando o resultado for bom, o núcleo promovido.

---

## 0. Como ler este documento

Cada spike tem:

- **Pergunta** — a incerteza concreta que precisa morrer.
- **Hipótese** — o que a arquitetura assume hoje.
- **Escopo** — o mínimo para responder à pergunta.
- **Critério de saída** — número verificável, não opinião.
- **Artefato** — o que fica no repositório.
- **Risco de falha** — o que muda se o critério não for atingido.

Um spike só é considerado concluído quando o critério é medido e registrado em
`docs/spikes/resultados.md` (uma linha por medição, com máquina e data).

### Grafo de dependências

```
S0.1 scheduler ──► S0.2 solver ──► S0.6 performance
       │                │
       ├──► S0.4 .sforge│
       │                │
       ├──► S0.5 API ───┴──► S0.7 world.*
       │
       └──► S0.3 WASM ──► S0.8 build
```

`S0.1` e `S0.5` são pré-requisitos de tudo: sem scheduler determinístico e sem
bus de comandos estável, nenhuma medição posterior é confiável.

---

## S0.1 — Scheduler determinístico com fases e delta cycles

**Pergunta:** a fila `(time_ns, phase, seq)` com no máximo 64 delta cycles
resolve realimentação combinacional de forma determinística e rápida o
suficiente para 60 Hz?

**Hipótese:** sim. Ordem total por `seq` monotônico garante reprodutibilidade
sem depender de ordem de containers ou de endereço de memória.

**Escopo**

1. `SimTime` = `int64` ns e conversões explícitas.
2. Fila de prioridade com chave `(t_ns, phase, seq)`; `seq` nunca reutilizado.
3. Fases `PreInput → Input → Evaluate → Settle → Commit → PostCommit`.
4. Detecção de delta cycle: eventos gerados no mesmo `t_ns` durante
   `Evaluate/Settle`; limite 64; ao exceder, `sim_oscillation` com a lista de
   nets instáveis.
5. Hash incremental do trace (FNV-1a 64) sobre `(t_ns, phase, kind, target)`.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Duas execuções do mesmo cenário produzem o mesmo hash | obrigatório |
| Hash idêntico com `-O0` e `-O2` | obrigatório |
| 1 M eventos drenados | ≤ 150 ms em um núcleo de 3 GHz |
| Inserção + remoção em fila com 100 k eventos pendentes | ≤ 2 µs por operação |
| Cenário com realimentação estável (2 inversores em anel) | converge em ≤ 6 delta cycles |
| Anel instável detectado | ≤ 64 delta cycles, erro com nets |

**Artefato:** `src/core/event/`, `tests/determinism/`, bench em `bench/`
(`simforge_bench`; fora do ctest de propósito).

**Risco de falha:** se o limite de 64 gerar falso positivo em PWM de alta
frequência, a saída é contar delta cycles por net (não globalmente), mantendo
o limite global como watchdog.

---

## S0.2 — LogicSolver de quatro estados e resolução de net

**Pergunta:** um solver lógico com forças (`strong`, `weak`, `Z`, `X`) cobre
LED, botão, pull-up/down, I2C/SPI e PWM com custo desprezível?

**Hipótese:** sim para o MVP; analógico entra depois como `MnaSolver`
(ADR 0005).

**Escopo**

1. `PinState { level ∈ {Low, High, Z, X}, strength ∈ {None, Weak, Strong} }`.
2. Resolução de net por wire-AND:
   - qualquer `Strong Low` → `Low`;
   - `Strong High` + `Strong Low` → `X` (contenção, `E_CONTENTION`);
   - só `Weak` → nível fraco;
   - todos `Z` → `Z` (flutuante, `W_FLOATING`).
3. Dispositivos embutidos: LED (brilho = duty), botão (pull-up, bounce),
   resistor (passa nível), chave.
4. PWM por software: bordas agendadas no scheduler; brilho = duty médio na
   janela.
5. Contadores de contenção por net, sem alocação no caminho quente.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Tabela-verdade de resolução (16 combinações) | 100% dos casos |
| Net com 200 drivers alternando | ≤ 5 µs por avaliação |
| Cenário `button-led` (1 botão, 1 LED, 10 s virtuais) | ≤ 20 ms de CPU |
| PWM 1 kHz em 8 canais | ≤ 3% de um núcleo a 60 Hz |
| Contenção detectada e reportada | 100% dos casos do teste |

**Artefato:** `src/core/electrical/`, `src/devices/`, `tests/unit/electrical*`.

**Risco de falha:** se PWM por software estourar o orçamento, migrar PWM para
periférico do board model (timer dedicado, bordas em lote) — muda o
`BoardFramework`, não o solver.

---

## S0.3 — Shim Arduino compilado para WASM

**Pergunta:** um sketch Arduino não trivial compila para WASM com `wasmtime` e
roda com GPIO/tempo virtuais, respeitando watchdog e determinismo?

**Hipótese:** sim, desde que o subconjunto do shim seja explícito e o
`delay()` avance tempo virtual.

**Escopo**

1. Shim mínimo: `pinMode`, `digitalWrite`, `digitalRead`, `delay`, `millis`,
   `micros`, `Serial.print/println`, `analogWrite`, `analogRead`.
2. Host imports: `sf_gpio_write`, `sf_gpio_read`, `sf_delay_ns`,
   `sf_millis`, `sf_serial_write`.
3. Execução com fuel por fatia + epoch deadline; loop sem yield → `firmware_hang`.
4. `delay()` avança o relógio virtual e devolve o controle ao scheduler.

**Critério de saída**

| Métrica | Limite |
|---|---|
| `blink.ino` (LED 500 ms) gera bordas em 500 000 000 ns ± 1 µs | exato |
| `button-led.ino` reage a botão em ≤ 2 ms virtuais | obrigatório |
| Loop infinito sem `delay` detectado | ≤ 1 s de parede |
| Consumo de memória do runtime | ≤ 16 MB por sessão |
| Overhead vs. nativo (mesmo sketch) | ≤ 3× |

**Artefato:** `src/compiler/shim/`, `src/runtime/wasm/`, `samples/blink/`.

**Risco de falha:** se o overhead passar de 3×, manter WASM como padrão em
nuvem/web e abrir caminho nativo em processo separado para desktop (já previsto
em ADR 0006), sem mudar a API.

---

## S0.4 — `.sforge` round-trip e validação

**Pergunta:** o formato JSON com IDs estáveis, topologia separada da geometria
e preservação de campos desconhecidos sobrevive a um ciclo completo de
abertura, edição e gravação sem perda?

**Hipótese:** sim, com um DOM que preserve ordem e campos desconhecidos.

**Escopo**

1. Parser/serializador JSON próprio (sem dependência externa na fase 0).
2. Modelo de projeto: `project`, `boards`, `devices`, `nets`, `buses`,
   `wires`, `firmware`, `simulation`, `layout`, `extensions`.
3. Validação em camadas: sintática → referencial → consistência → ERC.
4. Migração `1 → 1` (identidade) com golden files prontos para o futuro.
5. Escrita atômica (`tmp` + rename) e canônica.

**Critério de saída**

| Métrica | Limite |
|---|---|
| `abrir → salvar → abrir` produz documento semanticamente idêntico | 100% |
| Campos desconhecidos preservados em 3 níveis de aninhamento | 100% |
| Exemplo de `docs/03-plataforma.md` abre sem erro | obrigatório |
| Projeto com 10 k objetos carrega | ≤ 250 ms |
| Arquivo com referência quebrada rejeitado com diagnóstico localizado | obrigatório |

**Artefato:** `src/persistence/project/`, `tests/golden/`.

**Risco de falha:** se a preservação de desconhecidos custar mais de 15% do
tempo de carga, guardar o "resto" apenas por objeto alterado (lazy) e não no
parse.

---

## S0.5 — Bus de comandos único com C ABI e WebSocket

**Pergunta:** um único `CommandBus` serve C ABI, WebSocket e CLI sem duplicar
validação, mantendo erros e eventos idênticos?

**Hipótese:** sim; transporte é só framing.

**Escopo**

1. Envelope `id/method/params/meta` + `ok/result` + `error{code,...}`.
2. Dispatch por tabela, validação de `params` por método, códigos estáveis.
3. `idempotency_key` com cache de 60 s e `base_revision` com `409 conflict`.
4. Eventos com `seq` monotônico, `events.poll` com `after_seq` e `dropped`.
5. C ABI `sf_session_*` com buffer de resposta e `SF_ERR_BUFFER`.
6. CLI NDJSON sobre o mesmo bus.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Mesma requisição por C ABI e CLI produz `result` idêntico | 100% |
| Erro desconhecido nunca vaza como `internal_error` | 100% |
| Requisição de 1 KB | ≤ 20 µs de CPU (sem I/O) |
| 10 k eventos drenados por `events.poll` | ≤ 50 ms |
| Buffer pequeno na C ABI devolve tamanho necessário | obrigatório |

**Artefato:** `src/api/`, `include/simforge/c_api/`, `src/app/cli/`,
`tests/contracts/`.

**Risco de falha:** se o custo por requisição passar de 20 µs, separar
validação em tabela pré-compilada por método (sem reflexão em runtime).

---

## S0.6 — Orçamento de performance do frame de renderização

**Pergunta:** é possível montar um frame de 60 Hz com 10 k nets e 1 k
dispositivos em menos de 4 ms, incluindo serialização binária?

**Hipótese:** sim, se o frame for incremental e o cliente receber apenas o que
mudou.

**Escopo**

1. Snapshot de frame: estados de pino, brilho de LED, displays sujos.
2. Coalescência em 60 Hz independente do passo da simulação.
3. Codificação binária `SFFR` v1 (header + payload tipado) e fallback JSON.
4. Medição de tamanho de payload em cenários realistas.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Montar frame completo (1 k devices) | ≤ 4 ms |
| Frame incremental sem mudanças | ≤ 200 µs |
| Payload binário para 1 k devices | ≤ 32 KB |
| Payload JSON equivalente | ≤ 8× o binário |
| Jitter de publicação a 60 Hz | p99 ≤ 2 ms |

**Artefato:** `src/core/instrumentation/frame`, bench em `bench/`.

**Risco de falha:** se o frame completo estourar 4 ms, o renderer passa a
consumir apenas deltas e o frame completo vira snapshot sob demanda.

---

## S0.7 — Integração `world.*` com um mundo falso

**Pergunta:** a barreira `world.sync(t)` mais sample-and-hold mantém
determinismo com latência e perda de mensagens?

**Hipótese:** sim, desde que todo input usado seja gravado e o replay não
consulte o mundo.

**Escopo**

1. Servidor fake de mundo (processo separado) com entidades
   `sensor`/`actuator`.
2. Handshake `hello/caps/ready`, `sync`, `read`, `write`, `event`, `error`.
3. Timeout de 50 ms, degradação após 3 falhas, desconexão após 10.
4. Gravação dos inputs no trace e replay sem o mundo.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Replay com mundo desligado reproduz o mesmo hash | obrigatório |
| Barreira com mundo local | ≤ 1 ms por `sync` |
| Mundo que para de responder | estado `stale` em ≤ 150 ms |
| Evento atrasado 20 ms | aplicado no presente e marcado `late` |
| 1 k entidades no `caps` | handshake ≤ 100 ms |

**Artefato:** `src/integration/external_world/`, `tests/integration/world*`.

**Risco de falha:** se a barreira dominar o tempo de parede, trocar para
sync em lote a cada N ms virtuais, com janela de tolerância documentada.

---

## S0.8 — Build isolado com cache por hash

**Pergunta:** compilar Arduino em subprocesso com cache content-addressed é
rápido o bastante para o ciclo de edição (≤ 3 s no caso quente)?

**Hipótese:** sim; o gargalo é o primeiro build, não o incremental.

**Escopo**

1. Subprocesso com timeout, cota de disco e sem rede.
2. Hash de entrada: fontes, bibliotecas, flags, placa, versão da toolchain.
3. Cache `builds/<sha256>/` com `.elf`, `.bin`, mapa e diagnósticos.
4. Parse de diagnósticos para `{file, line, column, severity, code, message}`.
5. Cancelamento e limpeza de cache por LRU.

**Critério de saída**

| Métrica | Limite |
|---|---|
| Cache hit | ≤ 200 ms |
| Build incremental (1 arquivo alterado) | ≤ 3 s |
| Build limpo de `blink` | ≤ 20 s |
| Build travado | cancelado em ≤ 5 s |
| Escrita fora do diretório de build | 0 ocorrências |

**Artefato:** `src/compiler/`, `src/firmware/build_cache`.

**Risco de falha:** se o build limpo passar de 20 s, manter cache aquecido
por template de projeto e pré-compilar o core Arduino por placa.

---

## 1. Cronograma

| Semana | Spikes | Marco |
|---|---|---|
| 1 | S0.1, S0.4 | Núcleo determinístico + projeto ida-e-volta |
| 2 | S0.2, S0.5 | Circuito funcional + API única |
| 3 | S0.3, S0.8 | Firmware rodando + build cacheado |
| 4 | S0.6, S0.7 | Frame a 60 Hz + mundo externo |
| 5 | folga, medições finais, `spikes/resultados.md` | Go/no-go |

## 2. Critérios de go/no-go

O MVP começa apenas se **todos** forem verdadeiros:

1. S0.1: hash idêntico em duas execuções e entre `-O0`/`-O2`.
2. S0.2: tabela-verdade completa e contenção detectada.
3. S0.5: paridade C ABI ↔ CLI.
4. S0.4: round-trip sem perda, incluindo campos desconhecidos.

`S0.3`, `S0.6`, `S0.7` e `S0.8` podem falhar e virar item de roadmap, desde que
o caminho alternativo já esteja descrito no ADR correspondente.

## 3. O que muda a arquitetura

| Se falhar | Consequência |
|---|---|
| S0.1 | Scheduler vira passo fixo com sub-passos; ADR 0004 é reescrito |
| S0.2 | `LogicSolver` sai do MVP; MVP começa com `MnaSolver` reduzido |
| S0.3 | Firmware nativo em processo separado vira padrão no desktop |
| S0.4 | Formato migra para binário CBOR com dump JSON para diff |
| S0.5 | C ABI e WS ganham validação separada (dívida aceita) |
| S0.6 | Renderer só por deltas; frame completo sob demanda |
| S0.7 | Integração vira offline por arquivo de cenário |
| S0.8 | Build passa a exigir toolchain fixa empacotada |

## 4. Registro de resultados

Cada medição entra em `docs/spikes/resultados.md` com:

```
| spike | métrica | valor | limite | máquina | data |
```

Sem essa linha, o spike não conta como concluído — mesmo que o código exista.
