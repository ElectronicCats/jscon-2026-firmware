#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Rules of the Badge JSCON 2026 axolotl collecting game. Pure logic: no radio, no screen, no
// filesystem, no clock of its own, so it runs unchanged in the native test suite.
namespace axolotl
{

// The kit ships 13 axolotl files, but two pairs are the same drawing (costume = fan, faq = bubbles),
// so there are 11 to collect.
constexpr uint8_t kCount = 11;
constexpr int32_t kNearRssi = -80;           // at or above this, a badge counts as "near"
constexpr uint32_t kConfirmWindowMs = 30000; // two near readings this close together unlock
constexpr uint32_t kSightingLifeMs = 90000;  // how long a badge stays on the radar
constexpr uint8_t kSightingCapacity = 8;
constexpr uint8_t kCandidateCapacity = 8;
constexpr uint8_t kMetCapacity = 128;
constexpr uint16_t kAllCollected = (1u << kCount) - 1;

// Indexes follow the sprite table order in jsconf_sprites.h.
enum Id : uint8_t { Agent, Bubbles, Corn, Fan, Finance, Happy, Poncho, Referee, Scout, Venue, Vibes };

enum class Rarity : uint8_t { Common, Rare, Legendary };

constexpr uint8_t kLegendaryPercent = 3;
constexpr uint8_t kRarePercent = 20;
constexpr uint8_t kRare[] = {Finance, Referee, Scout};
constexpr uint8_t kCommon[] = {Bubbles, Corn, Fan, Happy, Poncho, Venue, Vibes};

inline Rarity rarityOf(uint8_t id)
{
    if (id == Agent)
        return Rarity::Legendary;
    for (uint8_t rare : kRare)
        if (rare == id)
            return Rarity::Rare;
    return Rarity::Common;
}

// The axolotl a badge is born with. Derived from an id that never changes for that badge (its MAC
// address), so it survives a factory reset and cannot be re-rolled.
inline uint8_t homeFromNodeId(uint32_t nodeId)
{
    uint32_t h = nodeId;
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    const uint32_t roll = h % 100;
    const uint32_t pick = h / 100;
    if (roll < kLegendaryPercent)
        return Agent;
    if (roll < kLegendaryPercent + kRarePercent)
        return kRare[pick % sizeof(kRare)];
    return kCommon[pick % sizeof(kCommon)];
}

// --- Wire format ---------------------------------------------------------------------------

struct Hello {
    bool isAnswer = false;
    uint8_t home = 0;
    uint16_t collected = 0;
};

constexpr size_t kHelloSize = 7;
constexpr uint8_t kHelloVersion = 2;

inline size_t encodeHello(const Hello &hello, uint8_t *out, size_t outSize)
{
    if (outSize < kHelloSize)
        return 0;
    out[0] = 'A';
    out[1] = 'J';
    out[2] = kHelloVersion;
    out[3] = hello.isAnswer ? 1 : 0;
    out[4] = hello.home;
    out[5] = hello.collected & 0xFF;
    out[6] = hello.collected >> 8;
    return kHelloSize;
}

inline bool decodeHello(const uint8_t *in, size_t inSize, Hello &hello)
{
    if (inSize != kHelloSize || in[0] != 'A' || in[1] != 'J' || in[2] != kHelloVersion || in[4] >= kCount)
        return false;
    hello.isAnswer = (in[3] & 1) != 0;
    hello.home = in[4];
    hello.collected = static_cast<uint16_t>(in[5] | (in[6] << 8)) & kAllCollected;
    return true;
}

// --- Game state ----------------------------------------------------------------------------

struct Outcome {
    bool unlocked = false;  // a new axolotl joined the collection
    bool newPerson = false; // first confirmed meeting with this badge
    uint8_t axolotl = 0;    // the other badge's axolotl, valid when either flag is set
};

struct Sighting {
    uint32_t nodeId;
    uint32_t seenAtMs;
    int32_t rssi;
    uint8_t home;
};

// magic(2) version(1) collected(2) peopleMet(2) metUsed(1) metNext(1) met(4 * kMetCapacity) checksum(2)
constexpr size_t kSaveSize = 9 + 4 * kMetCapacity + 2;
constexpr uint8_t kSaveVersion = 2;

class Game
{
  public:
    // forcedHome pins the axolotl of a seed badge; leave it at kCount to derive it from the id.
    void begin(uint32_t selfNodeId, uint8_t forcedHome = kCount)
    {
        self = selfNodeId;
        homeId = forcedHome < kCount ? forcedHome : homeFromNodeId(selfNodeId);
        collected = static_cast<uint16_t>(1u << homeId);
        met = 0;
        metUsed = 0;
        metNext = 0;
        candidateCount = 0;
        sightingUsed = 0;
        dirty = false;
    }

    uint8_t home() const { return homeId; }
    bool has(uint8_t id) const { return id < kCount && (collected & (1u << id)) != 0; }
    uint16_t collectedMask() const { return collected; }
    uint8_t collectedCount() const
    {
        uint8_t n = 0;
        for (uint8_t i = 0; i < kCount; i++)
            n += has(i) ? 1 : 0;
        return n;
    }
    bool isComplete() const { return collected == kAllCollected; }
    uint16_t peopleMet() const { return met; }

    // Bringup aid: shows every axolotl as collected without touching what gets saved.
    void previewAllCollected() { collected = kAllCollected; }

    bool isDirty() const { return dirty; }
    void clearDirty() { dirty = false; }

    // Feed every hello or answer heard. direct is false for anything that was relayed.
    Outcome onHello(uint32_t nodeId, uint8_t theirHome, int32_t rssi, bool direct, uint32_t nowMs)
    {
        Outcome outcome;
        if (!direct || theirHome >= kCount || nodeId == self || nodeId == 0)
            return outcome;

        noteSighting(nodeId, theirHome, rssi, nowMs);
        if (rssi < kNearRssi)
            return outcome;

        const int8_t pending = findCandidate(nodeId);
        if (pending < 0 || static_cast<uint32_t>(nowMs - candidates[pending].atMs) > kConfirmWindowMs) {
            rememberCandidate(nodeId, nowMs, pending);
            return outcome;
        }

        candidates[pending] = candidates[--candidateCount];
        outcome.axolotl = theirHome;
        if (!hasMet(nodeId)) {
            rememberMet(nodeId);
            outcome.newPerson = true;
            dirty = true;
        }
        if (!has(theirHome)) {
            collected |= static_cast<uint16_t>(1u << theirHome);
            outcome.unlocked = true;
            dirty = true;
        }
        return outcome;
    }

    // Answer a hello only when the sender is near and still lacks our axolotl.
    bool shouldAnswer(const Hello &hello, int32_t rssi, bool direct) const
    {
        return direct && !hello.isAnswer && rssi >= kNearRssi && (hello.collected & (1u << homeId)) == 0;
    }

    Hello makeHello(bool isAnswer) const
    {
        Hello hello;
        hello.isAnswer = isAnswer;
        hello.home = homeId;
        hello.collected = collected;
        return hello;
    }

    uint8_t sightingCount(uint32_t nowMs) const
    {
        uint8_t n = 0;
        for (uint8_t i = 0; i < sightingUsed; i++)
            n += isFresh(sightingTable[i], nowMs) ? 1 : 0;
        return n;
    }

    // Badges heard recently, strongest signal first.
    uint8_t sightings(uint32_t nowMs, Sighting *out, uint8_t outCapacity) const
    {
        uint8_t n = 0;
        for (uint8_t i = 0; i < sightingUsed && n < outCapacity; i++) {
            if (!isFresh(sightingTable[i], nowMs))
                continue;
            uint8_t slot = n++;
            while (slot > 0 && out[slot - 1].rssi < sightingTable[i].rssi) {
                out[slot] = out[slot - 1];
                slot--;
            }
            out[slot] = sightingTable[i];
        }
        return n;
    }

    size_t save(uint8_t *out, size_t outSize) const
    {
        if (outSize < kSaveSize)
            return 0;
        memset(out, 0, kSaveSize);
        out[0] = 'A';
        out[1] = 'J';
        out[2] = kSaveVersion;
        out[3] = collected & 0xFF;
        out[4] = collected >> 8;
        out[5] = met & 0xFF;
        out[6] = met >> 8;
        out[7] = metUsed;
        out[8] = metNext;
        for (uint8_t i = 0; i < metUsed; i++)
            putU32(out + 9 + 4 * i, metRing[i]);
        const uint16_t sum = checksum(out, kSaveSize - 2);
        out[kSaveSize - 2] = sum & 0xFF;
        out[kSaveSize - 1] = sum >> 8;
        return kSaveSize;
    }

    // Call after begin(). Leaves the state untouched when the data is damaged.
    bool load(const uint8_t *in, size_t inSize)
    {
        if (inSize != kSaveSize || in[0] != 'A' || in[1] != 'J' || in[2] != kSaveVersion)
            return false;
        const uint16_t sum = static_cast<uint16_t>(in[kSaveSize - 2] | (in[kSaveSize - 1] << 8));
        if (sum != checksum(in, kSaveSize - 2) || in[7] > kMetCapacity || in[8] >= kMetCapacity)
            return false;

        collected = (static_cast<uint16_t>(in[3] | (in[4] << 8)) & kAllCollected) | static_cast<uint16_t>(1u << homeId);
        met = static_cast<uint16_t>(in[5] | (in[6] << 8));
        metUsed = in[7];
        metNext = in[8];
        for (uint8_t i = 0; i < metUsed; i++)
            metRing[i] = getU32(in + 9 + 4 * i);
        dirty = false;
        return true;
    }

  private:
    struct Candidate {
        uint32_t nodeId;
        uint32_t atMs;
    };

    static bool isFresh(const Sighting &s, uint32_t nowMs)
    {
        return static_cast<uint32_t>(nowMs - s.seenAtMs) <= kSightingLifeMs;
    }

    static void putU32(uint8_t *out, uint32_t v)
    {
        out[0] = v & 0xFF;
        out[1] = (v >> 8) & 0xFF;
        out[2] = (v >> 16) & 0xFF;
        out[3] = (v >> 24) & 0xFF;
    }

    static uint32_t getU32(const uint8_t *in)
    {
        return static_cast<uint32_t>(in[0]) | (static_cast<uint32_t>(in[1]) << 8) | (static_cast<uint32_t>(in[2]) << 16) |
               (static_cast<uint32_t>(in[3]) << 24);
    }

    // Fletcher-16
    static uint16_t checksum(const uint8_t *data, size_t size)
    {
        uint16_t a = 0, b = 0;
        for (size_t i = 0; i < size; i++) {
            a = (a + data[i]) % 255;
            b = (b + a) % 255;
        }
        return static_cast<uint16_t>((b << 8) | a);
    }

    int8_t findCandidate(uint32_t nodeId) const
    {
        for (uint8_t i = 0; i < candidateCount; i++)
            if (candidates[i].nodeId == nodeId)
                return i;
        return -1;
    }

    void rememberCandidate(uint32_t nodeId, uint32_t nowMs, int8_t existing)
    {
        uint8_t slot;
        if (existing >= 0) {
            slot = existing;
        } else if (candidateCount < kCandidateCapacity) {
            slot = candidateCount++;
        } else {
            slot = 0; // full: drop the longest waiting one
            for (uint8_t i = 1; i < candidateCount; i++)
                if (static_cast<uint32_t>(nowMs - candidates[i].atMs) > static_cast<uint32_t>(nowMs - candidates[slot].atMs))
                    slot = i;
        }
        candidates[slot] = {nodeId, nowMs};
    }

    bool hasMet(uint32_t nodeId) const
    {
        for (uint8_t i = 0; i < metUsed; i++)
            if (metRing[i] == nodeId)
                return true;
        return false;
    }

    void rememberMet(uint32_t nodeId)
    {
        metRing[metNext] = nodeId;
        metNext = (metNext + 1) % kMetCapacity;
        if (metUsed < kMetCapacity)
            metUsed++;
        met++;
    }

    void noteSighting(uint32_t nodeId, uint8_t theirHome, int32_t rssi, uint32_t nowMs)
    {
        uint8_t slot = sightingUsed;
        for (uint8_t i = 0; i < sightingUsed; i++) {
            if (sightingTable[i].nodeId == nodeId) {
                slot = i;
                break;
            }
        }
        if (slot == sightingUsed) {
            if (sightingUsed < kSightingCapacity) {
                sightingUsed++;
            } else {
                // Full: reuse an expired entry, else the weakest, and only for a stronger signal.
                slot = 0;
                for (uint8_t i = 0; i < sightingUsed; i++) {
                    if (!isFresh(sightingTable[i], nowMs)) {
                        slot = i;
                        break;
                    }
                    if (sightingTable[i].rssi < sightingTable[slot].rssi)
                        slot = i;
                }
                if (isFresh(sightingTable[slot], nowMs) && sightingTable[slot].rssi >= rssi)
                    return;
            }
        }
        sightingTable[slot] = {nodeId, nowMs, rssi, theirHome};
    }

    uint32_t self = 0;
    uint8_t homeId = 0;
    uint16_t collected = 0;
    uint16_t met = 0;
    uint8_t metUsed = 0;
    uint8_t metNext = 0;
    uint32_t metRing[kMetCapacity] = {};
    Candidate candidates[kCandidateCapacity] = {};
    uint8_t candidateCount = 0;
    Sighting sightingTable[kSightingCapacity] = {};
    uint8_t sightingUsed = 0;
    bool dirty = false;
};

} // namespace axolotl
