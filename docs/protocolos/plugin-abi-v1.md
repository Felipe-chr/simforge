# SimForge Plugin ABI v1

**Status:** proposta congelável
**ABI:** `0x00010000` (major 1, minor 0)
**Manifesto:** [`plugin-v1.schema.json`](plugin-v1.schema.json)
**Linguagem:** C (compatível com C++ via `extern "C"`)

---

## 1. Objetivos

- Permitir que componentes, placas, compiladores, runtimes, exporters e mundos
  externos sejam instalados **sem recompilar o núcleo**.
- Garantir estabilidade binária entre versões minor.
- Isolar plugins não confiáveis.
- Detectar incompatibilidade antes de carregar código.

---

## 2. Regras de fronteira

1. Nenhum tipo STL, exceção, `std::string`, `std::vector` ou classe C++ cruza a
   ABI.
2. Structs são versionadas por `struct_size`; campos novos entram no fim.
3. Strings são UTF-8, com ownership explícito.
4. Toda função retorna `int` (0 = sucesso) ou preenche `sf_error`.
5. O plugin usa o allocator do host para memória que o host libera.
6. Handles são opacos (`void*` ou structs incompletas).
7. Callback nunca é chamado após `sf_plugin_shutdown`.
8. Nenhuma thread é criada pelo plugin sem declarar `thread_model=any_thread`.

---

## 3. Símbolos exportados

```c
#define SF_PLUGIN_ABI_V1 0x00010000u

#if defined(_WIN32)
#  define SF_EXPORT __declspec(dllexport)
#  define SF_CALL __cdecl
#else
#  define SF_EXPORT __attribute__((visibility("default")))
#  define SF_CALL
#endif

typedef struct sf_error {
    uint32_t struct_size;
    uint32_t code;
    const char* message;   /* UTF-8; válido até a próxima chamada */
} sf_error;

typedef struct sf_plugin_info {
    uint32_t struct_size;
    uint32_t abi_version;
    const char* plugin_id;
    const char* name;
    const char* version;       /* semver */
    const char* vendor;
    uint64_t capabilities;
} sf_plugin_info;

typedef struct sf_plugin_host {
    uint32_t struct_size;
    void* host_ctx;
    void (SF_CALL* log)(void*, int level, const char* msg);
    void* (SF_CALL* alloc)(void*, size_t);
    void  (SF_CALL* free)(void*, void*);
    uint64_t (SF_CALL* sim_time_ns)(void*);
    int   (SF_CALL* emit)(void*, const char* event_json);
} sf_plugin_host;
```

Exportações obrigatórias:

```c
SF_EXPORT const sf_plugin_info* SF_CALL sf_plugin_query(void);
SF_EXPORT int SF_CALL sf_plugin_init(const sf_plugin_host*, sf_error*);
SF_EXPORT int SF_CALL sf_plugin_register(sf_registrar*, const sf_registration*, size_t, sf_error*);
SF_EXPORT void SF_CALL sf_plugin_shutdown(void);
```

`sf_plugin_query` deve retornar um ponteiro estático válido por toda a vida da
biblioteca. `sf_plugin_init` recebe o host e pode alocar estado global.
`sf_plugin_register` registra fábricas. `sf_plugin_shutdown` libera tudo.

---

## 4. Registro

```c
typedef struct sf_device_vtable {
    uint32_t struct_size;
    void* (SF_CALL* create)(const char* params_json, sf_error*);
    void  (SF_CALL* destroy)(void*);
    int   (SF_CALL* configure)(void*, const char* params_json, sf_error*);
    int   (SF_CALL* attach)(void*, void* host_ctx, sf_error*);
    int   (SF_CALL* on_event)(void*, const void* event, sf_error*);
    int   (SF_CALL* write_state)(void*, void* writer, sf_error*);
    int   (SF_CALL* read_state)(void*, void* reader, sf_error*);
} sf_device_vtable;

typedef struct sf_device_registration {
    uint32_t struct_size;
    const char* type_id;
    uint32_t type_version;
    const char* descriptor_json;
    const sf_device_vtable* vtable;
} sf_device_registration;

typedef struct sf_registration {
    uint32_t struct_size;
    const sf_device_registration* devices;
    size_t device_count;
    /* extensões futuras: boards, compilers, exporters, worlds */
} sf_registration;

typedef struct sf_registrar sf_registrar;
```

- `descriptor_json` segue o schema publicado em `device.describe`.
- O host valida o descritor antes de registrar.
- `type_id` é namespace reverso e único.
- `attach` recebe `host_ctx` opaco; o plugin só o usa com funções do host.

---

## 5. Eventos e contexto

`on_event` recebe um blob opaco no formato:

```
u16 kind
u16 flags
u64 t_ns
u32 payload_len
u8  payload[payload_len]   /* CBOR */
```

Kinds: `pin_change`, `net_resolved`, `bus_event`, `timer`, `tick`, `custom`.
O plugin deve validar `payload_len` e o kind; kinds desconhecidos são ignorados.

Leituras e escritas de pino usam funções do host (não há acesso direto ao
netlist):

```c
int sf_host_read_pin(void* host_ctx, const char* pin, sf_pin_state* out);
int sf_host_drive_pin(void* host_ctx, const char* pin, sf_pin_state in);
int sf_host_schedule(void* host_ctx, uint64_t t_ns, uint32_t kind,
                     const void* payload, size_t len);
```

---

## 6. Versionamento

- `abi_version` codifica `major << 16 | minor`.
- Host exige `major` igual; `minor` maior é aceito se as capabilities exigidas
  estiverem presentes.
- `min_host` no manifesto impede carregar em host antigo.
- `struct_size` permite evolução de structs sem quebrar ABI.
- Remoção/renomeação de campo ou mudança de semântica exige major novo.
- Plugins declaram `capabilities` (bitmask) e o host publica as suas em
  `app.info`.

---

## 7. Manifesto

```json
{
  "format": "simforge.plugin",
  "schema": 1,
  "id": "acme.sensor.temp",
  "name": "Sensores ACME",
  "version": "1.4.2",
  "abi": "1.0",
  "kind": ["device"],
  "entry": {
    "windows": "acme_sensors.dll",
    "linux": "libacme_sensors.so",
    "macos": "libacme_sensors.dylib",
    "wasm": "acme_sensors.wasm"
  },
  "capabilities": ["device.temperature", "device.humidity"],
  "permissions": ["fs.read:assets"],
  "deterministic": true,
  "thread_model": "sim_thread",
  "min_host": "0.4.0",
  "license": "MIT",
  "signature": "ed25519:Ur2bCo0qHQnGrrfLDqoiGjHHPWQcWWuxxZ4h3bET+E4JqgliftSTQArv8cG4lTLFjBxL2KJYjJ4BbJDojH/7Gw=="
}
```

- `id` é único e imutável.
- `entry` deve ter ao menos uma plataforma.
- `permissions` são aplicadas pelo sandbox.
- `deterministic=true` obriga o plugin a não usar relógio de parede, threads ou
  RNG fora do fornecido pelo host.
- `signature` no exemplo é ilustrativa; em produção é Ed25519 sobre o hash do
  pacote.

Sintaxe de permissão: `<categoria>.<verbo>[:<argumento>]`, por exemplo
`fs.read:assets`, `net.connect:api.acme.com`, `process.spawn:gcc`. Categorias
reservadas: `fs`, `net`, `process`, `gpu`, `audio`, `display`, `clipboard`.
Permissão sem argumento vale para toda a categoria; com argumento, é restrita
ao escopo indicado (glob permitido em `fs.*`).

---

## 8. Ciclo de vida e hot reload

```
discover → validate → load → query → init → register
   → create/attach/on_event/... → destroy
   → shutdown → unload
```

Hot reload (modo dev):

1. Bloquear novas instâncias.
2. Serializar estado (`write_state`).
3. Destruir instâncias.
4. Copiar DLL para shadow path (Windows).
5. `shutdown` + unload.
6. Carregar nova versão, validar ABI.
7. Recriar instâncias e `read_state`.
8. Retomar.

Se o estado não for compatível, a sessão é reiniciada e o usuário é avisado.
Plugins `process` (runtime/compiler/world) fazem reload matando o processo.

---

## 9. Isolamento e confiança

| Tier | Execução | Permissões |
|---|---|---|
| `builtin` | in-process | todas |
| `verified` | in-process | declaradas + auditoria |
| `community` | in-process com guarda | declaradas |
| `untrusted` | processo separado | mínimas |

- Assinatura Ed25519 sobre o hash do pacote.
- Plugin que crasha 2× é quarentenado.
- Processo separado usa job object (Windows) / seccomp (Linux), sem rede por
  padrão.
- O host nunca carrega plugin de diretório gravável sem assinatura, salvo
  `--dev`.

---

## 10. Testes de ABI

- `plugin_abi_test`: compila um plugin de exemplo contra o SDK e valida todos os
  símbolos, structs e versionamento.
- Teste de compatibilidade: plugin v1.0 carrega em host v1.x.
- Teste de rejeição: major errado, assinatura inválida, descritor inválido.
- Fuzzing de `descriptor_json`, eventos e estado serializado.
- Matriz de compiladores: MSVC, Clang, GCC (mesma ABI C).
