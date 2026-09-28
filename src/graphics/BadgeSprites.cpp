#include "BadgeSprites.h"

#ifdef BADGE_JSCON2026

#include "Throttle.h"
#include "jsconf_sprites.h"
#include <Arduino.h>

namespace graphics
{

const BadgeSprite badgeBackgroundData = {JSCONF_BACKGROUND_W, JSCONF_BACKGROUND_H, jsconfBackgroundData};
const BadgeSprite badgeBackgroundName = {JSCONF_BACKGROUND_W, JSCONF_BACKGROUND_H, jsconfBackgroundName};

namespace
{
const BadgeSprite axolotlsLarge[JSCONF_AXOLOTL_COUNT] = JSCONF_AXOLOTLS_LARGE;
const BadgeSprite axolotlsSmall[JSCONF_AXOLOTL_COUNT] = JSCONF_AXOLOTLS_SMALL;
const BadgeSprite axolotlsIcon[JSCONF_AXOLOTL_COUNT] = JSCONF_AXOLOTLS_ICON;

constexpr uint32_t kAxolotlIntervalMs = 5000;
uint8_t axolotlOrder[JSCONF_AXOLOTL_COUNT];
uint8_t axolotlPosition = JSCONF_AXOLOTL_COUNT; // past the end: shuffle on first use
uint32_t axolotlShownAtMs = 0;

void shuffleAxolotls()
{
    const uint8_t previous = (axolotlPosition > 0 && axolotlPosition <= JSCONF_AXOLOTL_COUNT) ? axolotlOrder[axolotlPosition - 1]
                                                                                              : JSCONF_AXOLOTL_COUNT;
    for (uint8_t i = 0; i < JSCONF_AXOLOTL_COUNT; i++)
        axolotlOrder[i] = i;
    for (uint8_t i = JSCONF_AXOLOTL_COUNT - 1; i > 0; i--) {
        const uint8_t j = random(i + 1);
        const uint8_t swap = axolotlOrder[i];
        axolotlOrder[i] = axolotlOrder[j];
        axolotlOrder[j] = swap;
    }
    // Do not open a round with the axolotl that closed the previous one.
    if (axolotlOrder[0] == previous) {
        axolotlOrder[0] = axolotlOrder[JSCONF_AXOLOTL_COUNT - 1];
        axolotlOrder[JSCONF_AXOLOTL_COUNT - 1] = previous;
    }
}

void advanceAxolotl()
{
    if (++axolotlPosition >= JSCONF_AXOLOTL_COUNT) {
        shuffleAxolotls();
        axolotlPosition = 0;
    }
}

struct QueuedSprite {
    const BadgeSprite *sprite;
    int16_t x;
    int16_t y;
    uint16_t tintBe;
};

constexpr uint8_t kMaxQueuedSprites = 20; // background + every slot of the collection grid + spare
QueuedSprite queued[kMaxQueuedSprites];
uint8_t queuedCount = 0;
} // namespace

const BadgeSprite &axolotlSprite(uint8_t id, AxolotlSize size)
{
    if (id >= JSCONF_AXOLOTL_COUNT)
        id = 0;
    switch (size) {
    case AxolotlSize::Large:
        return axolotlsLarge[id];
    case AxolotlSize::Small:
        return axolotlsSmall[id];
    default:
        return axolotlsIcon[id];
    }
}

uint8_t currentBadgeAxolotl(uint16_t allowedMask)
{
    if (axolotlPosition >= JSCONF_AXOLOTL_COUNT) {
        advanceAxolotl();
        axolotlShownAtMs = millis();
    } else if (Throttle::hasElapsed(axolotlShownAtMs, kAxolotlIntervalMs)) {
        axolotlShownAtMs = millis();
        advanceAxolotl();
    }
    // Skip the ones that are not allowed; a full lap without a match means the mask is empty.
    for (uint8_t tries = 0; tries < JSCONF_AXOLOTL_COUNT; tries++) {
        if (allowedMask & (1u << axolotlOrder[axolotlPosition]))
            break;
        advanceAxolotl();
    }
    return axolotlOrder[axolotlPosition];
}

void queueBadgeSprite(const BadgeSprite &sprite, int16_t x, int16_t y, uint16_t tint)
{
    if (queuedCount >= kMaxQueuedSprites)
        return;
    const uint16_t tintBe =
        (tint == kBadgeSpriteNoTint) ? kBadgeSpriteTransparent : static_cast<uint16_t>((tint >> 8) | ((tint & 0xFF) << 8));
    queued[queuedCount++] = {&sprite, x, y, tintBe};
}

uint32_t getBadgeSpriteSignature()
{
    uint32_t hash = 2166136261u;
    for (uint8_t i = 0; i < queuedCount; i++) {
        const uint32_t parts[] = {(uint32_t)(uintptr_t)queued[i].sprite, (uint32_t)(uint16_t)queued[i].x,
                                  (uint32_t)(uint16_t)queued[i].y, (uint32_t)queued[i].tintBe};
        for (uint32_t part : parts)
            hash = (hash ^ part) * 16777619u;
    }
    return hash;
}

void clearBadgeSprites()
{
    queuedCount = 0;
}

void overlayBadgeSpritesRow(int16_t y, int16_t xStart, int16_t xEnd, uint16_t *row, uint16_t backgroundBe)
{
    // Topmost sprite first: a pixel it paints stops matching backgroundBe, so lower layers skip it.
    for (int8_t i = queuedCount - 1; i >= 0; i--) {
        const QueuedSprite &q = queued[i];
        const int32_t spriteRow = y - q.y;
        if (spriteRow < 0 || spriteRow >= q.sprite->height)
            continue;

        const int32_t first = (xStart > q.x) ? xStart : q.x;
        const int32_t last = (xEnd < q.x + q.sprite->width - 1) ? xEnd : (q.x + q.sprite->width - 1);
        const uint16_t *src = q.sprite->pixelsBe + spriteRow * q.sprite->width;
        const bool tinted = q.tintBe != kBadgeSpriteTransparent;
        for (int32_t x = first; x <= last; x++) {
            const uint16_t pixel = src[x - q.x];
            if (pixel != kBadgeSpriteTransparent && row[x] == backgroundBe)
                row[x] = tinted ? q.tintBe : pixel;
        }
    }
}

} // namespace graphics

#endif // BADGE_JSCON2026
