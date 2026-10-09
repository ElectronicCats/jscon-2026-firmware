#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include <stdint.h>

namespace graphics
{

// Full-color artwork for the Badge JSCON 2026. The UI framebuffer is 1 bit, so sprites are
// queued by the frame code and composited in TFTDisplay::display(), behind everything else.
struct BadgeSprite {
    uint16_t width;
    uint16_t height;
    const uint16_t *pixelsBe; // RGB565, byte-swapped; kBadgeSpriteTransparent = see-through
    // Streamed a row at a time when pixelsBe is null; RGB565 big-endian, opaque.
    const char *filePath = nullptr;
};

static constexpr uint16_t kBadgeSpriteTransparent = 0x1FF8;
static constexpr uint16_t kBadgeSpriteNoTint = kBadgeSpriteTransparent;

enum class AxolotlSize : uint8_t { Large, Small, Icon };

// Full-screen backgrounds cut from the kit's blue pattern: one for data pages, one with a name card.
extern const BadgeSprite badgeBackgroundData;
extern const BadgeSprite badgeBackgroundName;

// id follows axolotl::Id.
const BadgeSprite &axolotlSprite(uint8_t id, AxolotlSize size);

// The axolotl on show right now, picked from the ids set in allowedMask. One at a time, each
// allowed axolotl once per round, in a random order that is reshuffled each round.
uint8_t currentBadgeAxolotl(uint16_t allowedMask);

// Queue a sprite for the frame being drawn; later sprites are drawn over earlier ones.
// With a tint, every opaque pixel takes that RGB565 color, which turns a sprite into a silhouette.
// The queue is emptied after each display(), so `sprite` must outlive the frame.
// A new `revision` forces a repaint of a file-backed sprite whose file changed.
void queueBadgeSprite(const BadgeSprite &sprite, int16_t x, int16_t y, uint16_t tint = kBadgeSpriteNoTint, uint32_t revision = 0);
uint32_t getBadgeSpriteSignature();
void clearBadgeSprites();

// Paint queued sprites into row[xStart..xEnd] (indexed by screen x) of screen row y,
// only where the pixel still holds backgroundBe.
void overlayBadgeSpritesRow(int16_t y, int16_t xStart, int16_t xEnd, uint16_t *row, uint16_t backgroundBe);

} // namespace graphics

#endif // BADGE_JSCON2026
