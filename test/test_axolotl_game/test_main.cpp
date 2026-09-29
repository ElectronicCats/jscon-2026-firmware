// Deliberately does NOT include TestUtil.h. This suite is pure-function - no NodeDB, no router, no
// filesystem - so the harness-wide guards there would assert conditions it cannot reach.
#include "modules/badge/AxolotlGame.h"
#include <Arduino.h> // declares setup() and loop() with the linkage the framework expects
#include <cstdlib>
#include <cstring>
#include <unity.h>

using namespace axolotl;

static const uint32_t kSelf = 0x11111111;
static const uint32_t kPeer = 0x22222222;
static const uint32_t kOther = 0x33333333;

void setUp(void) {}
void tearDown(void) {}

// --- Own axolotl -------------------------------------------------------------

void test_home_isStableAndInRange()
{
    for (uint32_t id = 1; id < 5000; id += 7) {
        const uint8_t home = homeFromNodeId(id);
        TEST_ASSERT_TRUE(home < kCount);
        TEST_ASSERT_EQUAL_UINT8(home, homeFromNodeId(id));
    }
}

void test_home_rarityShares()
{
    uint32_t perTier[3] = {0, 0, 0};
    bool seen[kCount] = {};
    const uint32_t samples = 20000;
    for (uint32_t i = 0; i < samples; i++) {
        const uint8_t home = homeFromNodeId(0xA5000000u + i * 2654435761u);
        seen[home] = true;
        perTier[static_cast<uint8_t>(rarityOf(home))]++;
    }
    for (uint8_t i = 0; i < kCount; i++)
        TEST_ASSERT_TRUE_MESSAGE(seen[i], "every axolotl must be reachable");
    // 77 / 20 / 3 percent, with slack for the hash.
    TEST_ASSERT_UINT32_WITHIN(samples * 3 / 100, samples * 77 / 100, perTier[static_cast<uint8_t>(Rarity::Common)]);
    TEST_ASSERT_UINT32_WITHIN(samples * 3 / 100, samples * 20 / 100, perTier[static_cast<uint8_t>(Rarity::Rare)]);
    TEST_ASSERT_UINT32_WITHIN(samples * 1 / 100, samples * 3 / 100, perTier[static_cast<uint8_t>(Rarity::Legendary)]);
}

void test_rarity_tierSizes()
{
    uint8_t perTier[3] = {0, 0, 0};
    for (uint8_t i = 0; i < kCount; i++)
        perTier[static_cast<uint8_t>(rarityOf(i))]++;
    TEST_ASSERT_EQUAL_UINT8(7, perTier[static_cast<uint8_t>(Rarity::Common)]);
    TEST_ASSERT_EQUAL_UINT8(3, perTier[static_cast<uint8_t>(Rarity::Rare)]);
    TEST_ASSERT_EQUAL_UINT8(1, perTier[static_cast<uint8_t>(Rarity::Legendary)]);
}

// --- Collection --------------------------------------------------------------

void test_newGame_ownsOnlyHome()
{
    Game game;
    game.begin(kSelf);
    TEST_ASSERT_EQUAL_UINT8(homeFromNodeId(kSelf), game.home());
    TEST_ASSERT_TRUE(game.has(game.home()));
    TEST_ASSERT_EQUAL_UINT8(1, game.collectedCount());
    TEST_ASSERT_EQUAL_UINT16(0, game.peopleMet());
    TEST_ASSERT_FALSE(game.isComplete());
}

void test_seedBadge_hasFixedHome()
{
    for (uint8_t forced = 0; forced < kCount; forced++) {
        Game game;
        game.begin(kSelf, forced);
        TEST_ASSERT_EQUAL_UINT8(forced, game.home());
        TEST_ASSERT_TRUE(game.has(forced));
        TEST_ASSERT_EQUAL_UINT8(1, game.collectedCount());
        TEST_ASSERT_EQUAL_UINT8(forced, game.makeHello(false).home);
    }
    Game game;
    game.begin(kSelf, kCount); // out of range means "not forced"
    TEST_ASSERT_EQUAL_UINT8(homeFromNodeId(kSelf), game.home());
}

static uint8_t otherThan(uint8_t home)
{
    return (home + 1) % kCount;
}

void test_oneStrongReading_doesNotUnlock()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    const Outcome o = game.onHello(kPeer, theirs, kNearRssi, true, 1000);
    TEST_ASSERT_FALSE(o.unlocked);
    TEST_ASSERT_FALSE(game.has(theirs));
}

void test_twoStrongReadings_unlock()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 1000);
    const Outcome o = game.onHello(kPeer, theirs, -70, true, 1000 + kConfirmWindowMs);
    TEST_ASSERT_TRUE(o.unlocked);
    TEST_ASSERT_TRUE(o.newPerson);
    TEST_ASSERT_EQUAL_UINT8(theirs, o.axolotl);
    TEST_ASSERT_TRUE(game.has(theirs));
    TEST_ASSERT_EQUAL_UINT8(2, game.collectedCount());
    TEST_ASSERT_EQUAL_UINT16(1, game.peopleMet());
    TEST_ASSERT_TRUE(game.isDirty());
}

void test_secondReadingTooLate_startsOver()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 1000);
    const Outcome o = game.onHello(kPeer, theirs, -60, true, 1000 + kConfirmWindowMs + 1);
    TEST_ASSERT_FALSE(o.unlocked);
    TEST_ASSERT_FALSE(game.has(theirs));
}

void test_weakSignal_isRadarOnly()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, kNearRssi - 1, true, 1000);
    const Outcome o = game.onHello(kPeer, theirs, kNearRssi - 1, true, 2000);
    TEST_ASSERT_FALSE(o.unlocked);
    TEST_ASSERT_FALSE(game.has(theirs));
    TEST_ASSERT_EQUAL_UINT8(1, game.sightingCount(2000));
}

void test_relayedOrInvalid_isIgnored()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -40, false, 1000); // relayed
    game.onHello(kPeer, theirs, -40, false, 2000);
    game.onHello(kPeer, kCount, -40, true, 3000); // axolotl out of range
    game.onHello(kPeer, kCount, -40, true, 4000);
    game.onHello(kSelf, theirs, -40, true, 5000); // our own packet
    game.onHello(kSelf, theirs, -40, true, 6000);
    TEST_ASSERT_EQUAL_UINT8(1, game.collectedCount());
    TEST_ASSERT_EQUAL_UINT16(0, game.peopleMet());
    TEST_ASSERT_EQUAL_UINT8(0, game.sightingCount(6000));
}

void test_samePersonTwice_countsOnce()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 1000);
    game.onHello(kPeer, theirs, -60, true, 2000);
    game.clearDirty();
    game.onHello(kPeer, theirs, -60, true, 100000);
    const Outcome o = game.onHello(kPeer, theirs, -60, true, 101000);
    TEST_ASSERT_FALSE(o.unlocked);
    TEST_ASSERT_FALSE(o.newPerson);
    TEST_ASSERT_EQUAL_UINT16(1, game.peopleMet());
    TEST_ASSERT_FALSE(game.isDirty());
}

void test_sameAxolotlNewPerson_countsPersonOnly()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 1000);
    game.onHello(kPeer, theirs, -60, true, 2000);
    game.onHello(kOther, theirs, -60, true, 3000);
    const Outcome o = game.onHello(kOther, theirs, -60, true, 4000);
    TEST_ASSERT_FALSE(o.unlocked);
    TEST_ASSERT_TRUE(o.newPerson);
    TEST_ASSERT_EQUAL_UINT8(2, game.collectedCount());
    TEST_ASSERT_EQUAL_UINT16(2, game.peopleMet());
}

void test_clockWrap_stillConfirms()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 0xFFFFFF00u);
    const Outcome o = game.onHello(kPeer, theirs, -60, true, 0x00000100u);
    TEST_ASSERT_TRUE(o.unlocked);
}

void test_collectingAll_completes()
{
    Game game;
    game.begin(kSelf);
    uint32_t now = 1000;
    for (uint8_t i = 0; i < kCount; i++) {
        game.onHello(0x40000000u + i, i, -60, true, now);
        game.onHello(0x40000000u + i, i, -60, true, now + 1000);
        now += 5000;
    }
    TEST_ASSERT_EQUAL_UINT8(kCount, game.collectedCount());
    TEST_ASSERT_TRUE(game.isComplete());
}

// --- Radar -------------------------------------------------------------------

void test_radar_entriesExpire()
{
    Game game;
    game.begin(kSelf);
    game.onHello(kPeer, otherThan(game.home()), -95, true, 1000);
    TEST_ASSERT_EQUAL_UINT8(1, game.sightingCount(1000 + kSightingLifeMs));
    TEST_ASSERT_EQUAL_UINT8(0, game.sightingCount(1000 + kSightingLifeMs + 1));
}

void test_radar_listsStrongestFirst()
{
    Game game;
    game.begin(kSelf);
    game.onHello(kPeer, 1, -95, true, 1000);
    game.onHello(kOther, 2, -85, true, 1100);
    game.onHello(kPeer, 1, -82, true, 1200); // same badge walked closer
    Sighting list[kSightingCapacity];
    const uint8_t n = game.sightings(1300, list, kSightingCapacity);
    TEST_ASSERT_EQUAL_UINT8(2, n);
    TEST_ASSERT_EQUAL_UINT32(kPeer, list[0].nodeId);
    TEST_ASSERT_EQUAL_INT32(-82, list[0].rssi);
    TEST_ASSERT_EQUAL_UINT32(kOther, list[1].nodeId);
}

// --- Save and load -----------------------------------------------------------

void test_saveLoad_roundTrip()
{
    Game game;
    game.begin(kSelf);
    const uint8_t theirs = otherThan(game.home());
    game.onHello(kPeer, theirs, -60, true, 1000);
    game.onHello(kPeer, theirs, -60, true, 2000);

    uint8_t blob[kSaveSize];
    TEST_ASSERT_EQUAL_size_t(kSaveSize, game.save(blob, sizeof(blob)));

    Game restored;
    restored.begin(kSelf);
    TEST_ASSERT_TRUE(restored.load(blob, sizeof(blob)));
    TEST_ASSERT_TRUE(restored.has(theirs));
    TEST_ASSERT_EQUAL_UINT8(2, restored.collectedCount());
    TEST_ASSERT_EQUAL_UINT16(1, restored.peopleMet());
    TEST_ASSERT_FALSE(restored.isDirty());

    // The person is remembered, so meeting again is not a new person.
    restored.onHello(kPeer, theirs, -60, true, 1000);
    const Outcome o = restored.onHello(kPeer, theirs, -60, true, 2000);
    TEST_ASSERT_FALSE(o.newPerson);
}

void test_load_rejectsDamage()
{
    Game game;
    game.begin(kSelf);
    uint8_t blob[kSaveSize];
    game.save(blob, sizeof(blob));

    Game restored;
    restored.begin(kSelf);
    TEST_ASSERT_FALSE(restored.load(blob, kSaveSize - 1)); // truncated
    blob[5] ^= 0x40;
    TEST_ASSERT_FALSE(restored.load(blob, sizeof(blob))); // corrupted
    TEST_ASSERT_EQUAL_UINT8(1, restored.collectedCount());
}

void test_load_alwaysKeepsHome()
{
    // A save from another badge must not take away this badge's own axolotl.
    Game other;
    other.begin(kPeer);
    uint8_t blob[kSaveSize];
    other.save(blob, sizeof(blob));

    Game game;
    game.begin(kSelf);
    TEST_ASSERT_TRUE(game.load(blob, sizeof(blob)));
    TEST_ASSERT_EQUAL_UINT8(homeFromNodeId(kSelf), game.home());
    TEST_ASSERT_TRUE(game.has(game.home()));
}

// --- Wire format -------------------------------------------------------------

void test_hello_roundTrip()
{
    Hello out;
    out.isAnswer = true;
    out.home = 7;
    out.collected = 0x0285;
    uint8_t wire[kHelloSize];
    TEST_ASSERT_EQUAL_size_t(kHelloSize, encodeHello(out, wire, sizeof(wire)));

    Hello in;
    TEST_ASSERT_TRUE(decodeHello(wire, sizeof(wire), in));
    TEST_ASSERT_TRUE(in.isAnswer);
    TEST_ASSERT_EQUAL_UINT8(7, in.home);
    TEST_ASSERT_EQUAL_UINT16(0x0285, in.collected);
}

void test_hello_rejectsForeignPayloads()
{
    Hello in;
    const uint8_t text[] = {'h', 'e', 'l', 'l', 'o', '!', '!'};
    TEST_ASSERT_FALSE(decodeHello(text, sizeof(text), in));

    Hello out;
    out.home = 3;
    uint8_t wire[kHelloSize];
    encodeHello(out, wire, sizeof(wire));
    TEST_ASSERT_FALSE(decodeHello(wire, kHelloSize - 1, in)); // short
    wire[2] = 99;
    TEST_ASSERT_FALSE(decodeHello(wire, sizeof(wire), in)); // unknown version
    encodeHello(out, wire, sizeof(wire));
    wire[4] = kCount;
    TEST_ASSERT_FALSE(decodeHello(wire, sizeof(wire), in)); // axolotl out of range
}

void test_shouldAnswer_onlyWhenItHelps()
{
    Game game;
    game.begin(kSelf);
    Hello hello;
    hello.home = otherThan(game.home());

    hello.collected = 0; // they lack ours
    TEST_ASSERT_TRUE(game.shouldAnswer(hello, kNearRssi, true));
    TEST_ASSERT_FALSE(game.shouldAnswer(hello, kNearRssi - 1, true)); // too far to matter
    TEST_ASSERT_FALSE(game.shouldAnswer(hello, kNearRssi, false));    // relayed

    hello.collected = static_cast<uint16_t>(1u << game.home()); // they already have ours
    TEST_ASSERT_FALSE(game.shouldAnswer(hello, kNearRssi, true));

    hello.isAnswer = true; // never answer an answer
    hello.collected = 0;
    TEST_ASSERT_FALSE(game.shouldAnswer(hello, kNearRssi, true));
}

void setup()
{
    UNITY_BEGIN();
    RUN_TEST(test_home_isStableAndInRange);
    RUN_TEST(test_home_rarityShares);
    RUN_TEST(test_rarity_tierSizes);
    RUN_TEST(test_newGame_ownsOnlyHome);
    RUN_TEST(test_seedBadge_hasFixedHome);
    RUN_TEST(test_oneStrongReading_doesNotUnlock);
    RUN_TEST(test_twoStrongReadings_unlock);
    RUN_TEST(test_secondReadingTooLate_startsOver);
    RUN_TEST(test_weakSignal_isRadarOnly);
    RUN_TEST(test_relayedOrInvalid_isIgnored);
    RUN_TEST(test_samePersonTwice_countsOnce);
    RUN_TEST(test_sameAxolotlNewPerson_countsPersonOnly);
    RUN_TEST(test_clockWrap_stillConfirms);
    RUN_TEST(test_collectingAll_completes);
    RUN_TEST(test_radar_entriesExpire);
    RUN_TEST(test_radar_listsStrongestFirst);
    RUN_TEST(test_saveLoad_roundTrip);
    RUN_TEST(test_load_rejectsDamage);
    RUN_TEST(test_load_alwaysKeepsHome);
    RUN_TEST(test_hello_roundTrip);
    RUN_TEST(test_hello_rejectsForeignPayloads);
    RUN_TEST(test_shouldAnswer_onlyWhenItHelps);
    exit(UNITY_END());
}

void loop() {}
