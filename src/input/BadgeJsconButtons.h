#pragma once

#include "configuration.h"

#ifdef BADGE_JSCON2026

#include "InputBroker.h"
#include "concurrency/OSThread.h"

/**
 * Four discrete pushbuttons for the Badge JSCON 2026.
 *
 * Upstream's only driver for several direction pins is TrackballInterruptBase,
 * which is written for a rotating trackball: it counts pulses, requires the pin
 * to still be low at the next poll, and auto-repeats while a direction is held.
 * On plain pushbuttons that produces dropped taps, doubled taps, or runaway
 * repeats depending on how it is tuned. This badge wants something much simpler.
 *
 * Behaviour here, deliberately minimal:
 *   - poll all four pins every POLL_MS
 *   - a pin must read the same value for DEBOUNCE_POLLS consecutive polls
 *     before that value is accepted
 *   - exactly one event is emitted on each accepted high-to-low transition
 *   - no auto-repeat, no long-press, no chording
 *
 * Buttons are switch-to-GND with a debounce capacitor and no external pull
 * resistors, so internal pullups are used and pressed reads low.
 */
class BadgeJsconButtons : public Observable<const InputEvent *>, public concurrency::OSThread
{
  public:
    BadgeJsconButtons();
    void init();

  protected:
    int32_t runOnce() override;

  private:
    static constexpr uint32_t POLL_MS = 10;
    static constexpr uint8_t DEBOUNCE_POLLS = 3; // 30 ms stable before a change is accepted
    static constexpr uint8_t NUM_BUTTONS = 4;

    struct Button {
        uint8_t gpio;
        input_broker_event event;
        bool stableLow;    // last accepted state
        bool candidateLow; // value currently being confirmed
        uint8_t agreeCount;
    };

    Button buttons[NUM_BUTTONS];
    bool initialised = false;
};

extern BadgeJsconButtons *badgeJsconButtons;

#endif // BADGE_JSCON2026
