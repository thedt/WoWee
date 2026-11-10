// Entity, Unit, Player, GameObject, EntityManager tests
#include <catch_amalgamated.hpp>
#include "game/entity.hpp"
#include "game/npc_spline_locomotion.hpp"
#include "core/creature_locomotion.hpp"
#include <algorithm>
#include <memory>

using namespace wowee::game;

TEST_CASE("Entity default construction", "[entity]") {
    Entity e;
    REQUIRE(e.getGuid() == 0);
    REQUIRE(e.getType() == ObjectType::OBJECT);
    REQUIRE(e.getX() == 0.0f);
    REQUIRE(e.getY() == 0.0f);
    REQUIRE(e.getZ() == 0.0f);
    REQUIRE(e.getOrientation() == 0.0f);
}

TEST_CASE("Entity GUID constructor", "[entity]") {
    Entity e(0xDEADBEEF);
    REQUIRE(e.getGuid() == 0xDEADBEEF);
}

TEST_CASE("Entity position set/get", "[entity]") {
    Entity e;
    e.setPosition(1.0f, 2.0f, 3.0f, 1.57f);
    REQUIRE(e.getX() == Catch::Approx(1.0f));
    REQUIRE(e.getY() == Catch::Approx(2.0f));
    REQUIRE(e.getZ() == Catch::Approx(3.0f));
    REQUIRE(e.getOrientation() == Catch::Approx(1.57f));
}

TEST_CASE("Entity ignores positive-duration movement without displacement",
          "[entity][movement]") {
    Entity e;
    e.setPosition(10.0f, 20.0f, 30.0f, 0.0f);

    e.startMoveTo(10.0f, 20.0f, 30.0f, 1.25f, 0.5f);

    REQUIRE_FALSE(e.isEntityMoving());
    REQUIRE_FALSE(e.isActivelyMoving());
    REQUIRE(e.getOrientation() == Catch::Approx(1.25f));
}

TEST_CASE("Entity still interpolates meaningful movement", "[entity][movement]") {
    Entity e;
    e.setPosition(0.0f, 0.0f, 0.0f, 0.0f);

    e.startMoveTo(5.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE(e.isActivelyMoving());

    e.updateMovement(0.5f);
    REQUIRE(e.getX() == Catch::Approx(2.5f));
    REQUIRE(e.isActivelyMoving());
}

TEST_CASE("NPC spline walk hints are temporary and do not change movement mode", "[entity][movement][npc]") {
    Entity e;
    e.startMoveTo(10.0f, 0.0f, 0.0f, 0.0f, 4.0f);
    e.setSplineWalkingHint(true);
    CHECK(e.walkingForAnimation(false));
    e.updateMovement(4.0f);
    CHECK_FALSE(e.walkingForAnimation(false));

    e.startMoveTo(20.0f, 0.0f, 0.0f, 0.0f, 2.0f);
    CHECK_FALSE(e.walkingForAnimation(false));
    CHECK(e.walkingForAnimation(true));
    e.setSplineWalkingHint(false);
    CHECK_FALSE(e.walkingForAnimation(true));
    e.clearSplineWalkingHint();
    CHECK(e.walkingForAnimation(true));
    e.setSplineWalkingHint(true);
    e.setPosition(20.0f, 0.0f, 0.0f, 0.0f);
    e.startMoveTo(21.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    CHECK_FALSE(e.walkingForAnimation(false));
}

TEST_CASE("NPC ground spline speed is compared with that creature's speeds", "[entity][movement][npc]") {
    REQUIRE(npcSplineWalkingHint(10.0f, 4000, 2.5f, 7.0f) == true);
    REQUIRE(npcSplineWalkingHint(10.0f, 1429, 2.5f, 7.0f) == false);
    // A custom faster creature: an absolute 4.5-yard threshold would be wrong.
    REQUIRE(npcSplineWalkingHint(10.0f, 2000, 5.0f, 14.0f) == true);
    REQUIRE(npcSplineWalkingHint(10.0f, 3636, 2.5f, 7.0f) == true); // 10% path error
    REQUIRE(npcSplineWalkingHint(10.0f, 1299, 2.5f, 7.0f) == false);
    // Custom spline velocities and overlapping speed ranges are ambiguous.
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 10000, 2.5f, 7.0f).has_value());
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 2105, 2.5f, 7.0f).has_value());
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 500, 2.5f, 7.0f).has_value());
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 1818, 5.0f, 6.0f).has_value());
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 0, 2.5f, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(0.0f, 4000, 2.5f, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 4000, 0.0f, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 4000, 7.0f, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(10.0f, 4000, NAN, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(INFINITY, 4000, 2.5f, 7.0f));
    CHECK_FALSE(npcSplineWalkingHint(1000.0f, 1, 2.5f, 7.0f));
}

TEST_CASE("NPC flying and swimming animations take priority over walk hints", "[entity][movement][npc]") {
    using wowee::core::movingCreatureAnimation;
    using namespace wowee::rendering::anim;
    CHECK(movingCreatureAnimation(false, false, true) == WALK);
    CHECK(movingCreatureAnimation(false, false, false) == RUN);
    CHECK(movingCreatureAnimation(false, true, true) == SWIM);
    CHECK(movingCreatureAnimation(true, true, true) == FLY_FORWARD);
}

TEST_CASE("NPCs use the available ground animation when one is missing", "[entity][movement][npc]") {
    using wowee::core::supportedCreatureAnimation;
    using namespace wowee::rendering::anim;
    const auto onlyRun = [](uint32_t id) { return id == RUN; };
    const auto onlyWalk = [](uint32_t id) { return id == WALK; };
    const auto neither = [](uint32_t) { return false; };
    CHECK(supportedCreatureAnimation(WALK, onlyRun) == RUN);
    CHECK(supportedCreatureAnimation(RUN, onlyWalk) == WALK);
    CHECK(supportedCreatureAnimation(WALK, onlyWalk) == WALK);
    CHECK(supportedCreatureAnimation(RUN, onlyRun) == RUN);
    CHECK(supportedCreatureAnimation(WALK, neither) == WALK);
    CHECK(supportedCreatureAnimation(SWIM, onlyRun) == SWIM);
}

TEST_CASE("Entity field set/get/has", "[entity]") {
    Entity e;
    REQUIRE_FALSE(e.hasField(10));

    e.setField(10, 0xCAFE);
    REQUIRE(e.hasField(10));
    REQUIRE(e.getField(10) == 0xCAFE);

    // Overwrite
    e.setField(10, 0xBEEF);
    REQUIRE(e.getField(10) == 0xBEEF);

    // Non-existent returns 0
    REQUIRE(e.getField(999) == 0);
}

TEST_CASE("Unit construction and type", "[entity]") {
    Unit u;
    REQUIRE(u.getType() == ObjectType::UNIT);

    Unit u2(0x123);
    REQUIRE(u2.getGuid() == 0x123);
    REQUIRE(u2.getType() == ObjectType::UNIT);
}

TEST_CASE("Unit name", "[entity]") {
    Unit u;
    REQUIRE(u.getName().empty());
    u.setName("Hogger");
    REQUIRE(u.getName() == "Hogger");
}

TEST_CASE("Unit health", "[entity]") {
    Unit u;
    REQUIRE(u.getHealth() == 0);
    REQUIRE(u.getMaxHealth() == 0);

    u.setHealth(500);
    u.setMaxHealth(1000);
    REQUIRE(u.getHealth() == 500);
    REQUIRE(u.getMaxHealth() == 1000);
}

TEST_CASE("Unit power by type", "[entity]") {
    Unit u;
    u.setPowerType(0); // mana
    u.setPower(200);
    u.setMaxPower(500);

    REQUIRE(u.getPower() == 200);
    REQUIRE(u.getMaxPower() == 500);
    REQUIRE(u.getPowerByType(0) == 200);
    REQUIRE(u.getMaxPowerByType(0) == 500);

    // Set rage (type 1)
    u.setPowerByType(1, 50);
    u.setMaxPowerByType(1, 100);
    REQUIRE(u.getPowerByType(1) == 50);
    REQUIRE(u.getMaxPowerByType(1) == 100);

    // Out of bounds clamps
    REQUIRE(u.getPowerByType(7) == 0);
    REQUIRE(u.getMaxPowerByType(7) == 0);
}

TEST_CASE("Unit level, entry, displayId", "[entity]") {
    Unit u;
    REQUIRE(u.getLevel() == 1); // default
    u.setLevel(80);
    REQUIRE(u.getLevel() == 80);

    u.setEntry(1234);
    REQUIRE(u.getEntry() == 1234);

    u.setDisplayId(5678);
    REQUIRE(u.getDisplayId() == 5678);
}

TEST_CASE("Unit flags", "[entity]") {
    Unit u;
    u.setUnitFlags(0x01);
    REQUIRE(u.getUnitFlags() == 0x01);

    REQUIRE_FALSE(u.hasCreepVisibility());
    u.setVisibilityFlags(UNIT_VIS_FLAG_CREEP);
    REQUIRE(u.getVisibilityFlags() == UNIT_VIS_FLAG_CREEP);
    REQUIRE(u.hasCreepVisibility());
    u.clearCreepVisibility();
    REQUIRE_FALSE(u.hasCreepVisibility());
    REQUIRE((u.getVisibilityFlags() & UNIT_VIS_FLAG_CREEP) == 0);

    u.setDynamicFlags(0x02);
    REQUIRE(u.getDynamicFlags() == 0x02);

    u.setNpcFlags(0x04);
    REQUIRE(u.getNpcFlags() == 0x04);
    REQUIRE(u.isInteractable());

    u.setNpcFlags(0);
    REQUIRE_FALSE(u.isInteractable());
}

TEST_CASE("Corpse state uses the real dynamic dead bit", "[entity][corpse]") {
    REQUIRE(isUnitCorpseState(0, 100, 0));
    REQUIRE(isUnitCorpseState(0, 0, UNIT_DYNFLAG_DEAD));
    REQUIRE(isUnitCorpseState(0, 0, UNIT_DYNFLAG_LOOTABLE));

    REQUIRE_FALSE(isUnitCorpseState(100, 100, 0));
    // Regression: 0x08 is tapped-by-player, not dead.
    REQUIRE_FALSE(isUnitCorpseState(100, 100, UNIT_DYNFLAG_TAPPED_BY_PLAYER));
    REQUIRE(UNIT_DYNFLAG_DEAD == 0x00000020);
}

TEST_CASE("Unit faction and hostility", "[entity]") {
    Unit u;
    u.setFactionTemplate(14); // Undercity
    REQUIRE(u.getFactionTemplate() == 14);

    REQUIRE_FALSE(u.isHostile());
    u.setHostile(true);
    REQUIRE(u.isHostile());
}

TEST_CASE("Unit mount display ID", "[entity]") {
    Unit u;
    REQUIRE(u.getMountDisplayId() == 0);
    u.setMountDisplayId(14374);
    REQUIRE(u.getMountDisplayId() == 14374);
}

TEST_CASE("Player inherits Unit", "[entity]") {
    Player p(0xABC);
    REQUIRE(p.getType() == ObjectType::PLAYER);
    REQUIRE(p.getGuid() == 0xABC);

    // Player inherits Unit name - regression test for the shadowed-field fix
    p.setName("Arthas");
    REQUIRE(p.getName() == "Arthas");

    p.setLevel(80);
    REQUIRE(p.getLevel() == 80);
}

TEST_CASE("GameObject construction", "[entity]") {
    GameObject go(0x999);
    REQUIRE(go.getType() == ObjectType::GAMEOBJECT);
    REQUIRE(go.getGuid() == 0x999);

    go.setName("Mailbox");
    REQUIRE(go.getName() == "Mailbox");

    go.setEntry(42);
    REQUIRE(go.getEntry() == 42);

    go.setDisplayId(100);
    REQUIRE(go.getDisplayId() == 100);
}

TEST_CASE("EntityManager add/get/has/remove", "[entity]") {
    EntityManager mgr;
    REQUIRE(mgr.getEntityCount() == 0);

    auto unit = std::make_shared<Unit>(1);
    unit->setName("TestUnit");
    mgr.addEntity(1, unit);

    REQUIRE(mgr.getEntityCount() == 1);
    REQUIRE(mgr.hasEntity(1));
    REQUIRE_FALSE(mgr.hasEntity(2));

    auto retrieved = mgr.getEntity(1);
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->getGuid() == 1);

    mgr.removeEntity(1);
    REQUIRE_FALSE(mgr.hasEntity(1));
    REQUIRE(mgr.getEntityCount() == 0);
}

TEST_CASE("EntityManager clear", "[entity]") {
    EntityManager mgr;
    mgr.addEntity(1, std::make_shared<Entity>(1));
    mgr.addEntity(2, std::make_shared<Entity>(2));
    REQUIRE(mgr.getEntityCount() == 2);

    mgr.clear();
    REQUIRE(mgr.getEntityCount() == 0);
}

TEST_CASE("EntityManager null entity rejected", "[entity]") {
    EntityManager mgr;
    mgr.addEntity(1, nullptr);
    // Null should be rejected (logged warning, not stored)
    REQUIRE(mgr.getEntityCount() == 0);
}

TEST_CASE("EntityManager getEntities returns all", "[entity]") {
    EntityManager mgr;
    mgr.addEntity(10, std::make_shared<Unit>(10));
    mgr.addEntity(20, std::make_shared<Player>(20));
    mgr.addEntity(30, std::make_shared<GameObject>(30));

    const auto& all = mgr.getEntities();
    REQUIRE(all.size() == 3);
    REQUIRE(all.count(10) == 1);
    REQUIRE(all.count(20) == 1);
    REQUIRE(all.count(30) == 1);
}

TEST_CASE("EntityManager spatial query filters nearby entities", "[entity][spatial]") {
    EntityManager mgr;
    auto origin = std::make_shared<Unit>(1);
    origin->setPosition(-10.0f, -10.0f, 0.0f, 0.0f);
    auto nearby = std::make_shared<Unit>(2);
    nearby->setPosition(20.0f, 15.0f, 0.0f, 0.0f);
    auto distant = std::make_shared<Unit>(3);
    distant->setPosition(500.0f, 500.0f, 0.0f, 0.0f);
    mgr.addEntity(1, origin);
    mgr.addEntity(2, nearby);
    mgr.addEntity(3, distant);

    const auto result = mgr.getEntitiesNear(0.0f, 0.0f, 50.0f);
    REQUIRE(result.size() == 2);
    REQUIRE(std::any_of(result.begin(), result.end(), [](const auto& e) { return e->getGuid() == 1; }));
    REQUIRE(std::any_of(result.begin(), result.end(), [](const auto& e) { return e->getGuid() == 2; }));
    REQUIRE_FALSE(std::any_of(result.begin(), result.end(), [](const auto& e) { return e->getGuid() == 3; }));
}
