#pragma once
// Motor de simulação: fila determinística, solver e avaliação de dispositivos.

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "simforge/circuit.hpp"
#include "simforge/device.hpp"
#include "simforge/json.hpp"
#include "simforge/time.hpp"

namespace simforge {

enum class EngineState { Idle, Ready, Running, Paused, Stopped, Error };
const char* to_string(EngineState state);

struct EngineOptions {
    std::uint32_t delta_cycle_limit = 64;
    bool strict_determinism = true;
};

struct EngineError {
    std::string code;
    std::string message;
    json::Value details = json::Value::object();
};

class Engine {
public:
    explicit Engine(EngineOptions options = {});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Reconstrói o runtime a partir do documento `.sforge`.
    bool load(const json::Value& document, std::string* error);
    void reset();

    void start();
    void pause();
    void resume();
    void stop();

    void run_until(SimTime target);
    void run_for(SimTime delta);
    /// Drena até `max_events` eventos; devolve quantos foram processados.
    std::size_t step_events(std::size_t max_events);

    SimTime now() const;
    EngineState state() const;
    std::uint64_t trace_hash() const;
    std::size_t event_count() const;
    const std::vector<EngineError>& errors() const;
    void clear_errors();

    /// Estímulo de bancada/runtime (dev). Requer uma placa com o pino.
    bool set_pin(std::string_view owner_id, std::string_view pin, Level level,
                 std::string* error);
    bool release_pin(std::string_view owner_id, std::string_view pin,
                     std::string* error);
    bool set_device_param(std::string_view device_id, std::string_view key,
                          const json::Value& value, std::string* error);

    // --- hooks exclusivos do benchmark (S0.1) -----------------------------
    // Enfileiram/drenam eventos neutros sem resolver o circuito, para medir
    // somente o custo da `priority_queue`. Não alteram tempo, hash nem
    // contadores de eventos processados; não use em produção.

    /// Enfileira `count` eventos neutros a partir de `now`.
    void bench_enqueue(std::size_t count);
    /// Remove até `count` eventos da fila; devolve quantos saíram.
    std::size_t bench_dequeue(std::size_t count);
    /// Eventos pendentes na fila.
    std::size_t bench_queue_size() const;

    /// Snapshot compacto para renderização (`sim.frame`).
    json::Value frame(std::uint64_t seq) const;
    json::Value nets_state() const;

    /// Detalhes de implementa??o (p?blico apenas para o contexto do solver).
    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace simforge
