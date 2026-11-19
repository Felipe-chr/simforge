#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "simforge/device.hpp"

namespace simforge {
namespace {

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
class Led final : public Device {
public:
    Led(std::string id, const json::Value& params)
        : id_(std::move(id)),
          color_(params.get("color", json::Value("red")).is_string()
                     ? params.get("color", json::Value("red")).as_string()
                     : "red") {
        const json::Value vf = params.get("vf", json::Value(1.9));
        if (vf.is_number()) vf_ = vf.as_double();
    }

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "simforge.led"; }

    std::span<const PinDesc> pins() const override {
        static const std::vector<PinDesc> kPins = {
            {"A", PinDirection::Input, "anodo"},
            {"K", PinDirection::Input, "catodo"},
        };
        return kPins;
    }

    void reset() override {
        last_on_ = false;
        last_change_ = 0;
        high_ns_ = 0;
        start_ = 0;
    }

    void evaluate(DeviceContext& ctx) override {
        const Level anode = ctx.net_level(0);
        const Level cathode = ctx.net_level(1);
        const bool on = (anode == Level::High) && (cathode != Level::High);
        const SimTime now = ctx.now();
        if (on != last_on_) {
            if (last_on_) high_ns_ += now - last_change_;
            last_change_ = now;
            last_on_ = on;
        }
    }

    json::Value read_state(SimTime now) const override {
        json::Value state = json::Value::object();
        state.set("on", last_on_);
        state.set("brightness", duty(now));
        state.set("color", color_);
        return state;
    }

private:
    double duty(SimTime now) const {
        if (now <= start_) return last_on_ ? 1.0 : 0.0;
        SimTime high = high_ns_;
        if (last_on_) high += now - last_change_;
        double value = static_cast<double>(high) / static_cast<double>(now - start_);
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
        return value;
    }

    std::string id_;
    std::string color_;
    double vf_ = 1.9;
    bool last_on_ = false;
    SimTime last_change_ = 0;
    SimTime high_ns_ = 0;
    SimTime start_ = 0;
};

// ---------------------------------------------------------------------------
// Botão
// ---------------------------------------------------------------------------
class Button final : public Device {
public:
    Button(std::string id, const json::Value& params) : id_(std::move(id)) {
        const json::Value pull = params.get("pull", json::Value("up"));
        if (pull.is_string()) {
            const std::string& text = pull.as_string();
            if (text == "up") pull_ = Level::High;
            else if (text == "down") pull_ = Level::Low;
            else pull_ = Level::Z;
        }
        const json::Value pressed = params.get("pressed", json::Value(false));
        pressed_ = pressed.is_bool() && pressed.as_bool();
    }

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "simforge.button"; }

    std::span<const PinDesc> pins() const override {
        static const std::vector<PinDesc> kPins = {
            {"1", PinDirection::Bidirectional, "terminal 1"},
            {"2", PinDirection::Bidirectional, "terminal 2"},
        };
        return kPins;
    }

    void evaluate(DeviceContext& ctx) override {
        if (pressed_) {
            ctx.drive(0, drive_low());
            return;
        }
        if (pull_ != Level::Z) ctx.drive(0, weak(pull_));
    }

    void set_pressed(bool pressed, SimTime now) {
        if (pressed == pressed_) return;
        pressed_ = pressed;
        ++changes_;
        last_change_ = now;
    }

    bool set_param(std::string_view key, const json::Value& value, SimTime now,
                   std::string* error) override {
        if (key == "pressed") {
            if (!value.is_bool()) {
                if (error) *error = "pressed deve ser booleano";
                return false;
            }
            set_pressed(value.as_bool(), now);
            return true;
        }
        if (error) *error = "parâmetro não suportado: " + std::string(key);
        return false;
    }

    bool pressed() const { return pressed_; }

    json::Value read_state(SimTime /*now*/) const override {
        json::Value state = json::Value::object();
        state.set("pressed", pressed_);
        state.set("changes", static_cast<long long>(changes_));
        state.set("last_change_ns", static_cast<long long>(last_change_));
        return state;
    }

private:
    std::string id_;
    Level pull_ = Level::High;
    bool pressed_ = false;
    std::uint64_t changes_ = 0;
    SimTime last_change_ = 0;
};

// ---------------------------------------------------------------------------
// Resistor (passa nível, sem carga elétrica no modo lógico)
// ---------------------------------------------------------------------------
class Resistor final : public Device {
public:
    Resistor(std::string id, const json::Value& params) : id_(std::move(id)) {
        const json::Value ohms = params.get("ohms", json::Value(1000));
        if (ohms.is_number()) ohms_ = ohms.as_double();
    }

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "simforge.resistor"; }

    std::span<const PinDesc> pins() const override {
        static const std::vector<PinDesc> kPins = {
            {"A", PinDirection::Bidirectional, "terminal A"},
            {"B", PinDirection::Bidirectional, "terminal B"},
        };
        return kPins;
    }

    void evaluate(DeviceContext& ctx) override {
        const Level a = ctx.net_level(0);
        const Level b = ctx.net_level(1);
        if (a != Level::Z && a != Level::X) {
            ctx.drive(1, weak(a));
        } else if (b != Level::Z && b != Level::X) {
            ctx.drive(0, weak(b));
        }
    }

    json::Value read_state(SimTime) const override {
        json::Value state = json::Value::object();
        state.set("ohms", ohms_);
        return state;
    }

private:
    std::string id_;
    double ohms_ = 1000.0;
};

// ---------------------------------------------------------------------------
// Clock / PWM por software
// ---------------------------------------------------------------------------
class Clock final : public Device {
public:
    Clock(std::string id, const json::Value& params) : id_(std::move(id)) {
        const json::Value freq = params.get("freq_hz", json::Value(1000.0));
        if (freq.is_number() && freq.as_double() > 0.0) freq_hz_ = freq.as_double();
        const json::Value duty = params.get("duty", json::Value(0.5));
        if (duty.is_number()) set_duty(duty.as_double());
    }

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "simforge.clock"; }

    std::span<const PinDesc> pins() const override {
        static const std::vector<PinDesc> kPins = {
            {"OUT", PinDirection::Output, "saída digital"},
        };
        return kPins;
    }

    void evaluate(DeviceContext& ctx) override {
        ctx.drive(0, drive(level_at(ctx.now()) ? Level::High : Level::Low));
    }

    SimTime next_edge(SimTime now) const override {
        if (duty_ <= 0.0 || duty_ >= 1.0) return -1;
        const SimTime p = period_ns();
        if (p <= 1) return -1;
        const SimTime high = high_ns(p);
        const SimTime rem = now % p;
        SimTime next = (now / p) * p + (rem < high ? high : p);
        while (next <= now) next += p;
        return next;
    }

    bool set_param(std::string_view key, const json::Value& value, SimTime /*now*/,
                   std::string* error) override {
        if (!value.is_number()) {
            if (error) *error = std::string(key) + " deve ser numérico";
            return false;
        }
        if (key == "freq_hz") {
            const double freq = value.as_double();
            if (freq <= 0.0) {
                if (error) *error = "freq_hz deve ser positiva";
                return false;
            }
            freq_hz_ = freq;
            return true;
        }
        if (key == "duty") {
            set_duty(value.as_double());
            return true;
        }
        if (error) *error = "parâmetro não suportado: " + std::string(key);
        return false;
    }

    json::Value read_state(SimTime now) const override {
        json::Value state = json::Value::object();
        state.set("on", level_at(now));
        state.set("freq_hz", freq_hz_);
        state.set("duty", duty_);
        state.set("period_ns", static_cast<long long>(period_ns()));
        return state;
    }

private:
    void set_duty(double duty) {
        if (duty < 0.0) duty = 0.0;
        if (duty > 1.0) duty = 1.0;
        duty_ = duty;
    }

    SimTime period_ns() const {
        const double ns = 1e9 / freq_hz_;
        if (ns < 2.0) return 2;
        return static_cast<SimTime>(std::llround(ns));
    }

    SimTime high_ns(SimTime period) const {
        SimTime high = static_cast<SimTime>(
            std::llround(static_cast<double>(period) * duty_));
        if (high < 1) high = 1;
        if (high > period - 1) high = period - 1;
        return high;
    }

    bool level_at(SimTime now) const {
        const SimTime p = period_ns();
        if (p <= 0 || duty_ <= 0.0) return false;
        if (duty_ >= 1.0) return true;
        return (now % p) < high_ns(p);
    }

    std::string id_;
    double freq_hz_ = 1000.0;
    double duty_ = 0.5;
};

}  // namespace

void register_builtin_devices() {
    DeviceRegistry& registry = DeviceRegistry::instance();

    registry.add(DeviceType{
        "simforge.led",
        "LED",
        "component",
        {{"A", PinDirection::Input, "anodo"}, {"K", PinDirection::Input, "catodo"}},
        [](std::string id, const json::Value& params) -> std::unique_ptr<Device> {
            return std::make_unique<Led>(std::move(id), params);
        }});

    registry.add(DeviceType{
        "simforge.button",
        "Botão",
        "component",
        {{"1", PinDirection::Bidirectional, "terminal 1"},
         {"2", PinDirection::Bidirectional, "terminal 2"}},
        [](std::string id, const json::Value& params) -> std::unique_ptr<Device> {
            return std::make_unique<Button>(std::move(id), params);
        }});

    registry.add(DeviceType{
        "simforge.resistor",
        "Resistor",
        "component",
        {{"A", PinDirection::Bidirectional, "terminal A"},
         {"B", PinDirection::Bidirectional, "terminal B"}},
        [](std::string id, const json::Value& params) -> std::unique_ptr<Device> {
            return std::make_unique<Resistor>(std::move(id), params);
        }});

    registry.add(DeviceType{
        "simforge.clock",
        "Clock/PWM",
        "component",
        {{"OUT", PinDirection::Output, "saída digital"}},
        [](std::string id, const json::Value& params) -> std::unique_ptr<Device> {
            return std::make_unique<Clock>(std::move(id), params);
        }});
}

}  // namespace simforge
