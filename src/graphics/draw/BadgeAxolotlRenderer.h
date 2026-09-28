#pragma once

#include "configuration.h"

#if defined(BADGE_JSCON2026) && HAS_SCREEN

#include "graphics/BadgeSprites.h"
#include "input/InputBroker.h"
#include <OLEDDisplay.h>
#include <OLEDDisplayUi.h>

namespace graphics
{
namespace BadgeAxolotlRenderer
{

// Collection grid: every axolotl, in color once collected, as a silhouette until then.
void drawCollectionFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

// Badges heard recently, strongest first, with a hot/cold bar.
void drawRadarFrame(OLEDDisplay *display, OLEDDisplayUiState *state, int16_t x, int16_t y);

// Tell the renderer which frame slot the collection page got, so it can claim input there.
void setCollectionFrameIndex(uint8_t frameIndex);

// SELECT on the collection page opens a close-up of one axolotl; there, the two navigation
// buttons step through all of them and SELECT or CANCEL go back to the grid.
// Returns true when the event was used.
bool handleInput(const InputEvent *event, uint8_t currentFrame);

// Queue the small axolotl that decorates the bottom right corner of data pages. It rotates
// through the axolotls collected so far.
void queueCornerAxolotl(OLEDDisplay *display, int16_t x, int16_t y);

} // namespace BadgeAxolotlRenderer
} // namespace graphics

#endif
