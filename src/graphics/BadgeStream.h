#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include <stddef.h>
#include <stdint.h>

namespace graphics
{
/// AdminMessage.send_input_event codes that start and stop the live screen stream.
constexpr uint32_t kBadgeStreamStartEvent = 0xe1;
constexpr uint32_t kBadgeStreamStopEvent = 0xe2;
/// Unknown FromRadio field that carries one stream frame (official clients skip it).
constexpr uint32_t kBadgeStreamField = 99;

void badgeStreamStart();
void badgeStreamStop();
bool badgeStreamActive();
/// True while the next display() must repaint the whole frame (stream just started or a frame was dropped).
bool badgeStreamNeedsKeyframe();
/// Called by TFTDisplay after pixels are pushed: `startIndex` is y * width + x in raster order, big-endian RGB565.
void badgeStreamPixels(uint32_t startIndex, const uint16_t *pixelsBe, size_t count);
/// Called when a full repaint finished, so a keyframe request is satisfied.
void badgeStreamKeyframeDone();
/// Serial thread: copy the next encoded payload (FromRadio bytes) to `out`, 0 if none. `out` holds >= 520 bytes.
size_t badgeStreamPop(uint8_t *out);
bool badgeStreamHasData();
} // namespace graphics

#endif
