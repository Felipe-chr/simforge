#pragma once
// Placa: dispositivo com pinos de MCU controláveis pelo runtime/dev.

#include <span>
#include <string>
#include <string_view>

#include "simforge/device.hpp"

namespace simforge {

class Board : public Device {
public:
    /// Dirige um pino de saída (nível forte).
    virtual bool set_pin(std::string_view pin, Level level, std::string* error) = 0;
    /// Devolve o pino para alta impedância.
    virtual bool release_pin(std::string_view pin, std::string* error) = 0;
    /// Último nível dirigido (Z se solto).
    virtual Level driven_level(std::string_view pin) const = 0;
    /// Nomes dos pinos de GPIO.
    virtual std::span<const std::string> gpio_pins() const = 0;
};

/// Registra a ESP32-S3 DevKitC e futuras placas embutidas.
void register_builtin_boards();

}  // namespace simforge
