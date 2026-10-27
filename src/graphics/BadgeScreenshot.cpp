#include "BadgeScreenshot.h"

#ifdef BADGE_JSCON2026

#include "FSCommon.h"
#include "SPILock.h"
#include "graphics/Screen.h"

namespace graphics
{
namespace
{
volatile bool pending = false;
bool capturing = false;
File shot;
} // namespace

void requestBadgeScreenshot()
{
    LOG_INFO("Screenshot requested");
    pending = true;
    if (screen)
        screen->runNow();
}

bool badgeScreenshotPending()
{
    return pending;
}

void badgeScreenshotBegin()
{
#ifdef FSCom
    // TFTDisplay::display() already holds spiLock around these calls.
    if (FSCom.exists(kBadgeScreenshotPath))
        FSCom.remove(kBadgeScreenshotPath);
    shot = FSCom.open(kBadgeScreenshotPath, FILE_O_WRITE);
    capturing = shot;
#endif
}

void badgeScreenshotWrite(const uint16_t *pixelsBe, size_t pixelCount)
{
#ifdef FSCom
    if (!capturing)
        return;
    if (shot.write(reinterpret_cast<const uint8_t *>(pixelsBe), pixelCount * 2) != pixelCount * 2) {
        LOG_WARN("Screenshot: short write, dropping capture");
        shot.close();
        capturing = false;
    }
#endif
}

void badgeScreenshotEnd()
{
#ifdef FSCom
    if (capturing) {
        shot.close();
        LOG_INFO("Screenshot saved to %s", kBadgeScreenshotPath);
    }
#endif
    capturing = false;
    pending = false;
}
} // namespace graphics

#endif
