#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include "concurrency/OSThread.h"
#include "graphics/BadgeSprites.h"

class OLEDDisplay;
struct OLEDDisplayUiState;

/// Canvas page: full-screen RGB565 big-endian image at kCanvasPath, uploaded over XModem.
class BadgeCanvasModule : private concurrency::OSThread
{
  public:
    static constexpr const char *kCanvasPath = "/prefs/canvas.bin";

    BadgeCanvasModule();

    void drawFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

  protected:
    int32_t runOnce() override;

  private:
    void probe();

    graphics::BadgeSprite sprite;
    uint32_t revision = 0;
    uint32_t lastProbeMs = 0;
    bool ready = false;
    bool receiving = false;
};

extern BadgeCanvasModule *badgeCanvasModule;

#endif // BADGE_JSCON2026
