#pragma once
// Modelo elétrico digital: níveis, forças, resolução de net e netlist.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace simforge {

/// Quatro estados: Low, High, alta impedância e indeterminado.
enum class Level : std::uint8_t { Low = 0, High = 1, Z = 2, X = 3 };

/// Força de quem dirige o pino. `Strong` vence `Weak`.
enum class Strength : std::uint8_t { None = 0, Weak = 1, Strong = 2 };

struct PinState {
    Level level = Level::Z;
    Strength strength = Strength::None;
};

const char* to_string(Level level);
const char* to_string(Strength strength);
bool parse_level(std::string_view text, Level& out);
bool parse_strength(std::string_view text, Strength& out);

constexpr PinState drive_low() { return {Level::Low, Strength::Strong}; }
constexpr PinState drive_high() { return {Level::High, Strength::Strong}; }
constexpr PinState drive(Level level) { return {level, Strength::Strong}; }
constexpr PinState weak(Level level) { return {level, Strength::Weak}; }
constexpr PinState high_z() { return {}; }

/// Resolução de net por wire-AND com detecção de contenção (ADR 0005).
/// - dois níveis fortes opostos  -> X (contenção)
/// - um nível forte             -> esse nível
/// - só fracos                  -> nível fraco (ou X se opostos)
/// - nada                       -> Z
PinState resolve_pin_states(const PinState* states, std::size_t count);

enum class NetKind { Digital, Analog, Power, Ground, Bus };
const char* to_string(NetKind kind);
bool parse_net_kind(std::string_view text, NetKind& out);

/// Referência a um pino de placa ou dispositivo.
struct PinRef {
    std::string owner_id;
    std::string pin;
};

bool operator==(const PinRef& a, const PinRef& b);
bool operator<(const PinRef& a, const PinRef& b);

/// Netlist editável do documento (fonte de verdade da conectividade).
class Netlist {
public:
    struct Net {
        std::string id;
        std::string name;
        NetKind kind = NetKind::Digital;
        std::vector<PinRef> pins;
    };

    Net& add(std::string id, std::string name, NetKind kind);
    bool remove(std::string_view id);
    Net* find(std::string_view id);
    const Net* find(std::string_view id) const;
    const std::vector<Net>& nets() const { return nets_; }

    /// Liga o pino à net. Falha se o pino já estiver em outra net.
    bool attach(const PinRef& pin, std::string_view net_id, std::string* error);

    /// Remove o pino de qualquer net.
    bool detach(const PinRef& pin);

    /// ID da net do pino, ou nullptr.
    const std::string* net_of(const PinRef& pin) const;

private:
    std::vector<Net> nets_;
};

}  // namespace simforge
