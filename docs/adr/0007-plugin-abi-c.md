# ADR 0007 — ABI C versionada para plugins

- **Status:** proposto
- **Data:** 2026-11-14
- **Relacionado:** ADR 0001, ADR 0012

## Contexto

Componentes de terceiros precisam ser instalados sem recompilar o núcleo. Uma
ABI C++ não é estável entre compiladores, versões de STL e flags.

## Decisão

- Plugins expõem uma ABI C (`extern "C"`) com structs versionadas por
  `struct_size`, handles opacos e erros por código.
- O manifesto declara `abi`, `capabilities`, `min_host` e permissões.
- O host converte descritores JSON em tipos internos e valida tudo.
- Nenhuma exceção, STL ou classe C++ cruza a fronteira.
- Compatibilidade por major; minor é aditivo.

## Consequências

- Plugins podem ser escritos em C, C++ ou Rust (com FFI).
- Há uma camada de conversão e testes de ABI.
- Hot reload exige serialização de estado.
- O SDK precisa manter headers e exemplos sincronizados.

## Alternativas

- ABI C++: simples, mas frágil entre compiladores.
- COM: complexo e específico de Windows.
- Plugins apenas embutidos: não atende ao requisito de instalação dinâmica.
