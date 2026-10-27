#include "BadgeStream.h"

#ifdef BADGE_JSCON2026

#include "SerialConsole.h"
#include "concurrency/LockGuard.h"
#include "graphics/Screen.h"

namespace graphics
{
namespace
{
// Frame payload: u32 start index (BE), then runs of [count-1 u8][value u16 BE]. The payload is wrapped as FromRadio
// field 99 so it is valid protobuf for any client.
constexpr size_t kMaxPayload = 240;
constexpr size_t kRingSize = 32 * 1024;

uint8_t ring[kRingSize];
size_t head = 0, tail = 0, used = 0; // frames are stored as [len16][bytes]
volatile bool active = false;
volatile bool needKey = false;
concurrency::Lock ringLock;

void ringPush(const uint8_t *data, size_t len)
{
    // Meshtastic threads are cooperative, so the serial thread cannot run while display() waits for room: drain the queue
    // from here. A photo (little to compress) is far bigger than the ring.
    for (int tries = 0; tries < 400 && used + len + 2 > kRingSize; tries++) {
        if (!console)
            break;
        if (!console->pumpScreenStream())
            delay(1);
    }
    concurrency::LockGuard guard(&ringLock);
    if (used + len + 2 > kRingSize) {
        needKey = true; // the host is not reading: the client's picture is stale, resend everything later
        return;
    }
    auto put = [&](uint8_t b) {
        ring[head] = b;
        head = (head + 1) % kRingSize;
    };
    put(len >> 8);
    put(len & 0xff);
    for (size_t i = 0; i < len; i++)
        put(data[i]);
    used += len + 2;
}
} // namespace

void badgeStreamStart()
{
    {
        concurrency::LockGuard guard(&ringLock);
        head = tail = used = 0;
    }
    needKey = true;
    active = true;
    if (screen)
        screen->runNow();
}

void badgeStreamStop()
{
    active = false;
    needKey = false;
}

bool badgeStreamActive()
{
    return active;
}

bool badgeStreamNeedsKeyframe()
{
    return active && needKey;
}

void badgeStreamKeyframeDone()
{
    needKey = false;
}

void badgeStreamPixels(uint32_t startIndex, const uint16_t *px, size_t count)
{
    if (!active || count == 0)
        return;
    uint8_t body[kMaxPayload];
    size_t i = 0;
    while (i < count) {
        size_t n = 4;
        const uint32_t idx = startIndex + i;
        body[0] = idx >> 24;
        body[1] = idx >> 16;
        body[2] = idx >> 8;
        body[3] = idx;
        while (i < count && n + 3 <= kMaxPayload) {
            const uint16_t v = px[i];
            size_t run = 1;
            while (i + run < count && run < 256 && px[i + run] == v)
                run++;
            body[n++] = run - 1;
            body[n++] = v >> 8;
            body[n++] = v & 0xff;
            i += run;
        }
        // FromRadio field 99, wire type 2: tag 0x9a 0x06, then a varint length.
        uint8_t msg[kMaxPayload + 8];
        size_t m = 0;
        msg[m++] = 0x9a;
        msg[m++] = 0x06;
        if (n < 128) {
            msg[m++] = n;
        } else {
            msg[m++] = 0x80 | (n & 0x7f);
            msg[m++] = n >> 7;
        }
        memcpy(msg + m, body, n);
        ringPush(msg, m + n);
    }
    if (console)
        console->wakeForStream();
}

bool badgeStreamHasData()
{
    return used > 0;
}

size_t badgeStreamPop(uint8_t *out)
{
    concurrency::LockGuard guard(&ringLock);
    if (used == 0)
        return 0;
    auto get = [&]() {
        const uint8_t b = ring[tail];
        tail = (tail + 1) % kRingSize;
        return b;
    };
    const size_t len = (get() << 8) | get();
    for (size_t i = 0; i < len; i++)
        out[i] = get();
    used -= len + 2;
    return len;
}
} // namespace graphics

#endif
