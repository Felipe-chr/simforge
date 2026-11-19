#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "simforge/board.hpp"
#include "simforge/device.hpp"

namespace simforge {
namespace {

class Esp32S3DevKitC final : public Board {
public:
    explicit Esp32S3DevKitC(std::string id) : id_(std::move(id)) {
        pins_.push_back({"3V3", PinDirection::Power, "alimentação 3,3 V"});
        pins_.push_back({"5V", PinDirection::Power, "alimentação 5 V"});
        pins_.push_back({"GND", PinDirection::Ground, "terra"});
        pins_.push_back({"EN", PinDirection::Input, "reset"});

        for (int gpio = 0; gpio <= 21; ++gpio) add_gpio(gpio);
        for (int gpio = 26; gpio <= 48; ++gpio) add_gpio(gpio);

        driven_.assign(pins_.size(), Level::Z);
        for (std::size_t i = 0; i < pins_.size(); ++i) {
            index_[pins_[i].name] = i;
            if (pins_[i].name.rfind("GPIO", 0) == 0) gpio_.push_back(pins_[i].name);
        }
    }

    std::string_view id() const override { return id_; }
    std::string_view type_id() const override { return "simforge.esp32s3.devkitc"; }
    std::span<const PinDesc> pins() const override { return pins_; }
    std::span<const std::string> gpio_pins() const override { return gpio_; }

    void reset() override {
        std::fill(driven_.begin(), driven_.end(), Level::Z);
    }

    void evaluate(DeviceContext& ctx) override {
        for (std::size_t i = 0; i < driven_.size(); ++i) {
            if (driven_[i] != Level::Z) ctx.drive(i, drive(driven_[i]));
        }
    }

    bool set_pin(std::string_view pin, Level level, std::string* error) override {
        const auto it = index_.find(std::string(pin));
        if (it == index_.end()) {
            if (error) *error = "pino inexistente na placa: " + std::string(pin);
            return false;
        }
        if (pins_[it->second].direction == PinDirection::Power ||
            pins_[it->second].direction == PinDirection::Ground) {
            if (error) *error = "pino fixo não pode ser dirigido: " + std::string(pin);
            return false;
        }
        driven_[it->second] = level;
        return true;
    }

    bool release_pin(std::string_view pin, std::string* error) override {
        const auto it = index_.find(std::string(pin));
        if (it == index_.end()) {
            if (error) *error = "pino inexistente na placa: " + std::string(pin);
            return false;
        }
        driven_[it->second] = Level::Z;
        return true;
    }

    Level driven_level(std::string_view pin) const override {
        const auto it = index_.find(std::string(pin));
        if (it == index_.end()) return Level::Z;
        return driven_[it->second];
    }

    json::Value read_state(SimTime) const override {
        json::Value state = json::Value::object();
        json::Value pins = json::Value::object();
        for (std::size_t i = 0; i < driven_.size(); ++i) {
            if (driven_[i] != Level::Z) pins.set(pins_[i].name, to_string(driven_[i]));
        }
        state.set("driven", std::move(pins));
        return state;
    }

private:
    void add_gpio(int gpio) {
        pins_.push_back({"GPIO" + std::to_string(gpio), PinDirection::Bidirectional,
                         "GPIO" + std::to_string(gpio)});
    }

    std::string id_;
    std::vector<PinDesc> pins_;
    std::vector<Level> driven_;
    std::unordered_map<std::string, std::size_t> index_;
    std::vector<std::string> gpio_;
};

}  // namespace

void register_builtin_boards() {
    DeviceRegistry::instance().add(DeviceType{
        "simforge.esp32s3.devkitc",
        "ESP32-S3 DevKitC-1",
        "board",
        {},  // descritor montado na fábrica abaixo
        [](std::string id, const json::Value&) -> std::unique_ptr<Device> {
            return std::make_unique<Esp32S3DevKitC>(std::move(id));
        }});
}

}  // namespace simforge
