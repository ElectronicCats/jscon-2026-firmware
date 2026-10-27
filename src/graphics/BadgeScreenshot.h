#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include <stddef.h>
#include <stdint.h>

namespace graphics
{
/// AdminMessage.send_input_event code that asks for one screenshot.
constexpr uint32_t kBadgeScreenshotEvent = 0xe0;
/// The frame is written here as big-endian RGB565, row-major, and downloaded over XModem.
constexpr const char *kBadgeScreenshotPath = "/prefs/screen.bin";

/// Ask the next TFTDisplay::display() to repaint the whole frame and save it.
void requestBadgeScreenshot();
bool badgeScreenshotPending();

/// Called by TFTDisplay around a full repaint. Rows are streamed to flash, so no frame buffer is needed.
void badgeScreenshotBegin();
void badgeScreenshotWrite(const uint16_t *pixelsBe, size_t pixelCount);
void badgeScreenshotEnd();
} // namespace graphics

#endif
