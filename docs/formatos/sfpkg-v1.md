# `.sfpkg` — Pacote de Componentes v1

**Status:** proposta congelável
**Extensão:** `.sfpkg`
**Formato:** ZIP (deflate), UTF-8, sem symlinks
**Assinatura:** Ed25519 sobre o `manifest.json` canônico
**Schema:** [`sfpkg-v1.schema.json`](sfpkg-v1.schema.json)

---

## 1. Objetivo

Distribuir componentes, placas, compiladores, runtimes e temas de forma
versionada, verificável e instalável sem recompilar o núcleo.

Um `.sfpkg` pode conter um ou mais plugins e seus assets (símbolos, fontes,
modelos, documentação).

---

## 2. Layout

```
acme-sensors-1.4.2.sfpkg
├─ manifest.json
├─ signature.ed25519
├─ plugins/
│  └─ acme_sensors/
│     ├─ plugin.json          (manifesto de plugin v1)
│     ├─ bin/acme_sensors.dll
│     ├─ bin/libacme_sensors.so
│     └─ bin/acme_sensors.wasm
├─ symbols/
│  ├─ temp.svg
│  └─ humidity.svg
├─ models/
│  └─ dht22.json
├─ docs/
│  └─ README.md
└─ LICENSE
```

- `manifest.json` é obrigatório e fica na raiz.
- `signature.ed25519` assina os bytes canônicos do manifesto.
- Caminhos são relativos, normalizados, sem `..` e sem absolutos.
- Nenhum arquivo executável fora de `plugins/*/bin`.
- ZIP sem symlinks, sem entries duplicadas, sem nomes reservados do Windows.

---

## 3. Manifesto

```json
{
  "format": "simforge.package",
  "schema": 1,
  "id": "acme.sensors",
  "name": "Sensores ACME",
  "version": "1.4.2",
  "kind": ["device"],
  "publisher": {
    "id": "acme",
    "name": "ACME Ltda.",
    "homepage": "https://acme.example"
  },
  "min_host": "0.4.0",
  "license": "MIT",
  "description": "Sensores de temperatura e umidade.",
  "plugins": [
    {
      "path": "plugins/acme_sensors/plugin.json",
      "id": "acme.sensor.temp",
      "version": "1.4.2"
    }
  ],
  "dependencies": [
    { "id": "simforge.core", "version": ">=0.4.0 <1.0.0" }
  ],
  "files": [
    { "path": "plugins/acme_sensors/bin/acme_sensors.dll",
      "sha256": "9f2c7d1e4b8a6f3c2d5e0a9b8c7d6e5f4a3b2c1d0e9f8a7b6c5d4e3f2a1b0c9d", "size": 245760 },
    { "path": "symbols/temp.svg", "sha256": "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d5e6f708192a3b4c5d6e7f809", "size": 2048 }
  ]
}
```

| Campo | Regra |
|---|---|
| `id` | namespace reverso, único no marketplace |
| `version` | semver |
| `kind` | `device`, `board`, `compiler`, `runtime`, `solver`, `exporter`, `world`, `theme`, `bundle` |
| `files` | lista completa com hash e tamanho |
| `dependencies` | semver ranges; resolvidas antes da instalação |
| `min_host` | versão mínima do SimForge |

A assinatura **não** fica dentro do manifesto (evita autorreferência). Ela é
gravada em `signature.ed25519` e cobre os bytes canônicos de `manifest.json`.
O arquivo de assinatura contém `alg`, `key_id` e `value`.

---

## 4. Instalação

```
1. abrir ZIP e validar limites (tamanho, entradas, profundidade)
2. ler manifest.json; validar schema
3. verificar assinatura e cadeia de confiança
4. verificar hashes/tamanhos de todos os arquivos
5. resolver dependências (sem ciclos)
6. checar ABI dos plugins e min_host
7. extrair para diretório temporário
8. mover atomicamente para packages/<id>/<version>/
9. registrar no catálogo e habilitar conforme trust tier
```

- Falha em qualquer etapa não altera a instalação existente.
- Versões antigas permanecem disponíveis até não haver projeto usando.
- `plugin.install` só instala pacotes assinados, salvo `--dev`.

---

## 5. Trust tiers e assinatura

| Tier | Requisito | Execução |
|---|---|---|
| `builtin` | parte do núcleo | in-process |
| `verified` | assinatura + curadoria | in-process |
| `community` | assinatura do autor | in-process com guarda |
| `untrusted` | sem assinatura | processo separado ou bloqueado |

- Chaves públicas ficam em `trusted_keys.json` (built-in + atualizáveis).
- Revogação por `key_id` e por hash de pacote.
- O marketplace publica índice assinado; o cliente verifica antes de listar.
- `publisher.id` é vinculado à chave; troca de chave exige rotação assinada.

---

## 6. Segurança

- **Zip slip:** rejeitar caminhos que escapem da raiz.
- **Zip bomb:** limite de razão de compressão (100:1) e de tamanho extraído
  (256 MB por pacote).
- **Symlinks:** proibidos.
- **Executáveis:** somente em `plugins/*/bin`, com hash verificado.
- **Scripts de instalação:** proibidos; toda instalação é declarativa.
- **Dependências:** resolução offline por lockfile; sem downloads implícitos.
- **Sandbox:** o tier determina o isolamento em runtime.

---

## 7. Compatibilidade e atualização

- `schema` inteiro; versão futura recusada.
- Atualização é uma nova versão imutável; nunca se sobrescreve `1.4.2`.
- `simforge.package` pode agregar vários plugins; cada um mantém seu próprio
  `plugin.json` e ABI.
- Desinstalação remove arquivos e catálogo, preservando builds em cache que
  referenciam o pacote (marcados como órfãos).
- O lockfile do projeto fixa versões exatas; o resolvedor nunca troca sozinho.

---

## 8. Publicação

```
sfpkg pack ./acme-sensors --out acme-sensors-1.4.2.sfpkg --key acme-2026
sfpkg verify acme-sensors-1.4.2.sfpkg
sfpkg inspect acme-sensors-1.4.2.sfpkg --json
```

O publisher assina o manifesto; o marketplace assina o índice. O SimForge
verifica ambos antes de instalar.
