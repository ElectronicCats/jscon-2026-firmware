#include "BadgeCanvasModule.h"

#ifdef BADGE_JSCON2026

#include "FSCommon.h"
#include "SPILock.h"
#include "Throttle.h"
#include "graphics/ScreenFonts.h"
#include "graphics/SharedUIDisplay.h"
#include "xmodem.h"

BadgeCanvasModule *badgeCanvasModule;

namespace
{
constexpr int32_t kPollMs = 250;
constexpr uint32_t kProbeIntervalMs = 2000;
constexpr int16_t kBodyTop = 22;
} // namespace

BadgeCanvasModule::BadgeCanvasModule() : concurrency::OSThread("BadgeCanvas")
{
    // Logical size: the panel is mounted rotated (SCREEN_ROTATE).
    sprite.width = TFT_HEIGHT;
    sprite.height = TFT_WIDTH;
    sprite.pixelsBe = nullptr;
    sprite.filePath = kCanvasPath;
}

void BadgeCanvasModule::probe()
{
    lastProbeMs = millis();
#ifdef FSCom
    const size_t expected = (size_t)sprite.width * sprite.height * 2;
    concurrency::LockGuard g(spiLock);
    File f = FSCom.open(kCanvasPath, FILE_O_READ);
    ready = f && f.size() == expected;
    if (f)
        f.close();
#endif
}

int32_t BadgeCanvasModule::runOnce()
{
    // XModem removes the target before it sets busy, so a canvas upload reads as not ready until it ends.
    const bool busy = xModem.isBusy();
    if (busy != receiving) {
        receiving = busy;
        if (!busy)
            revision++;
        probe();
    } else if (!busy && Throttle::hasElapsed(lastProbeMs, kProbeIntervalMs)) {
        probe();
    }
    return kPollMs;
}

void BadgeCanvasModule::drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
    display->clear();
    if (ready) {
        // Sprites only paint over background pixels, so the 1-bit frame stays empty.
        graphics::clearBadgeSprites();
        graphics::queueBadgeSprite(sprite, 0, 0, graphics::kBadgeSpriteNoTint, revision);
        return;
    }

    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->setFont(FONT_SMALL);
    graphics::drawCommonHeader(display, x, y, "Canvas");
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    const int16_t centerX = x + SCREEN_WIDTH / 2;
    const int16_t lineY = y + kBodyTop + 24;
    if (receiving) {
        display->drawString(centerX, lineY, "Receiving image over USB...");
    } else {
        display->drawString(centerX, lineY, "No image yet");
        display->drawString(centerX, lineY + FONT_HEIGHT_SMALL, "Upload a 428x142 image over USB");
    }
    display->setTextAlignment(TEXT_ALIGN_LEFT);
}

#endif // BADGE_JSCON2026
