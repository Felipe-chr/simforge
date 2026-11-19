#pragma once
// Framework de dispositivo: interface mínima, contexto de simulação e registro.

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "simforge/circuit.hpp"
#include "simforge/json.hpp"
#include "simforge/time.hpp"

namespace simforge {

enum class PinDirection { Input, Output, Bidirectional, Power, Ground };

const char* to_string(PinDirection direction);

struct PinDesc {
    std::string name;
    PinDirection direction = PinDirection::Bidirectional;
    std::string description;
};

/// O que um dispositivo pode fazer durante a avaliação.
class DeviceContext {
public:
    virtual ~DeviceContext() = default;
    virtual SimTime now() const = 0;
    /// Nível resolvido da net ligada ao pino local (Z se não conectado).
    virtual Level net_level(std::size_t local_pin) const = 0;
    /// Estado dirigido pelo pino local no ciclo atual.
    virtual void drive(std::size_t local_pin, PinState state) = 0;
};

class Device {
public:
    virtual ~Device() = default;
    virtual std::string_view id() const = 0;
    virtual std::string_view type_id() const = 0;
    virtual std::span<const PinDesc> pins() const = 0;
    /// Chamado em cada iteração do solver; deve ser puro em relação ao tempo.
    virtual void evaluate(DeviceContext& ctx) = 0;
    virtual void reset() {}
    /// Próxima borda de uma fonte periódica (PWM/clock), estritamente > `now`.
    /// Devolve -1 quando o dispositivo não gera eventos próprios. O motor
    /// agenda um wakeup nesse instante e volta a perguntar depois.
    virtual SimTime next_edge(SimTime now) const {
        (void)now;
        return -1;
    }
    /// Alteração de parâmetro em tempo de execução (`circuit.setDevice`).
    virtual bool set_param(std::string_view key, const json::Value& value,
                           SimTime now, std::string* error) {
        (void)key;
        (void)value;
        (void)now;
        if (error) *error = "parâmetro não suportado";
        return false;
    }
    /// Estado observável para `sim.frame` (sem regra de negócio).
    virtual json::Value read_state(SimTime now) const {
        (void)now;
        return json::Value::object();
    }
};

struct DeviceType {
    std::string id;
    std::string name;
    std::string category = "component";
    std::vector<PinDesc> pins;
    std::function<std::unique_ptr<Device>(std::string id, const json::Value& params)>
        factory;
};

class DeviceRegistry {
public:
    static DeviceRegistry& instance();

    bool add(DeviceType type);
    const DeviceType* find(std::string_view id) const;
    std::unique_ptr<Device> create(std::string_view type, std::string id,
                                   const json::Value& params) const;
    std::vector<const DeviceType*> all() const;

private:
    std::vector<DeviceType> types_;
};

/// Registra LED, botão, resistor e a placa ESP32-S3.
void register_builtin_devices();

}  // namespace simforge
