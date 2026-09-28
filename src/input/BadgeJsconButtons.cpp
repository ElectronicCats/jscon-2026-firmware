#include "BadgeJsconButtons.h"

#ifdef BADGE_JSCON2026

#include "InputBroker.h"

BadgeJsconButtons *badgeJsconButtons;

BadgeJsconButtons::BadgeJsconButtons() : concurrency::OSThread("BadgeJsconButtons") {}

void BadgeJsconButtons::init()
{
    // Pin to event mapping for this badge:
    //   IO40 and IO43 navigate, IO18 selects, IO01 goes back.
    // CANCEL is used for back rather than LEFT or BACK because the Meshtastic
    // UI handles CANCEL as "back" in far more places.
    // ALT_PRESS / USER_PRESS act as LEFT / RIGHT between frames and as UP / DOWN
    // inside menus and pickers; plain UP / DOWN never switch frames.
    buttons[0] = {TB_UP, INPUT_BROKER_ALT_PRESS, false, false, 0};
    buttons[1] = {TB_DOWN, INPUT_BROKER_USER_PRESS, false, false, 0};
    buttons[2] = {TB_PRESS, INPUT_BROKER_SELECT, false, false, 0};
    buttons[3] = {TB_LEFT, INPUT_BROKER_CANCEL, false, false, 0};

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        pinMode(buttons[i].gpio, INPUT_PULLUP);
    }
    delay(10);

    // Seed the accepted state from the current pin level so that a button held
    // down at boot does not immediately register as a fresh press.
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        bool low = (digitalRead(buttons[i].gpio) == LOW);
        buttons[i].stableLow = low;
        buttons[i].candidateLow = low;
        buttons[i].agreeCount = DEBOUNCE_POLLS;
    }

    initialised = true;
    LOG_INFO("BadgeJsconButtons: UP=%d DOWN=%d SELECT=%d BACK=%d", buttons[0].gpio, buttons[1].gpio, buttons[2].gpio,
             buttons[3].gpio);

    inputBroker->registerSource(this);
    setInterval(POLL_MS);
}

int32_t BadgeJsconButtons::runOnce()
{
    if (!initialised)
        return POLL_MS;

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        Button &b = buttons[i];
        bool low = (digitalRead(b.gpio) == LOW);

        if (low != b.candidateLow) {
            // Reading changed; start confirming the new value.
            b.candidateLow = low;
            b.agreeCount = 1;
            continue;
        }

        if (b.agreeCount < DEBOUNCE_POLLS) {
            b.agreeCount++;
            if (b.agreeCount < DEBOUNCE_POLLS)
                continue;

            // The candidate value has now been stable long enough to accept.
            if (b.candidateLow != b.stableLow) {
                b.stableLow = b.candidateLow;
                if (b.stableLow) {
                    // Accepted press. One event, on the press edge only.
                    InputEvent e = {};
                    e.source = "BadgeJsconButtons";
                    e.inputEvent = b.event;
                    e.kbchar = 0x00;
#ifdef INPUT_DEBUG
                    LOG_DEBUG("BadgeJsconButtons: GPIO %d -> event %d", b.gpio, (int)b.event);
#endif
                    notifyObservers(&e);
                }
            }
        }
    }

    return POLL_MS;
}

#endif // BADGE_JSCON2026
