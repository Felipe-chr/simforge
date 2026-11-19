# ADR 0001 — Núcleo em C++20 com API C estável

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0014, ADR 0018

## Contexto

O SimForge precisa rodar em desktop, navegador, CI, nuvem e dentro de outros
produtos. Um núcleo preso a uma UI ou a um runtime específico inviabiliza esses
hosts e duplica regras.

## Decisão

- Todo o domínio de simulação vive em `libsimforge`, em C++20.
- O núcleo não depende de UI, sistema operacional, rede ou banco.
- A fronteira pública para outros produtos é uma API C versionada; a API C++
  interna é estável por major, mas não é ABI.
- Hosts específicos (desktop, WASM, CLI) são adaptadores.

## Consequências

- Testes de domínio rodam sem janela e sem servidor.
- A mesma lógica atende desktop, navegador e PistaForge.
- Há um custo de conversão entre a API C e os tipos internos.
- Requer disciplina de headers públicos e CI de ABI.

## Alternativas

- API C++ apenas: ABI instável entre compiladores.
- Núcleo em TypeScript/Rust: não atende ao requisito de C++20 e ao reuso
  embarcado.
