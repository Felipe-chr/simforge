#include "simforge/circuit.hpp"

#include <algorithm>

namespace simforge {

const char* to_string(Level level) {
    switch (level) {
        case Level::Low: return "low";
        case Level::High: return "high";
        case Level::Z: return "z";
        case Level::X: return "x";
    }
    return "?";
}

const char* to_string(Strength strength) {
    switch (strength) {
        case Strength::None: return "none";
        case Strength::Weak: return "weak";
        case Strength::Strong: return "strong";
    }
    return "?";
}

bool parse_level(std::string_view text, Level& out) {
    if (text == "low" || text == "0") { out = Level::Low; return true; }
    if (text == "high" || text == "1") { out = Level::High; return true; }
    if (text == "z") { out = Level::Z; return true; }
    if (text == "x") { out = Level::X; return true; }
    return false;
}

bool parse_strength(std::string_view text, Strength& out) {
    if (text == "none") { out = Strength::None; return true; }
    if (text == "weak") { out = Strength::Weak; return true; }
    if (text == "strong") { out = Strength::Strong; return true; }
    return false;
}

PinState resolve_pin_states(const PinState* states, std::size_t count) {
    bool strong_low = false;
    bool strong_high = false;
    bool weak_low = false;
    bool weak_high = false;
    bool any_x = false;
    bool any_strong = false;
    bool any_weak = false;

    for (std::size_t i = 0; i < count; ++i) {
        const PinState s = states[i];
        if (s.strength == Strength::None) continue;
        switch (s.level) {
            case Level::Low:
                if (s.strength == Strength::Strong) { strong_low = true; any_strong = true; }
                else { weak_low = true; any_weak = true; }
                break;
            case Level::High:
                if (s.strength == Strength::Strong) { strong_high = true; any_strong = true; }
                else { weak_high = true; any_weak = true; }
                break;
            case Level::X:
                any_x = true;
                if (s.strength == Strength::Strong) any_strong = true;
                else any_weak = true;
                break;
            case Level::Z:
                break;
        }
    }

    if (any_x) return {Level::X, any_strong ? Strength::Strong : Strength::Weak};
    if (any_strong) {
        if (strong_low && strong_high) return {Level::X, Strength::Strong};
        return {strong_high ? Level::High : Level::Low, Strength::Strong};
    }
    if (any_weak) {
        if (weak_low && weak_high) return {Level::X, Strength::Weak};
        return {weak_high ? Level::High : Level::Low, Strength::Weak};
    }
    return high_z();
}

const char* to_string(NetKind kind) {
    switch (kind) {
        case NetKind::Digital: return "digital";
        case NetKind::Analog: return "analog";
        case NetKind::Power: return "power";
        case NetKind::Ground: return "ground";
        case NetKind::Bus: return "bus";
    }
    return "?";
}

bool parse_net_kind(std::string_view text, NetKind& out) {
    if (text == "digital") { out = NetKind::Digital; return true; }
    if (text == "analog") { out = NetKind::Analog; return true; }
    if (text == "power") { out = NetKind::Power; return true; }
    if (text == "ground") { out = NetKind::Ground; return true; }
    if (text == "bus") { out = NetKind::Bus; return true; }
    return false;
}

bool operator==(const PinRef& a, const PinRef& b) {
    return a.owner_id == b.owner_id && a.pin == b.pin;
}

bool operator<(const PinRef& a, const PinRef& b) {
    if (a.owner_id != b.owner_id) return a.owner_id < b.owner_id;
    return a.pin < b.pin;
}

Netlist::Net& Netlist::add(std::string id, std::string name, NetKind kind) {
    nets_.push_back(Net{std::move(id), std::move(name), kind, {}});
    return nets_.back();
}

bool Netlist::remove(std::string_view id) {
    const auto it = std::find_if(nets_.begin(), nets_.end(),
                                 [&](const Net& n) { return n.id == id; });
    if (it == nets_.end()) return false;
    nets_.erase(it);
    return true;
}

Netlist::Net* Netlist::find(std::string_view id) {
    for (auto& net : nets_) {
        if (net.id == id) return &net;
    }
    return nullptr;
}

const Netlist::Net* Netlist::find(std::string_view id) const {
    for (const auto& net : nets_) {
        if (net.id == id) return &net;
    }
    return nullptr;
}

bool Netlist::attach(const PinRef& pin, std::string_view net_id, std::string* error) {
    const std::string* current = net_of(pin);
    if (current && *current != net_id) {
        if (error) {
            *error = "pino " + pin.owner_id + "." + pin.pin + " já pertence à net " +
                     *current;
        }
        return false;
    }
    Net* net = find(net_id);
    if (!net) {
        if (error) *error = "net inexistente: " + std::string(net_id);
        return false;
    }
    if (current) return true;
    net->pins.push_back(pin);
    return true;
}

bool Netlist::detach(const PinRef& pin) {
    bool removed = false;
    for (auto& net : nets_) {
        const auto it = std::remove(net.pins.begin(), net.pins.end(), pin);
        if (it != net.pins.end()) {
            net.pins.erase(it, net.pins.end());
            removed = true;
        }
    }
    return removed;
}

const std::string* Netlist::net_of(const PinRef& pin) const {
    for (const auto& net : nets_) {
        for (const auto& p : net.pins) {
            if (p == pin) return &net.id;
        }
    }
    return nullptr;
}

}  // namespace simforge
