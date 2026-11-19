# `.sforge` — Formato de Projeto v1

**Status:** congelável
**Media type:** `application/vnd.simforge.project+json`
**Extensão:** `.sforge`
**Schema:** [`sforge-v1.schema.json`](sforge-v1.schema.json)
**Codificação:** UTF-8, sem BOM
**Unidades:** elétrica em SI; esquemático em unidades de grade inteiras

---

## 1. Identidade do documento

```json
{
  "format": "simforge.project",
  "schema": 1
}
```

- `format` é obrigatório e constante.
- `schema` é inteiro; ausência é tratada como `1` apenas para documentos
  legados. Versões futuras são recusadas com `project.schema_unsupported`.
- O documento é autossuficiente em topologia e parâmetros; binários de firmware
  são referenciados por hash e vivem no cache.

---

## 2. Estrutura

| Campo | Tipo | Obrigatório | Descrição |
|---|---|---|---|
| `format` | string | sim | `"simforge.project"` |
| `schema` | integer | sim | `1` |
| `project` | object | sim | metadados e revisão |
| `units` | object | não | declaração de unidades |
| `boards` | array | sim | placas instanciadas |
| `devices` | array | sim | componentes instanciados |
| `nets` | array | sim | topologia elétrica |
| `buses` | array | não | protocolos sobre nets |
| `wires` | array | não | geometria dos fios |
| `firmware` | object | não | código, bibliotecas e artefato |
| `simulation` | object | não | parâmetros de execução |
| `layout` | object | não | estado de visualização |
| `extensions` | object | não | dados de plugins/versões futuras |

Campos desconhecidos são preservados no round-trip. O leitor aplica padrões
documentados quando um campo opcional está ausente.

---

## 3. Identidade e referências

- Todo objeto persistente tem `id` ULID (26 caracteres Crockford base32).
- Referências usam `id`, nunca índice de array.
- `device_id`, `board_id`, `net_id`, `bus_id`, `wire_id` são escopados ao
  documento.
- Renomear não altera `id`.
- Duplicar um objeto gera novos IDs e remapeia referências internas.

Regex ULID: `^[0-9A-HJKMNP-TV-Z]{26}$`.

---

## 4. Unidades e coordenadas

```json
{
  "units": {
    "length": "mm",
    "grid_mm": 1.27,
    "voltage": "V",
    "current": "A",
    "resistance": "ohm",
    "time": "ns"
  }
}
```

- `position`, `points` e dimensões de símbolo usam **unidades de grade**
  inteiras (`grid_mm`, padrão 1,27 mm).
- Rotação: `0`, `90`, `180`, `270` (graus, anti-horário).
- Parâmetros elétricos usam SI (`vf` em V, `r_series` em Ω, `clock_hz` em Hz).
- Tempo de simulação em ns inteiros.
- O renderer converte grade → pixels; o core nunca depende de pixels.

---

## 5. Placa

```json
{
  "id": "01J9Z6M0A1B2C3D4E5F6G7H8J9",
  "type": "simforge.esp32s3.devkitc",
  "type_version": 1,
  "position": { "x": 120, "y": 80 },
  "rotation": 0,
  "label": "Controlador",
  "params": { "psram": true },
  "pins": {
    "GPIO2": { "net": "N1" },
    "GND": { "net": "GND" }
  }
}
```

- `type` referencia um descritor registrado (`board.catalog`).
- `type_version` fixa a versão do descritor e é **obrigatório** no schema 1, para
  que o projeto continue reproduzível após atualizações de plugin.
- `params` é validado contra o schema do descritor.
- `pins` mapeia pinos da placa para nets (atalho; a verdade completa está em
  `nets[].pins`). As duas visões devem ser consistentes; o validador rejeita
  divergência.

---

## 6. Dispositivo

```json
{
  "id": "01J9Z6N7B2C3D4E5F6G7H8J9K0",
  "type": "simforge.led",
  "type_version": 1,
  "position": { "x": 200, "y": 60 },
  "rotation": 90,
  "label": "LED1",
  "params": { "color": "red", "vf": 1.9, "r_series": 330 },
  "pins": {
    "A": { "net": "N1" },
    "K": { "net": "GND" }
  },
  "hidden": false,
  "locked": false
}
```

- `pins` pode omitir pinos não conectados.
- Um pino pertence a no máximo uma net.
- `params` é opaco para o formato; o descritor do dispositivo define schema,
  defaults e validação.
- `hidden`/`locked` são dicas de UI, não afetam a simulação.

---

## 7. Net

```json
{
  "id": "N1",
  "name": "LED_A",
  "kind": "digital",
  "pins": [
    { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "pin": "GPIO2" },
    { "device": "01J9Z6N7B2C3D4E5F6G7H8J9K0", "pin": "A" }
  ]
}
```

- `kind`: `digital`, `analog`, `power`, `ground`, `bus`.
- `pins` é a fonte de verdade elétrica.
- Uma net com menos de dois pinos é permitida durante a edição, mas gera
  warning `W_DANGLING_NET`.
- `power`/`ground` têm semântica especial (rails, referência).
- Nets não podem ter o mesmo `id` nem o mesmo nome dentro do escopo (nomes
  duplicados geram warning).

---

## 8. Barramento

I2C:

```json
{
  "id": "I2C0",
  "kind": "i2c",
  "controller": { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "peripheral": "I2C0" },
  "sda": "N10",
  "scl": "N11",
  "clock_hz": 400000,
  "targets": [
    { "device": "01J9Z6R9D4E5F6G7H8J9K0M1N2", "address": 60 }
  ]
}
```

Os blocos desta seção são recortes de um documento maior: `N10`, `N11`, `N20`...
e os dispositivos citados existem no projeto, mas não são repetidos aqui.

- `sda`/`scl` referenciam nets existentes.
- `address` é decimal (0–127); a UI pode exibir hexadecimal.
- `targets` lista dispositivos que respondem; um target sem modelo gera
  `E_UNSUPPORTED_TARGET` na simulação.

SPI:

```json
{
  "id": "SPI0",
  "kind": "spi",
  "controller": { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "peripheral": "SPI2" },
  "sclk": "N20", "mosi": "N21", "miso": "N22",
  "clock_hz": 1000000,
  "mode": 0,
  "targets": [{ "device": "01J9Z6S1E5F6G7H8J9K0M1N2P3", "cs": "N23" }]
}
```

UART:

```json
{
  "id": "UART0",
  "kind": "uart",
  "controller": { "board": "01J9Z6M0A1B2C3D4E5F6G7H8J9", "peripheral": "UART0" },
  "tx": "N30", "rx": "N31",
  "baud": 115200,
  "data_bits": 8, "parity": "none", "stop_bits": 1
}
```

O schema valida campos obrigatórios por `kind`; campos adicionais são
preservados.

---

## 9. Fio (geometria)

```json
{
  "id": "W1",
  "net": "N1",
  "points": [{ "x": 150, "y": 60 }, { "x": 200, "y": 60 }],
  "color": "#e11d48",
  "width": 1
}
```

- `wires` não define conectividade; apenas desenha a net.
- Vários fios podem pertencer à mesma net.
- Pontos são unidades de grade; o renderer aplica zoom/pan.
- Um fio sem net é inválido.

---

## 10. Firmware

```json
{
  "entry": "src/main.cpp",
  "backend": "platformio",
  "framework": "arduino",
  "libraries": [
    { "name": "Adafruit SSD1306", "version": "2.5.9" }
  ],
  "defines": { "DEBUG": "1" },
  "artifact": "sha256:9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d",
  "build_id": "01J9Z7Q4W8E2R5T1Y3U6I9O0P2"
}
```

- `entry` é relativo à raiz do projeto; caminhos absolutos e `..` são
  rejeitados.
- `backend`: `platformio`, `arduino-cli`, `esp-idf` ou `shim`.
- `artifact` referencia o cache de builds; se ausente, o projeto abre sem
  firmware carregado.
- `libraries` é um lockfile lógico; versões exatas obrigatórias.

---

## 11. Simulação

```json
{
  "solver": "auto",
  "seed": 12345,
  "slice_us": 1000,
  "analog_step_us": 10,
  "real_time": false,
  "max_run_ns": 600000000000
}
```

| Campo | Padrão | Regra |
|---|---|---|
| `solver` | `auto` | `logic`, `mna`, `auto` |
| `seed` | `0` | inteiro 64 bits |
| `slice_us` | `1000` | 100–10000 |
| `analog_step_us` | `10` | 1–1000 |
| `real_time` | `false` | throttle 1× |
| `max_run_ns` | ausente | limite de segurança |

O seed entra no hash do trace. Alterá-lo invalida replays anteriores.

---

## 12. Layout e extensões

```json
{
  "layout": {
    "zoom": 1.0,
    "pan": { "x": 0, "y": 0 },
    "grid": true,
    "theme": "dark"
  },
  "extensions": {
    "acme.plugin": { "custom": true }
  }
}
```

- `layout` é descartável; não afeta simulação.
- `extensions` preserva dados de plugins; o core não interpreta.
- Chaves de extensão usam namespace reverso.

---

## 13. Validação

Ordem:

1. **Sintática:** JSON, schema, limites de tamanho/profundidade.
2. **Referencial:** IDs únicos, referências existentes, pinos válidos.
3. **Consistência:** `pins` da placa/dispositivo × `nets[].pins`.
4. **Elétrica (ERC):** curto, flutuante, tensão, corrente, GND comum.
5. **Simulação:** dispositivo/placa suportados, runtime, firmware.
6. **Segurança:** paths, assets, conteúdo executável.

Diagnósticos:

```json
{
  "severity": "error",
  "code": "E_SHORT",
  "message": "GPIO2 e GND conectados na mesma net.",
  "path": "/nets/7",
  "net": "N7",
  "pins": [{ "board": "01KA3EHR01V3RS3DTR98H6BCFN", "pin": "GPIO2" }],
  "fix": "separate_net"
}
```

Códigos estáveis: `E_*` bloqueiam simulação; `W_*` avisam.

O exemplo distribuído em
[`examples/blink-led.sforge`](../../examples/blink-led.sforge) é validado contra
este schema por `tools/validate_contracts.py` e reaberto pelo núcleo no teste
`project_example_blink_led_is_valid`.

---

## 14. Migração

```
schema 1 ──► schema 2 ──► schema 3
```

- Cadeia de funções puras `migrate_N_to_N+1`.
- Cada migração tem golden files de entrada/saída.
- Campos desconhecidos são preservados; se uma migração não puder preservá-los,
  o documento é marcado `lossy` e o usuário confirma.
- `schema` futuro é recusado, nunca "adivinhado".
- O documento migrado só é salvo após validação completa.

---

## 15. Compatibilidade e limites

- Adicionar campo opcional não incrementa schema.
- Remover/renomear campo ou mudar semântica incrementa schema.
- Limites padrão: 64 MB, profundidade 64, 100k objetos, 1M pontos de fio.
- Assets referenciados por hash; paths relativos, normalizados, sem `..`.
- O arquivo nunca contém código executável; firmware é hash + cache.
- Escrita atômica: `arquivo.tmp` + `rename`.

---

## 16. Bundle

`.sforgebundle` é um ZIP com:

```
project.sforge
assets/<hash>
firmware/ (opcional, assinado)
manifest.json
```

`manifest.json` lista hashes e versões. O bundle é para troca/submissão; o
formato de trabalho continua sendo `.sforge` + assets.
