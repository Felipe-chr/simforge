#include "simforge/device.hpp"

#include <algorithm>

namespace simforge {

const char* to_string(PinDirection direction) {
    switch (direction) {
        case PinDirection::Input: return "input";
        case PinDirection::Output: return "output";
        case PinDirection::Bidirectional: return "bidirectional";
        case PinDirection::Power: return "power";
        case PinDirection::Ground: return "ground";
    }
    return "?";
}

DeviceRegistry& DeviceRegistry::instance() {
    static DeviceRegistry registry;
    return registry;
}

bool DeviceRegistry::add(DeviceType type) {
    if (find(type.id)) return false;
    types_.push_back(std::move(type));
    return true;
}

const DeviceType* DeviceRegistry::find(std::string_view id) const {
    for (const auto& type : types_) {
        if (type.id == id) return &type;
    }
    return nullptr;
}

std::unique_ptr<Device> DeviceRegistry::create(std::string_view type, std::string id,
                                               const json::Value& params) const {
    const DeviceType* info = find(type);
    if (!info || !info->factory) return nullptr;
    return info->factory(std::move(id), params);
}

std::vector<const DeviceType*> DeviceRegistry::all() const {
    std::vector<const DeviceType*> out;
    out.reserve(types_.size());
    for (const auto& type : types_) out.push_back(&type);
    std::sort(out.begin(), out.end(),
              [](const DeviceType* a, const DeviceType* b) { return a->id < b->id; });
    return out;
}

}  // namespace simforge
