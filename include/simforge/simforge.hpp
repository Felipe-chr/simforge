#pragma once
// Header guarda-chuva do núcleo SimForge (C++20).
//
// Inclui a API C++ completa de uma vez. Para uso em C ou em FFI, prefira
// include/simforge/c_api/simforge.h.
//
// Módulos, em ordem de dependência:
//   json     DOM JSON com ordem preservada (ADR 0019)
//   time     Tempo virtual em nanossegundos e fases do motor
//   ids      ULID
//   circuit  Netlist: pinos, nets, conexões
//   device   Dispositivos e registro de tipos
//   board    Placas (ESP32-S3 DevKitC)
//   engine   Motor de simulação (fila de eventos, solver lógico)
//   project  Documento .sforge e validação
//   paths    Caminhos UTF-8 da API ↔ std::filesystem::path
//   session  CommandBus api-v1 (/simforge/docs/protocolos/api-v1.md)

#include "simforge/json.hpp"
#include "simforge/time.hpp"
#include "simforge/ids.hpp"
#include "simforge/circuit.hpp"
#include "simforge/device.hpp"
#include "simforge/board.hpp"
#include "simforge/engine.hpp"
#include "simforge/project.hpp"
#include "simforge/paths.hpp"
#include "simforge/session.hpp"
