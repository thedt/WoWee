// Wrath creature motion through the same packet dispatch used in the world.
#include "game_replay.hpp"
#include "core/creature_locomotion.hpp"
#include "game/entity.hpp"
#include "game/spline_packet.hpp"
#include "game/world_packets.hpp"

using namespace wowee::game;
using wowee::network::Packet;

namespace {
constexpr uint64_t kCreature = 0xF130000000000123ull;
constexpr uint32_t kWalk = static_cast<uint32_t>(MovementFlags::WALKING);

// Object::BuildMovementUpdate for a living creature, followed by empty fields.
Packet livingCreature(float walk, float run) {
    Packet p(wireOpcode(Opcode::SMSG_UPDATE_OBJECT));
    p.writeUInt32(1);
    p.writeUInt8(static_cast<uint8_t>(UpdateType::CREATE_OBJECT2));
    p.writePackedGuid(kCreature);
    p.writeUInt8(static_cast<uint8_t>(ObjectType::UNIT));
    p.writeUInt16(0x20); // UPDATEFLAG_LIVING
    p.writeUInt32(0);    // movement flags
    p.writeUInt16(0);    // movement flags 2
    p.writeUInt32(0);    // time
    for (int i = 0; i < 4; ++i) p.writeFloat(0.0f); // position and orientation
    p.writeUInt32(0);    // fall time
    p.writeFloat(walk);
    p.writeFloat(run);
    for (int i = 0; i < 7; ++i) p.writeFloat(1.0f); // remaining speeds
    p.writeUInt8(0);     // no update fields
    return p;
}

// PacketBuilder::WriteMonsterMove, with one destination or a curved path.
Packet creatureMove(uint32_t duration, uint32_t splineFlags = 0, bool curved = false) {
    Packet p(wireOpcode(Opcode::SMSG_MONSTER_MOVE));
    p.writePackedGuid(kCreature);
    p.writeUInt8(0);
    p.writeFloat(0.0f); p.writeFloat(0.0f); p.writeFloat(0.0f);
    p.writeUInt32(123);
    p.writeUInt8(0); // normal movement
    p.writeUInt32(splineFlags);
    p.writeUInt32(duration);
    p.writeUInt32(curved ? 2 : 1);
    if (curved) {
        p.writeFloat(5.0f); p.writeFloat(10.0f); p.writeFloat(0.0f);
    }
    p.writeFloat(10.0f); p.writeFloat(0.0f); p.writeFloat(0.0f);
    return p;
}

Packet creatureMode(Opcode opcode) {
    Packet p(wireOpcode(opcode));
    p.writePackedGuid(kCreature);
    return p;
}

Packet creatureSpeed(Opcode opcode, float speed) {
    Packet p = creatureMode(opcode);
    p.writeFloat(speed);
    return p;
}

Packet creatureStop() {
    Packet p(wireOpcode(Opcode::SMSG_MONSTER_MOVE));
    p.writePackedGuid(kCreature);
    p.writeUInt8(0);
    p.writeFloat(10.0f); p.writeFloat(0.0f); p.writeFloat(0.0f);
    p.writeUInt32(124);
    p.writeUInt8(1);
    return p;
}

std::shared_ptr<Unit> creature(replay::Session& session) {
    return std::dynamic_pointer_cast<Unit>(session.handler().getEntityManager().getEntity(kCreature));
}
} // namespace

TEST_CASE("Wrath NPC animation follows spline speed without changing movement flags", "[replay][movement][npc]") {
    replay::Session session;
    session.receive(livingCreature(2.5f, 7.0f));
    const auto npc = creature(session);
    REQUIRE(npc);
    REQUIRE(npc->getWalkSpeed() == Catch::Approx(2.5f));
    REQUIRE(npc->getRunSpeed() == Catch::Approx(7.0f));

    bool flagWalking = false;
    unsigned flagUpdates = 0;
    uint32_t selected = 0;
    session.handler().setUnitMoveFlagsCallback([&](uint64_t guid, uint32_t flags, uint32_t mask) {
        REQUIRE(guid == kCreature);
        ++flagUpdates;
        if (mask & kWalk) flagWalking = (flags & kWalk) != 0;
    });
    session.handler().setCreatureMoveCallback([&](uint64_t guid, float, float, float, uint32_t duration) {
        REQUIRE(guid == kCreature);
        if (duration)
            selected = wowee::core::movingCreatureAnimation(false, false, npc->walkingForAnimation(flagWalking));
    });

    session.receive(creatureMove(4000)); // 10 yards at 2.5 yards/s
    CHECK(flagUpdates == 0);
    CHECK_FALSE(flagWalking);
    CHECK(selected == wowee::rendering::anim::WALK);
    npc->updateMovement(1.0f);
    CHECK(npc->getY() == Catch::Approx(2.5f)); // unchanged server timing

    session.receive(creatureMove(1429)); // same flags, approximately 7 yards/s
    CHECK(flagUpdates == 0);
    CHECK(selected == wowee::rendering::anim::RUN);

    // A fresh explicit mode overrides the inference for the active spline.
    session.receive(creatureMode(Opcode::SMSG_SPLINE_MOVE_SET_WALK_MODE));
    CHECK(flagWalking);
    CHECK(npc->walkingForAnimation(flagWalking));
    session.receive(creatureMode(Opcode::SMSG_SPLINE_MOVE_SET_RUN_MODE));
    CHECK_FALSE(flagWalking);
    CHECK_FALSE(npc->walkingForAnimation(flagWalking));

    session.receive(creatureMove(4000));
    REQUIRE(npc->walkingForAnimation(false));
    session.receive(creatureStop());
    CHECK_FALSE(npc->isActivelyMoving());
    CHECK_FALSE(npc->walkingForAnimation(false));
}

TEST_CASE("NPC spline inference uses updated per-creature speeds and full path", "[replay][movement][npc]") {
    replay::Session session;
    session.receive(livingCreature(2.5f, 7.0f));
    auto npc = creature(session);
    REQUIRE(npc);

    session.receive(creatureSpeed(Opcode::SMSG_SPLINE_SET_WALK_SPEED, 5.0f));
    session.receive(creatureSpeed(Opcode::SMSG_SPLINE_SET_RUN_SPEED, 14.0f));
    CHECK(npc->getWalkSpeed() == Catch::Approx(5.0f));
    CHECK(npc->getRunSpeed() == Catch::Approx(14.0f));
    session.receive(creatureMove(2000)); // 5 yards/s is this creature's walk
    CHECK(npc->walkingForAnimation(false));

    session.receive(creatureSpeed(Opcode::SMSG_SPLINE_SET_WALK_SPEED, 0.0f));
    CHECK(npc->getWalkSpeed() == Catch::Approx(5.0f));
    session.receive(creatureSpeed(Opcode::SMSG_SPLINE_SET_WALK_SPEED, 2.5f));
    session.receive(creatureSpeed(Opcode::SMSG_SPLINE_SET_RUN_SPEED, 7.0f));
    // Each segment is sqrt(125) yards. The full path is about 22.36 yards:
    // 7 yards/s over 3194 ms, even though its endpoint chord is only 10 yards.
    session.receive(creatureMove(3194, SplineFlagWotlk::CATMULLROM, true));
    CHECK_FALSE(npc->walkingForAnimation(true));
}

TEST_CASE("NPC spline inference falls back when speeds or ground motion are unknown", "[replay][movement][npc]") {
    replay::Session session;
    session.receive(replay::UpdateObject().create(kCreature, ObjectType::UNIT, {}));
    auto npc = creature(session);
    REQUIRE(npc);
    session.receive(creatureMove(4000));
    CHECK_FALSE(npc->walkingForAnimation(false));
    CHECK(npc->walkingForAnimation(true));

    // A flying spline must not be classified as ground walking or running.
    npc->setWalkSpeed(2.5f);
    npc->setRunSpeed(7.0f);
    session.receive(creatureMove(1429, SplineFlagWotlk::FLYING));
    CHECK(npc->walkingForAnimation(true));

    // Scripted speeds which don't resemble either ground gait should preserve
    // the existing movement mode rather than forcing a walk or run.
    session.receive(creatureMove(10000));
    CHECK_FALSE(npc->walkingForAnimation(false));
    session.receive(creatureMove(500));
    CHECK(npc->walkingForAnimation(true));
}
