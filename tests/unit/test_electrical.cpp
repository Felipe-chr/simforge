#include "framework.hpp"
#include "simforge/circuit.hpp"

using namespace simforge;

namespace {

// Recebe o estado de um único pino e devolve o resolvido.
Level single(Level level, Strength strength = Strength::Strong) {
    const PinState s{level, strength};
    return resolve_pin_states(&s, 1).level;
}

Level pair(Level a, Level b, Strength sa = Strength::Strong,
           Strength sb = Strength::Strong) {
    const PinState states[2] = {{a, sa}, {b, sb}};
    return resolve_pin_states(states, 2).level;
}

}  // namespace

TEST(electrical_single_driver) {
    CHECK(single(Level::Low) == Level::Low);
    CHECK(single(Level::High) == Level::High);
    // Z e X ficam como estão; Z sem força vira Z.
    CHECK(single(Level::Z) == Level::Z);
    CHECK(single(Level::X) == Level::X);
    CHECK(resolve_pin_states(nullptr, 0).level == Level::Z);
}

TEST(electrical_truth_table_16) {
    // Cada pino forte em um de {Low, High, Z, X}: 4x4 = 16 combinações.
    const Level levels[4] = {Level::Low, Level::High, Level::Z, Level::X};
    const Level expected[4][4] = {
        /*       Low        High       Z          X      */
        /* Low  */ {Level::Low, Level::X, Level::Low, Level::X},
        /* High */ {Level::X, Level::High, Level::High, Level::X},
        /* Z    */ {Level::Low, Level::High, Level::Z, Level::X},
        /* X    */ {Level::X, Level::X, Level::X, Level::X},
    };
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const Level got = pair(levels[i], levels[j]);
            if (got != expected[i][j]) {
                ST_FAIL(std::string("resolução errada em (") + to_string(levels[i]) +
                        "," + to_string(levels[j]) + "): veio " + to_string(got));
            }
        }
    }
}

TEST(electrical_strength_priority) {
    // Forte vence fraco.
    CHECK(pair(Level::High, Level::Low, Strength::Strong, Strength::Weak) == Level::High);
    CHECK(pair(Level::Low, Level::High, Strength::Strong, Strength::Weak) == Level::Low);
    // Dois fracos opostos -> X.
    CHECK(pair(Level::Low, Level::High, Strength::Weak, Strength::Weak) == Level::X);
    // Um fraco sozinho resolve o nível.
    CHECK(pair(Level::High, Level::Z, Strength::Weak, Strength::None) == Level::High);
    // Contenção de fortes -> X com força forte.
    const PinState states[2] = {drive_low(), drive_high()};
    const PinState resolved = resolve_pin_states(states, 2);
    CHECK(resolved.level == Level::X);
    CHECK(resolved.strength == Strength::Strong);
}

TEST(electrical_floating_net_is_z) {
    const PinState states[2] = {high_z(), high_z()};
    const PinState resolved = resolve_pin_states(states, 2);
    CHECK(resolved.level == Level::Z);
    CHECK(resolved.strength == Strength::None);
}

TEST(electrical_parse_roundtrip) {
    for (const Level level : {Level::Low, Level::High, Level::Z, Level::X}) {
        Level parsed = Level::Low;
        CHECK(parse_level(to_string(level), parsed));
        CHECK(parsed == level);
    }
    Level bad = Level::Low;
    CHECK(!parse_level("talvez", bad));
}

TEST(electrical_netlist_attach_detach) {
    Netlist netlist;
    netlist.add("n1", "sinal", NetKind::Digital);
    netlist.add("n2", "outro", NetKind::Digital);

    std::string error;
    const PinRef a{"board1", "GPIO2"};
    CHECK(netlist.attach(a, "n1", &error));
    CHECK(error.empty());
    const std::string* current = netlist.net_of(a);
    CHECK(current != nullptr);
    if (current) CHECK_EQ(*current, std::string("n1"));

    // Reanexar à mesma net é idempotente.
    CHECK(netlist.attach(a, "n1", &error));
    // Mover para outra net falha.
    CHECK(!netlist.attach(a, "n2", &error));
    CHECK(!error.empty());

    // Pino inexistente na net falha.
    CHECK(!netlist.attach(PinRef{"board1", "GPIO3"}, "n_missing", &error));

    CHECK(netlist.detach(a));
    CHECK(netlist.net_of(a) == nullptr);
    CHECK(!netlist.detach(a));  // já removido
}

TEST(electrical_netlist_lookup) {
    Netlist netlist;
    netlist.add("n1", "a", NetKind::Power);
    netlist.add("n2", "b", NetKind::Ground);
    CHECK(netlist.find("n1") != nullptr);
    CHECK(netlist.find("n2") != nullptr);
    CHECK(netlist.find("n3") == nullptr);
    CHECK_EQ(netlist.nets().size(), std::size_t(2));

    netlist.add("n3", "c", NetKind::Bus);
    CHECK_EQ(netlist.nets().size(), std::size_t(3));
    CHECK(netlist.remove("n2"));
    CHECK(!netlist.remove("n2"));
    CHECK(netlist.find("n2") == nullptr);
}
