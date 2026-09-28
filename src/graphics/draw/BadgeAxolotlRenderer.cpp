#include "BadgeAxolotlRenderer.h"

#if defined(BADGE_JSCON2026) && HAS_SCREEN

#include "graphics/ScreenFonts.h"
#include "graphics/SharedUIDisplay.h"
#include "graphics/TFTColorRegions.h"
#include "graphics/TFTPalette.h"
#include "jsconf_font.h"
#include "modules/badge/AxolotlModule.h"

namespace graphics
{
namespace BadgeAxolotlRenderer
{

namespace
{
constexpr int16_t kBodyTop = 22; // below the common header
constexpr int16_t kCellW = 71;
constexpr int16_t kCellH = 60;
constexpr uint8_t kTopRowCells = 6;
constexpr uint8_t kRadarRows = 2;
constexpr int32_t kBarFloorRssi = -110; // empty bar
constexpr int32_t kBarCeilRssi = -40;   // full bar

uint8_t collectionFrameIndex = 255;
bool closeUp = false;
uint8_t closeUpId = 0;

const char *rarityName(uint8_t id)
{
    switch (axolotl::rarityOf(id)) {
    case axolotl::Rarity::Legendary:
        return "Legendario";
    case axolotl::Rarity::Rare:
        return "Raro";
    default:
        return "Comun";
    }
}

void queueCentered(const BadgeSprite &sprite, int16_t cellX, int16_t cellY, uint16_t tint = kBadgeSpriteNoTint)
{
    queueBadgeSprite(sprite, cellX + (kCellW - sprite.width) / 2, cellY + (kCellH - sprite.height) / 2, tint);
}

void tintText(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
#if GRAPHICS_TFT_COLORING_ENABLED
    // Off pixels keep the body color so the background and the sprites show through.
    registerTFTColorRegionDirect(x, y, w, h, color, getThemeBodyBg());
#endif
}
} // namespace

void queueCornerAxolotl(OLEDDisplay *display, int16_t x, int16_t y)
{
    const uint16_t mask = axolotlModule ? axolotlModule->game().collectedMask() : axolotl::kAllCollected;
    const BadgeSprite &sprite = axolotlSprite(currentBadgeAxolotl(mask), AxolotlSize::Small);
    queueBadgeSprite(sprite, x + SCREEN_WIDTH - sprite.width - 4, y + SCREEN_HEIGHT - sprite.height);
}

void setCollectionFrameIndex(uint8_t frameIndex)
{
    collectionFrameIndex = frameIndex;
}

bool handleInput(const InputEvent *event, uint8_t currentFrame)
{
    if (currentFrame != collectionFrameIndex || !axolotlModule) {
        closeUp = false;
        return false;
    }
    const input_broker_event e = event->inputEvent;
    if (!closeUp) {
        if (e != INPUT_BROKER_SELECT)
            return false;
        closeUp = true;
        closeUpId = axolotlModule->game().home();
        return true;
    }
    if (e == INPUT_BROKER_SELECT || e == INPUT_BROKER_CANCEL || e == INPUT_BROKER_BACK) {
        closeUp = false;
    } else if (e == INPUT_BROKER_ALT_PRESS || e == INPUT_BROKER_UP || e == INPUT_BROKER_LEFT) {
        closeUpId = (closeUpId + axolotl::kCount - 1) % axolotl::kCount;
    } else if (e == INPUT_BROKER_USER_PRESS || e == INPUT_BROKER_DOWN || e == INPUT_BROKER_RIGHT) {
        closeUpId = (closeUpId + 1) % axolotl::kCount;
    }
    return true;
}

static void drawCloseUp(OLEDDisplay *display, int16_t x, int16_t y, const axolotl::Game &game)
{
    const bool have = game.has(closeUpId);
    char title[24];
    snprintf(title, sizeof(title), "Ajolote %u de %u", closeUpId + 1, axolotl::kCount);
    drawCommonHeader(display, x, y, title);

    const BadgeSprite &sprite = axolotlSprite(closeUpId, AxolotlSize::Large);
    const int16_t slotW = 150;
    const int16_t spriteX = x + (slotW - sprite.width) / 2;
    const int16_t spriteY = y + kBodyTop + (SCREEN_HEIGHT - kBodyTop - sprite.height) / 2;
    queueBadgeSprite(sprite, spriteX, spriteY, have ? kBadgeSpriteNoTint : JSCONF_COLOR_DEEP);
    if (!have) {
        display->setFont(FONT_LARGE);
        display->drawString(x + (slotW - display->getStringWidth("?")) / 2, spriteY + (sprite.height - FONT_HEIGHT_LARGE) / 2,
                            "?");
    }

    const int16_t textX = x + slotW + 6;
    const int16_t textW = SCREEN_WIDTH - slotW - 6;
    const char *name = have ? AxolotlModule::nameOf(closeUpId) : "???";
    display->setFont(jsconfFontName);
    const int16_t nameY = y + kBodyTop + 2;
    display->drawString(textX, nameY, name);
    tintText(textX, nameY, textW, JSCONF_FONT_NAME_HEIGHT, JSCONF_COLOR_GOLD);

    display->setFont(FONT_SMALL);
    const int16_t lineY = nameY + JSCONF_FONT_NAME_HEIGHT + 2;
    display->drawString(textX, lineY, rarityName(closeUpId));
    const char *status = closeUpId == game.home() ? "Es el tuyo" : have ? "Ya lo tienes" : "Busca a quien lo tenga";
    display->drawString(textX, lineY + FONT_HEIGHT_SMALL, status);
    display->drawString(textX, lineY + 2 * FONT_HEIGHT_SMALL, "Arriba / Abajo: cambiar");
}

void drawCollectionFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    if (!axolotlModule) {
        drawCommonHeader(display, x, y, "Ajolodex");
        return;
    }
    axolotlModule->requestBoost();
    const axolotl::Game &game = axolotlModule->game();
    if (closeUp) {
        drawCloseUp(display, x, y, game);
        return;
    }

    char title[24];
    snprintf(title, sizeof(title), "Ajolodex %u/%u", game.collectedCount(), axolotl::kCount);
    drawCommonHeader(display, x, y, title);

    for (uint8_t id = 0; id < axolotl::kCount; id++) {
        const bool topRow = id < kTopRowCells;
        const uint8_t column = topRow ? id : id - kTopRowCells;
        const int16_t rowOffset = topRow ? 0 : kCellW / 2; // the shorter second row sits centered
        const int16_t cellX = x + rowOffset + column * kCellW;
        const int16_t cellY = y + kBodyTop + (topRow ? 0 : kCellH);
        const BadgeSprite &sprite = axolotlSprite(id, AxolotlSize::Icon);

        if (game.has(id)) {
            queueCentered(sprite, cellX, cellY);
        } else {
            queueCentered(sprite, cellX, cellY, JSCONF_COLOR_DEEP);
            display->setFont(FONT_MEDIUM);
            display->drawString(cellX + (kCellW - display->getStringWidth("?")) / 2, cellY + (kCellH - FONT_HEIGHT_MEDIUM) / 2,
                                "?");
            display->setFont(FONT_SMALL);
        }

        const bool isOwn = id == game.home();
        const bool isNewest = id == axolotlModule->lastUnlocked();
        if (isOwn || isNewest) {
            display->drawRect(cellX + 1, cellY + 1, kCellW - 2, kCellH - 2);
            display->drawRect(cellX + 2, cellY + 2, kCellW - 4, kCellH - 4);
            tintText(cellX, cellY, kCellW, kCellH, isOwn ? JSCONF_COLOR_GOLD : TFTPalette::White);
        }
    }
}

void drawRadarFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    drawCommonHeader(display, x, y, "Radar");
    if (!axolotlModule)
        return;
    axolotlModule->requestBoost();
    const axolotl::Game &game = axolotlModule->game();

    axolotl::Sighting heard[axolotl::kSightingCapacity];
    const uint8_t count = game.sightings(millis(), heard, axolotl::kSightingCapacity);
    if (count == 0) {
        const char *line1 = "Nadie cerca todavia";
        const char *line2 = "Busca mas ajolotes!";
        display->drawString(x + (SCREEN_WIDTH - display->getStringWidth(line1)) / 2, y + kBodyTop + 30, line1);
        display->drawString(x + (SCREEN_WIDTH - display->getStringWidth(line2)) / 2, y + kBodyTop + 30 + FONT_HEIGHT_SMALL,
                            line2);
        return;
    }

    // Axolotls still missing come first: they are what the player is hunting.
    uint8_t order[axolotl::kSightingCapacity];
    uint8_t ordered = 0;
    for (uint8_t pass = 0; pass < 2; pass++)
        for (uint8_t i = 0; i < count; i++)
            if (game.has(heard[i].home) == (pass == 1))
                order[ordered++] = i;

    for (uint8_t row = 0; row < kRadarRows && row < count; row++) {
        const axolotl::Sighting &s = heard[order[row]];
        const int16_t rowY = y + kBodyTop + row * kCellH;
        const bool have = game.has(s.home);
        const bool near = s.rssi >= axolotl::kNearRssi;
        const BadgeSprite &sprite = axolotlSprite(s.home, AxolotlSize::Icon);
        queueCentered(sprite, x + 2, rowY, have ? kBadgeSpriteNoTint : JSCONF_COLOR_DEEP);

        const int16_t textX = x + kCellW + 10;
        char line[48];
        snprintf(line, sizeof(line), "%s  !%04x", have ? AxolotlModule::nameOf(s.home) : "???",
                 (unsigned int)(s.nodeId & 0xFFFF));
        display->drawString(textX, rowY + 6, line);
        if (!have)
            tintText(textX, rowY + 6, display->getStringWidth(line), FONT_HEIGHT_SMALL, JSCONF_COLOR_GOLD);

        // Hot/cold bar: fuller means closer.
        const int16_t barX = textX;
        const int16_t barY = rowY + 6 + FONT_HEIGHT_SMALL + 6;
        const int16_t barW = 180;
        const int16_t barH = 14;
        int32_t level = (s.rssi - kBarFloorRssi) * (barW - 4) / (kBarCeilRssi - kBarFloorRssi);
        if (level < 0)
            level = 0;
        if (level > barW - 4)
            level = barW - 4;
        display->drawRect(barX, barY, barW, barH);
        display->fillRect(barX + 2, barY + 2, level, barH - 4);
        tintText(barX, barY, barW, barH, near ? JSCONF_COLOR_GOLD : TFTPalette::White);

        snprintf(line, sizeof(line), near ? "%d dBm  CERCA!" : "%d dBm", (int)s.rssi);
        display->drawString(barX + barW + 8, barY - 3, line);
    }
}

} // namespace BadgeAxolotlRenderer
} // namespace graphics

#endif
