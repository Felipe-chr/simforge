#pragma once
// Sessão de comandos: ponto único de entrada da API v1.
//
// Toda mutação (projeto, circuito, simulação) passa por aqui. C ABI, CLI e
// (futuramente) WebSocket são apenas transportes sobre o mesmo CommandBus.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "simforge/json.hpp"

namespace simforge {

/// Lote de eventos devolvido por `events.poll` / push do C ABI.
struct EventBatch {
    std::vector<json::Value> events;
    std::uint64_t next_seq = 1;
    std::uint64_t dropped = 0;
};

class Session {
public:
    Session();
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    /// Executa uma requisição no envelope api-v1 e devolve a resposta JSON.
    std::string execute(std::string_view request_json);

    /// Variante já parseada (usada pelo C ABI para evitar reparsing).
    json::Value execute_request(const json::Value& request);

    /// Eventos com `seq` > `after_seq`, no máximo `max`.
    EventBatch poll_events(std::uint64_t after_seq, std::size_t max);

    /// Último código de erro (diagnóstico de transporte).
    std::string last_error() const;

    static const char* product_version();
    static const char* engine_version();
    static int api_version();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace simforge
