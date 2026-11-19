# Resultados dos spikes

Uma linha por medição. Sem medição registrada, o spike não conta como
concluído. Ver [`../04-spikes-fase-0.md`](../04-spikes-fase-0.md).

Máquina de referência: `Windows 11 · MSVC 19.51 (VS 18) · x86-64 · build Release`.
Data da campanha funcional: 2026-11-18.

> **Nota de ambiente.** O WDAC/Smart App Control desta máquina bloqueia
> executáveis grandes recém-compilados no preset `dev` (o `.exe` de teste Debug
> tem ~1,9 MB). As medições usam o preset `release` (~0,5 MB), que executa
> normalmente. Ainda assim, logo após um link o Windows pode recusar o binário
> por alguns instantes (`spawn UNKNOWN` / “política de Controle de Aplicativo
> bloqueou”); recompilar ou executar uma vez pelo PowerShell resolve. A ponte do
> frontend tenta o spawn até 4 vezes justamente por isso. É uma restrição da
> máquina de desenvolvimento, não do produto.

## Resultados funcionais (verificados por teste automatizado)

| spike | métrica | valor | limite | máquina | data |
|---|---|---|---|---|---|
| S0.1 | hash igual em duas execuções independentes | ok (`engine_deterministic_hash`) | obrigatório | dev/ref | 2026-11-15 |
| S0.1 | reset restaura hash inicial | ok (`engine_reset_restores_initial_hash`) | obrigatório | dev/ref | 2026-11-15 |
| S0.2 | tabela-verdade de resolução (16 combinações) | 16/16 (`electrical_truth_table_16`) | 100% | dev/ref | 2026-11-15 |
| S0.2 | contenção forte↔forte reportada como `x` | ok (`engine_contention_resolves_to_x`) | 100% | dev/ref | 2026-11-15 |
| S0.4 | `abrir → salvar → abrir` idêntico | 100% (`project_roundtrip_preserves_document`) | 100% | dev/ref | 2026-11-15 |
| S0.4 | campos desconhecidos preservados | ok (3 níveis) | 100% | dev/ref | 2026-11-15 |
| S0.4 | referência quebrada rejeitada com diagnóstico | ok (`E_UNKNOWN_OWNER`) | obrigatório | dev/ref | 2026-11-15 |
| S0.4 | exemplo `.sforge` distribuído valida no schema e no núcleo | ok (`examples/blink-led.sforge`) | obrigatório | dev/ref | 2026-11-15 |
| S0.5 | envelope, erros estáveis e idempotência | ok (`test_session`) | 100% | dev/ref | 2026-11-15 |
| S0.5 | CLI NDJSON sobre o mesmo bus | ok (manual + `test_session`) | 100% | dev/ref | 2026-11-15 |
| S0.5 | buffer pequeno devolve tamanho necessário | ok (`SF_ERR_BUFFER`) | obrigatório | dev/ref | 2026-11-15 |
| S0.5 | ABI C exercitada em runtime (ciclo, erros, eventos) | ok (`test_c_api`) | obrigatório | dev/ref | 2026-11-15 |
| S0.6 | header guarda-chuva compila sozinho | ok (`test_umbrella`) | obrigatório | dev/ref | 2026-11-15 |

## Medições de performance

Reprodutível com `simforge_bench` (alvo `bench/`, fora do ctest): melhor de três
execuções, cada uma já com melhor de 3–5 repetições internas.

| spike | métrica | valor | limite | status | nota |
|---|---|---|---|---|---|
| S0.1 | drenar 1 M eventos da fila | 113.08 ms (≈113 ns/evento) | ≤ 150 ms | ok | 1 M eventos simultâneos no heap; profundidade real é muito menor |
| S0.1 | enfileirar 1 M eventos via API pública | 269.15 ms | — | informativo | inclui busca de pino por nome + `solve()` por chamada |
| S0.1 | custo por evento no caminho público | 0.27 µs | — | informativo | teto do caso "100 k pendentes" |
| S0.1 | inserção na fila com 100 k pendentes | 0.01 µs | ≤ 2 µs | ok | `push` puro (`bench_enqueue`), sem solve |
| S0.1 | remoção da fila com 100 k pendentes | 0.05 µs | ≤ 2 µs | ok | `pop` puro (`bench_dequeue`), sem handlers |
| S0.2 | net com 200 drivers alternando | 2.08 µs/solve | ≤ 5 µs | ok | 1 net, 200 pinos, 1 driver ativo por vez |
| S0.2 | cenário `button-led` (10 s virtuais) | 3.28 ms | ≤ 20 ms | ok | passo de 1 ms; botão 20 ms a cada 200 ms |
| S0.2 | PWM 1 kHz em 8 canais (1 s virtual) | 1.50 ms | ≤ 30 ms | ok | 16 k bordas agendadas; 3% de 1 s a 60 Hz |
| S0.4 | projeto com 10 k objetos (abrir + validar) | 167.04 ms | ≤ 250 ms | ok | 10 001 objetos, 1.69 MB, 0 diagnósticos |
| S0.5 | requisição de 1 KB (sem I/O) | 4.80 µs | ≤ 20 µs | ok | `app.ping`, 1 079 bytes de requisição |
| S0.5 | 10 k mutações via CommandBus | 69.93 ms | — | informativo | emitir 10 k eventos + revisões |
| S0.5 | 10 k eventos por `events.poll` | 25.43 ms | ≤ 50 ms | ok | buffer cheio (limite de 10 k eventos) |
| S0.6 | frame completo com 1 k dispositivos | 0.81 ms | ≤ 4 ms | ok | payload JSON de 54 118 bytes |

## Pendentes

| spike | métrica | motivo |
|---|---|---|
| S0.6 | payload binário para 1 k dispositivos (≤ 32 KB) | transporte binário não implementado; o JSON (54 KB) serve de teto |

## Notas de medição

- O alvo de S0.1 é a **fila**: enfileirar e drenar são medidos separadamente
  porque a injeção pública (`set_pin`) resolve o circuito a cada chamada.
- O caso isolado usa `Engine::bench_enqueue`/`bench_dequeue`: eles enfileiram
  eventos neutros e drenam sem processar handlers, então medem só a
  `std::priority_queue`. Os hooks não alteram tempo, hash nem contadores; há
  teste (`engine_bench_queue_hooks_are_isolated`) garantindo o isolamento.
- O PWM vem do dispositivo `simforge.clock` (`freq_hz`, `duty`): cada borda é um
  evento próprio (`kind` carrega o índice do dispositivo) e o motor reagenda a
  próxima ao drenar. O host não precisa chamar `sim.step` para cada transição.
- Duas otimizações saíram destas medições:
  - o solver reutiliza um buffer por net em vez de alocar a cada ciclo
    (≈40% menos tempo no caminho de injeção);
  - o buffer de eventos da `Session` virou `std::deque`, porque o descarte do
    evento mais antigo era O(n) num `std::vector`.
- A drenagem de 1 M eventos simultâneos é limitada por cache (heap de 1 M
  elementos). Para profundidades realistas (centenas), o custo por evento é
  muito menor. Se o perfil real exigir, o próximo passo é uma fila em buckets
  por fatia de tempo.

## Próximas medições

1. Transporte binário para S0.6.
2. Repetir a campanha em Linux/GCC e macOS/Clang assim que houver CI com essas
   plataformas.

Registrar cada linha com máquina e data. Sem isso, o spike continua aberto.

## Frontend (protótipo visual)

Fora dos critérios de go/no-go do core, mas medido para calibrar o custo da
camada de apresentação:

| item | resultado |
|---|---|
| `npm run build` (tsc + vite) | 260 ms |
| bundle JS | 282.10 kB (85.43 kB gzip) |
| bundle CSS | 28.88 kB (5.95 kB gzip) |
| `npm run lint` (oxlint) | 0 erros (5 avisos de fast-refresh/effect) |

Interações verificadas no navegador contra o `MockTransport` da `api-v1`:
executar/pausar/reiniciar/parar, passo de 1 ms e velocidade; LED piscando em
`sim.frame`; arrasto de componente com snap de 12 unidades; inserção e remoção
via `circuit.*` com revisão e log; abas Console, Serial, Código, Instrumentos e
Bancada.

### Ponte com o núcleo

`web/server/simforge-cli.ts` sobe `simforge-cli api` (NDJSON, uma linha por
requisição) e expõe `GET /sf/status` e `POST /sf/command` no dev server. Em
navegador, contra o executável `release`:

| verificação | resultado |
|---|---|
| `project.open` do exemplo com caminho acentuado | ok (após `include/simforge/paths.hpp`) |
| `sim.load` + `sim.frame` | `N1:z`, `N2:z`, `N3:high/weak` |
| `sim.debug.setPin GPIO2=high` + `sim.step 1 ms` | `N1:high/strong`, LED `on:true` |
| `sim.debug.releasePin GPIO2` | `N1:z`, LED `off` |
| cadência de `sim.run` (ponte) | 3.37 s virtuais em ~3.3 s reais |

O caminho acentuado (“Área de Trabalho”) quebrava `project.open`: o `std::string`
UTF-8 da API era interpretado no code page ANSI ao virar `std::filesystem::path`.
A correção centralizou a conversão em `simforge/paths.hpp` e ganhou o teste
`session_project_opens_from_utf8_path`.

Pendente: transporte binário/WebSocket no lugar de NDJSON e testes automatizados
de UI.
