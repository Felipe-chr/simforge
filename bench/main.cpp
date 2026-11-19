// Benchmarks do núcleo SimForge.
//
// Fora do ctest de propósito: medições variam com a máquina e não devem
// quebrar CI. O objetivo é preencher `docs/spikes/resultados.md`.
//
// Uso:
//   simforge-bench                  tabela no console (ASCII)
//   simforge-bench --markdown       linhas Markdown (UTF-8) para a doc
//   simforge-bench --check          sai com 1 se algo exceder o limite
//
// Cada linha informa o spike, a métrica, o valor medido, o limite do
// `docs/spikes/resultados.md` e o status.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "simforge/simforge.hpp"

using namespace simforge;
using simforge::json::Value;

namespace {

using Clock = std::chrono::steady_clock;

struct Row {
    std::string spike;
    std::string metric;
    std::string note;
    double value = 0.0;
    std::string unit;
    double limit = 0.0;
    std::string limit_unit;
    bool measured = true;
    bool info_only = false;
    bool pass = true;
};

std::vector<Row> g_rows;

void record(std::string spike, std::string metric, double value, std::string unit,
            double limit, std::string limit_unit, std::string note = {}) {
    Row row;
    row.spike = std::move(spike);
    row.metric = std::move(metric);
    row.value = value;
    row.unit = std::move(unit);
    row.limit = limit;
    row.limit_unit = std::move(limit_unit);
    row.pass = value <= limit;
    row.note = std::move(note);
    g_rows.push_back(std::move(row));
}

void record_pending(std::string spike, std::string metric, std::string note) {
    Row row;
    row.spike = std::move(spike);
    row.metric = std::move(metric);
    row.note = std::move(note);
    row.measured = false;
    row.pass = false;
    g_rows.push_back(std::move(row));
}

/// Linha informativa: medida, sem limite do spike (não entra no --check).
void record_info(std::string spike, std::string metric, double value, std::string unit,
                 std::string note = {}) {
    Row row;
    row.spike = std::move(spike);
    row.metric = std::move(metric);
    row.value = value;
    row.unit = std::move(unit);
    row.info_only = true;
    row.note = std::move(note);
    g_rows.push_back(std::move(row));
}

/// Tempo em ms entre dois instantes.
double ms_between(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

/// Menor tempo de `repeats` execuções (reduz ruído de agendamento).
double best_ms(int repeats, const std::function<void()>& fn) {
    double best = 1e300;
    for (int r = 0; r < repeats; ++r) {
        const Clock::time_point start = Clock::now();
        fn();
        const Clock::time_point end = Clock::now();
        best = std::min(best, ms_between(start, end));
    }
    return best;
}

// ---------------------------------------------------------------------------
// Construção de documentos
// ---------------------------------------------------------------------------

Value base_document() {
    Value doc = Value::object();
    doc.set("format", Value("simforge.project"));
    doc.set("schema", Value(1));
    doc.set("project", Value::object());
    doc.set("boards", Value::array());
    doc.set("devices", Value::array());
    doc.set("nets", Value::array());
    return doc;
}

Value board_entry(std::string id) {
    Value entry = Value::object();
    entry.set("id", Value(std::move(id)));
    entry.set("type", Value("simforge.esp32s3.devkitc"));
    entry.set("params", Value::object());
    return entry;
}

Value device_entry(std::string id, std::string type, Value params = Value::object()) {
    Value entry = Value::object();
    entry.set("id", Value(std::move(id)));
    entry.set("type", Value(std::move(type)));
    entry.set("params", std::move(params));
    entry.set("pins", Value::object());
    return entry;
}

Value pin_ref(const char* kind, std::string owner, std::string pin) {
    Value ref = Value::object();
    ref.set(kind, Value(std::move(owner)));
    ref.set("pin", Value(std::move(pin)));
    return ref;
}

Value net_entry(std::string id, Value pins) {
    Value net = Value::object();
    net.set("id", Value(std::move(id)));
    net.set("name", Value(id.empty() ? std::string("net") : id));
    net.set("kind", Value("digital"));
    net.set("pins", std::move(pins));
    return net;
}

/// Placa + LED: GPIO2 -> LED.A, GND -> LED.K.
Value led_document() {
    Value doc = base_document();
    doc["boards"].push(board_entry("board1"));
    doc["devices"].push(device_entry("led1", "simforge.led"));

    Value anode = Value::array();
    anode.push(pin_ref("board", "board1", "GPIO2"));
    anode.push(pin_ref("device", "led1", "A"));
    Value cathode = Value::array();
    cathode.push(pin_ref("board", "board1", "GND"));
    cathode.push(pin_ref("device", "led1", "K"));

    doc["nets"].push(net_entry("n_a", std::move(anode)));
    doc["nets"].push(net_entry("n_k", std::move(cathode)));
    return doc;
}

/// Placa + LED + botão (o cenário `button-led` do spike S0.2).
Value button_led_document() {    Value doc = led_document();
    Value params = Value::object();
    params.set("pull", Value("up"));
    doc["devices"].push(device_entry("button1", "simforge.button", std::move(params)));

    Value signal = Value::array();
    signal.push(pin_ref("board", "board1", "GPIO0"));
    signal.push(pin_ref("device", "button1", "1"));

    // O terminal 2 do botão vai para o mesmo GND do LED: um pino de placa não
    // pode estar em duas nets.
    doc["nets"].push(net_entry("n_btn", std::move(signal)));
    doc["nets"].at(1)["pins"].push(pin_ref("device", "button1", "2"));
    return doc;
}

std::string padded(const std::string& prefix, int index, int width) {
    std::string digits = std::to_string(index);
    if (static_cast<int>(digits.size()) < width) {
        digits.insert(digits.begin(), static_cast<std::size_t>(width) -
                                          digits.size(), '0');
    }
    return prefix + digits;
}

/// `channels` clocks, cada um em uma net própria (spike S0.2, PWM).
Value pwm_document(int channels, double freq_hz, double duty) {
    Value doc = base_document();
    for (int i = 0; i < channels; ++i) {
        const std::string id = padded("clk", i, 2);
        Value params = Value::object();
        params.set("freq_hz", Value(freq_hz));
        params.set("duty", Value(duty));
        doc["devices"].push(device_entry(id, "simforge.clock", std::move(params)));

        Value pins = Value::array();
        pins.push(pin_ref("device", id, "OUT"));
        doc["nets"].push(net_entry("n_" + id, std::move(pins)));
    }
    return doc;
}

// ---------------------------------------------------------------------------
// Dispositivo de bancada: um driver forte por net
// ---------------------------------------------------------------------------

class BenchDriver final : public Device {
public:
    BenchDriver(std::string id, Level level) : id_(std::move(id)), level_(level) {}

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "bench.driver"; }

    std::span<const PinDesc> pins() const override {
        static const std::vector<PinDesc> kPins = {
            {"OUT", PinDirection::Output, "saída"},
        };
        return kPins;
    }

    void evaluate(DeviceContext& ctx) override {
        ctx.drive(0, level_ == Level::Z ? high_z() : drive(level_));
    }

    bool set_param(std::string_view key, const json::Value& value, SimTime /*now*/,
                   std::string* error) override {
        if (key != "level" || !value.is_string()) {
            if (error) *error = "esperado {level: low|high|z}";
            return false;
        }
        Level parsed = Level::Z;
        if (!parse_level(value.as_string(), parsed)) {
            if (error) *error = "nível inválido: " + value.as_string();
            return false;
        }
        level_ = parsed;
        return true;
    }

private:
    std::string id_;
    Level level_ = Level::Z;
};

void register_bench_driver() {
    DeviceRegistry::instance().add(DeviceType{
        "bench.driver",
        "Driver de bancada",
        "component",
        {{"OUT", PinDirection::Output, "saída"}},
        [](std::string id, const json::Value& params) -> std::unique_ptr<Device> {
            Level level = Level::Z;
            const Value raw = params.get("level", Value("z"));
            if (raw.is_string()) parse_level(raw.as_string(), level);
            return std::make_unique<BenchDriver>(std::move(id), level);
        }});
}

// ---------------------------------------------------------------------------
// S0.1 - fila e motor
// ---------------------------------------------------------------------------

void bench_engine_events() {
    Engine engine;
    std::string error;
    if (!engine.load(led_document(), &error)) {
        std::fprintf(stderr, "bench: engine.load falhou: %s\n", error.c_str());
        return;
    }

    // O spike S0.1 é sobre a fila: enfileirar e drenar são medidos separados.
    // A injeção via API pública resolve a cada chamada, então não é fila pura.
    const int total = 1'000'000;
    double best_enqueue = 1e300;
    double best_drain = 1e300;
    std::size_t drained = 0;

    for (int repeat = 0; repeat < 3; ++repeat) {
        engine.reset();
        std::string ignored;
        const Clock::time_point t0 = Clock::now();
        for (int i = 0; i < total; ++i) {
            engine.set_pin("board1", "GPIO2", (i % 2) == 0 ? Level::High : Level::Low,
                           &ignored);
        }
        const Clock::time_point t1 = Clock::now();
        drained = engine.step_events(static_cast<std::size_t>(total) * 2);
        const Clock::time_point t2 = Clock::now();
        best_enqueue = std::min(best_enqueue, ms_between(t0, t1));
        best_drain = std::min(best_drain, ms_between(t1, t2));
    }

    record("S0.1", "1 M eventos drenados da fila", best_drain, "ms", 150.0, "ms",
           std::to_string(static_cast<long long>(drained)) + " eventos processados");
    record_info("S0.1", "enfileirar 1 M eventos via API pública", best_enqueue, "ms",
                "inclui busca de pino por nome + solve por chamada");
    record_info("S0.1", "custo por evento no caminho público",
                best_enqueue * 1000.0 / static_cast<double>(total), "µs",
                "enfileirar (não é só a fila); teto para 100 k pendentes");
}

void bench_queue_isolated() {
    // Só a `priority_queue`: hooks neutros, sem resolver o circuito.
    Engine engine;
    const std::size_t pending = 100'000;
    const std::size_t extra = 10'000;
    double best_push = 1e300;
    double best_pop = 1e300;
    std::size_t popped = 0;

    for (int repeat = 0; repeat < 5; ++repeat) {
        engine.bench_dequeue(engine.bench_queue_size());
        engine.bench_enqueue(pending);

        const Clock::time_point t0 = Clock::now();
        engine.bench_enqueue(extra);
        const Clock::time_point t1 = Clock::now();
        popped = engine.bench_dequeue(pending + extra);
        const Clock::time_point t2 = Clock::now();

        best_push = std::min(best_push, ms_between(t0, t1));
        best_pop = std::min(best_pop, ms_between(t1, t2));
    }

    record("S0.1", "inserção na fila com 100 k pendentes",
           best_push * 1000.0 / static_cast<double>(extra), "µs", 2.0, "µs",
           "push puro, sem solve");
    record("S0.1", "remoção da fila com 100 k pendentes",
           best_pop * 1000.0 / static_cast<double>(popped), "µs", 2.0, "µs",
           "pop puro, sem processar handlers");
}

// ---------------------------------------------------------------------------
// S0.2 - solver elétrico
// ---------------------------------------------------------------------------

void bench_solve_many_drivers() {
    Engine engine;
    register_bench_driver();

    Value doc = base_document();
    doc["boards"].push(board_entry("board1"));
    Value pins = Value::array();
    const int drivers = 200;
    for (int i = 0; i < drivers; ++i) {
        const std::string id = padded("drv", i, 3);
        doc["devices"].push(device_entry(id, "bench.driver"));
        pins.push(pin_ref("device", id, "OUT"));
    }
    doc["nets"].push(net_entry("n_bus", std::move(pins)));

    std::string error;
    if (!engine.load(doc, &error)) {
        std::fprintf(stderr, "bench: load 200 drivers falhou: %s\n", error.c_str());
        return;
    }

    const int toggles = 20'000;
    double ms = best_ms(3, [&] {
        for (int i = 0; i < toggles; ++i) {
            const Value level((i % 2) == 0 ? "high" : "low");
            engine.set_device_param("drv000", "level", level, &error);
        }
    });
    record("S0.2", "net com 200 drivers alternando (solve)", ms * 1000.0 / toggles,
           "µs", 5.0, "µs", "1 net, 200 pinos, 1 driver ativo por vez");

    // Cenário button-led: 10 s virtuais com pressionamento a 5 Hz.
    Engine led;
    if (!led.load(button_led_document(), &error)) {
        std::fprintf(stderr, "bench: load button-led falhou: %s\n", error.c_str());
        return;
    }
    const int step_ms = 1;
    const int total_ms = 10'000;
    ms = best_ms(3, [&] {
        for (int t = 0; t < total_ms; t += step_ms) {
            const bool pressed = (t % 200) < 20;
            led.set_device_param("button1", "pressed", Value(pressed), &error);
            led.run_until(led.now() + kNsPerMs * step_ms);
        }
        led.reset();
    });
    record("S0.2", "cenário button-led (10 s virtuais)", ms, "ms", 20.0, "ms",
           "passo de 1 ms, botão 20 ms a cada 200 ms");
}

void bench_pwm() {
    // Critério: PWM 1 kHz em 8 canais consome ≤ 3% de um núcleo a 60 Hz.
    // Simular 1 s virtual dá 16 k bordas; 3% de 1 s = 30 ms.
    Engine engine;
    std::string error;
    if (!engine.load(pwm_document(8, 1000.0, 0.5), &error)) {
        std::fprintf(stderr, "bench: load PWM falhou: %s\n", error.c_str());
        return;
    }
    const double ms = best_ms(3, [&] {
        engine.reset();
        engine.run_for(1'000'000'000);
    });
    record("S0.2", "PWM 1 kHz em 8 canais (1 s virtual)", ms, "ms", 30.0, "ms",
           "16 k bordas agendadas, 8 nets");
}

// ---------------------------------------------------------------------------
// S0.4 - projeto
// ---------------------------------------------------------------------------

void bench_project_load() {
    // ~10 k objetos: 1 placa + 5 000 resistores + 5 000 nets.
    Value doc = base_document();
    doc["project"].set("id", Value(new_ulid()));
    doc["project"].set("name", Value("Bench 10k"));

    doc["boards"].push(board_entry("board1"));
    const int devices = 5'000;
    for (int i = 0; i < devices; ++i) {
        doc["devices"].push(device_entry(padded("r", i, 5), "simforge.resistor"));
    }
    for (int i = 0; i < devices; ++i) {
        Value pins = Value::array();
        pins.push(pin_ref("device", padded("r", i, 5), "A"));
        pins.push(pin_ref("device", padded("r", i, 5), "B"));
        doc["nets"].push(net_entry(padded("n", i, 5), std::move(pins)));
    }

    const std::string text = doc.dump(2);
    const double megabytes = static_cast<double>(text.size()) / (1024.0 * 1024.0);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "simforge_bench_10k.sforge";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    std::size_t diagnostics = 0;
    const double ms = best_ms(3, [&] {
        std::string error;
        auto loaded = Project::load(path, &error);
        if (!loaded) {
            std::fprintf(stderr, "bench: load 10k falhou: %s\n", error.c_str());
            return;
        }
        const auto found = loaded->validate();
        diagnostics = found.size();
    });
    std::filesystem::remove(path);

    record("S0.4", "projeto com 10 k objetos (abrir + validar)", ms, "ms", 250.0, "ms",
           "10 001 objetos, " + std::to_string(static_cast<long long>(diagnostics)) +
               " diagnósticos, " + std::to_string(static_cast<int>(megabytes * 100)) +
               "/100 MB");
}

// ---------------------------------------------------------------------------
// S0.5 - CommandBus
// ---------------------------------------------------------------------------

std::string request_json(const char* method, Value params, const char* id = "bench") {
    Value request = Value::object();
    request.set("id", Value(id));
    request.set("method", Value(method));
    request.set("params", std::move(params));
    return request.dump(0);
}

bool response_ok(const std::string& response) {
    return response.find("\"ok\":true") != std::string::npos;
}

void bench_session_request() {
    Session session;
    Value params = Value::object();
    params.set("blob", Value(std::string(1024, 'x')));
    const std::string request = request_json("app.ping", std::move(params));

    const int iterations = 2'000;
    const double ms = best_ms(5, [&] {
        for (int i = 0; i < iterations; ++i) {
            if (!response_ok(session.execute(request))) {
                std::fprintf(stderr, "bench: app.ping falhou\n");
                return;
            }
        }
    });
    record("S0.5", "requisição de 1 KB (app.ping, sem I/O)",
           ms * 1000.0 / iterations, "µs", 20.0, "µs",
           std::to_string(request.size()) + " bytes por requisição");
}

void bench_events_poll() {
    Session session;
    session.execute(request_json("project.new", Value::object()));

    const int mutations = 9'999;  // 1 (project.new) + 9 999 = 10 000 eventos
    double fill_ms = best_ms(1, [&] {
        for (int i = 0; i < mutations; ++i) {
            if (!response_ok(session.execute(
                    request_json("circuit.addDevice",
                                 [] {
                                     Value p = Value::object();
                                     p.set("type", Value("simforge.resistor"));
                                     return p;
                                 }())))) {
                std::fprintf(stderr, "bench: addDevice falhou\n");
                return;
            }
        }
    });
    record_info("S0.5", "10 k mutações via CommandBus (encher buffer)", fill_ms, "ms",
                "custo de emitir 10 k eventos + revisões");
    Value params = Value::object();
    params.set("max", Value(10'000));
    params.set("after_seq", Value(0));
    const std::string poll = request_json("events.poll", std::move(params));

    const double ms = best_ms(5, [&] {
        if (!response_ok(session.execute(poll))) {
            std::fprintf(stderr, "bench: events.poll falhou\n");
        }
    });
    record("S0.5", "10 k eventos por events.poll", ms, "ms", 50.0, "ms",
           "buffer cheio (limite de 10 k eventos)");
}

// ---------------------------------------------------------------------------
// S0.6 - frame
// ---------------------------------------------------------------------------

void bench_frame() {
    Value doc = base_document();
    doc["boards"].push(board_entry("board1"));
    const int count = 1'000;
    for (int i = 0; i < count; ++i) {
        doc["devices"].push(device_entry(padded("r", i, 4), "simforge.resistor"));
    }

    Engine engine;
    std::string error;
    if (!engine.load(doc, &error)) {
        std::fprintf(stderr, "bench: load 1k falhou: %s\n", error.c_str());
        return;
    }

    std::size_t bytes = 0;
    const double ms = best_ms(5, [&] {
        const Value frame = engine.frame(1);
        const std::string payload = frame.dump(0);
        bytes = payload.size();
    });
    record("S0.6", "frame completo com 1 k dispositivos (JSON)", ms, "ms", 4.0, "ms",
           std::to_string(static_cast<long long>(bytes)) + " bytes");

    record_pending("S0.6", "payload binário para 1 k dispositivos",
                   "transporte binário não implementado; JSON acima serve de teto");
}

// ---------------------------------------------------------------------------
// Relatório
// ---------------------------------------------------------------------------

void print_console() {
    std::printf("\nSimForge bench (%s)\n", SIMFORGE_BENCH_CONFIG);
    std::printf("%-6s %-46s %12s %-4s %10s %-4s %-9s\n", "spike", "métrica",
                "valor", "un", "limite", "un", "status");
    std::printf("%s\n", std::string(96, '-').c_str());
    for (const Row& row : g_rows) {
        const char* status = !row.measured ? "pendente"
                             : row.info_only ? "info"
                                             : (row.pass ? "ok" : "ACIMA");
        if (!row.measured) {
            std::printf("%-6s %-46s %12s %-4s %10s %-4s %-9s\n", row.spike.c_str(),
                        row.metric.c_str(), "-", "", "-", "", status);
        } else if (row.info_only) {
            std::printf("%-6s %-46s %12.2f %-4s %10s %-4s %-9s\n", row.spike.c_str(),
                        row.metric.c_str(), row.value, row.unit.c_str(), "-", "",
                        status);
        } else {
            std::printf("%-6s %-46s %12.2f %-4s %10.2f %-4s %-9s\n", row.spike.c_str(),
                        row.metric.c_str(), row.value, row.unit.c_str(), row.limit,
                        row.limit_unit.c_str(), status);
        }
        if (!row.note.empty()) std::printf("       nota: %s\n", row.note.c_str());
    }
    std::printf("\n");
}

void print_markdown() {
    std::printf("| spike | métrica | valor | limite | status | nota |\n");
    std::printf("|---|---|---|---|---|---|\n");
    for (const Row& row : g_rows) {
        char value[64] = "-";
        char limit[64] = "-";
        if (row.measured) {
            std::snprintf(value, sizeof(value), "%.2f %s", row.value, row.unit.c_str());
            if (!row.info_only) {
                std::snprintf(limit, sizeof(limit), "%.2f %s", row.limit,
                              row.limit_unit.c_str());
            }
        }
        const char* status = !row.measured ? "pendente"
                             : row.info_only ? "informativo"
                                             : (row.pass ? "ok" : "acima");
        std::printf("| %s | %s | %s | %s | %s | %s |\n", row.spike.c_str(),
                    row.metric.c_str(), value, limit, status, row.note.c_str());
    }
}

}  // namespace

int main(int argc, char** argv) {
#if defined(_WIN32)
    // Console e pipes recebem UTF-8 (métricas usam "µs").
    SetConsoleOutputCP(CP_UTF8);
#endif
    bool markdown = false;
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--markdown") == 0) {
            markdown = true;
        } else if (std::strcmp(argv[i], "--check") == 0) {
            check = true;
        } else {
            std::fprintf(stderr, "uso: %s [--markdown] [--check]\n", argv[0]);
            return 2;
        }
    }

    seed_ids(1);

    bench_engine_events();
    bench_queue_isolated();
    bench_solve_many_drivers();
    bench_pwm();
    bench_project_load();
    bench_session_request();
    bench_events_poll();
    bench_frame();

    if (markdown) {
        print_markdown();
    } else {
        print_console();
    }

    if (check) {
        for (const Row& row : g_rows) {
            if (row.measured && !row.pass) return 1;
        }
    }
    return 0;
}
