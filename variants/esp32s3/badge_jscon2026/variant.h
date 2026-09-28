#ifndef _VARIANT_BADGE_JSCON2026_H_
#define _VARIANT_BADGE_JSCON2026_H_

// Badge JSCON 2026 - Electronic Cats
// ESP32-S3-WROOM-1-N8: 8 MB flash, no PSRAM.
// Pinout verified against the KiCad netlist at
// Documents/electronicCats/Hw/badge_JSCON_2026/hardware/

// -- LoRa: VGdd79S915N0S1 915 MHz module (Semtech LLCC68) --------------------
// Confirmed against the VGdd79SxxxN0S1 hardware spec V1.2, section 1 (Overview)
// and section 6 (Programming notes): this module is built on an LLCC68 die, NOT
// an SX1262. Its driver is SX1262/SX1268-compatible, which is why the SX126X_*
// pin defines below still apply, but the LLCC68 has narrower LoRa limits.
//
// LLCC68 LoRa limits, from spec section 6:
//   BW 125 kHz -> SF 5 to 9
//   BW 250 kHz -> SF 5 to 10
//   BW 500 kHz -> SF 5 to 11
// SF12 is unsupported at any bandwidth. Meshtastic's LongSlow and VLongSlow
// presets need SF12 and fail with Critical Error #3 (see LLCC68Interface.h).
// LongFast (BW 250, SF 11) also exceeds the BW-250 ceiling of SF10 in this
// spec. Choose a preset that fits the table above.
#define USE_LLCC68

#define LORA_SCK 37  // IO37
#define LORA_MISO 36 // IO36
#define LORA_MOSI 35 // IO35
#define LORA_CS 38   // IO38 = NSS

#define LORA_RESET 47 // IO47 = RST
#define LORA_DIO1 48  // IO48 = DIO1 (IRQ)
#define LORA_DIO2 39  // IO39 = BUSY

#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_DIO2
#define SX126X_RESET LORA_RESET

// DIO2 drives the module's internal antenna switch. Confirmed by the spec's
// pin table (section 4, DIO2 row: "directly drives the module's internal
// antenna switch, just call SetDio2AsRfSwitchCtrl()") and by section 6, which
// shows SX126xSetDio2AsRfSwitchCtrl(true) as required. DIO2 is internal to the
// module and is not brought out to a pad, which matches the board netlist.
#define SX126X_DIO2_AS_RF_SWITCH

// NO TCXO. The spec's parameter table (section 2) lists the 32 MHz reference as
// "wu yuan jing zhen" - a passive crystal, not a temperature-compensated
// oscillator - and section 6 wraps the SetDio3AsTcxoCtrl() call in #ifdef
// USE_TCXO. Module pin 8 (DIO3) is left unconnected on this board, confirmed in
// the netlist as unconnected-(U5-DIO3-Pad8).
//
// SX126X_DIO3_TCXO_VOLTAGE is therefore deliberately NOT defined. Defining it
// would make the driver drive DIO3 as a TCXO supply and wait for a TCXO that
// does not exist, which is the classic "radio inits then does not work"
// failure. Do not add it back.

// -- Display: ER-TFT2.79-1, NV3007 controller, on HSPI ------------------------
// USE_ARDUINO_GFX selects the Arduino_GFX-typed `tft` member and code paths
// in TFTDisplay.cpp/.h (as opposed to the LGFX fallback). Not called out
// explicitly in the task brief, but required: hackaday-communicator, which
// shares the same NV3007 connect() code path, sets this too.
#define USE_ARDUINO_GFX 1
#define TFT_MOSI 11 // IO11 = SDA
#define TFT_SCLK 12 // IO12 = SCL
#define TFT_DC 9    // IO09 = RS  (data/command)
#define TFT_RST 10  // IO10 = display reset
#define TFT_CS 21   // IO21 = chip select
#define TFT_MISO -1 // not connected
#define TFT_BL 2    // IO02 = backlight, PWM through Q2

#define TFT_HEIGHT 428 // physical rows
#define TFT_WIDTH 142  // physical columns
#define TFT_OFFSET_X1 12
#define TFT_OFFSET_Y1 0
#define TFT_OFFSET_X2 14
#define TFT_OFFSET_Y2 0
#define SCREEN_ROTATE // swap logical dimensions for the Meshtastic UI
#define TFT_ROTATION 1
#define SCREEN_TRANSITION_FRAMERATE 5
#define HAS_SCREEN 1
#define TFT_BLACK 0
#define BRIGHTNESS_DEFAULT 254
#define USE_TFTDISPLAY 1
// REQUIRED. main.cpp only constructs graphics::Screen unconditionally when
// HAS_SPI_TFT (or USE_EINK / USE_SPISSD1306) is defined; otherwise it waits for
// an I2C-detected display, finds none on this board, and the screen is never
// created at all - no TFT init ever runs and the panel stays dark with no error
// in the log. hackaday-communicator sets this for the same reason.
#define HAS_SPI_TFT 1

// -- Branding -----------------------------------------------------------------
// JSConf Mexico 2026 logo as the only boot screen, and the brand colors for the
// default theme (see the BADGE_JSCON2026 entry in TFTColorRegions.cpp): the
// kit's navy background with a darker header bar.
#include "jsconf_branding.h"
#define TFT_HEADER_BG_COLOR_OVERRIDE JSCONF_COLOR_DEEP

// -- Buttons ------------------------------------------------------------------
// All four are switch-to-GND with a debounce cap and no external pull
// resistors, so they use internal pullups. Verified on hardware with a
// standalone GPIO diagnostic: every button gives exactly one clean edge per
// press, no bounce, no float (IO43 included).
//
// Behaviour, all on SHORT press, no long-press:
//   IO40 and IO43 move between pages, and up / down inside menus
//   IO18 selects
//   IO01 goes back
//
// BadgeJsconButtons (src/input/BadgeJsconButtons.cpp) drives all four with a
// polled, debounced reader. The upstream trackball driver is built for a
// rotating trackball: on plain pushbuttons it dropped taps, doubled them
// (measured 100-160 ms apart with TB_THRESHOLD 1) or auto-repeated.
//
// HAS_TRACKBALL is 0 so that driver does not claim the pins, and BUTTON_PIN is
// deliberately NOT defined so ButtonThread does not claim IO40. The TB_*
// defines are kept only as the pin source for BadgeJsconButtons.
#define HAS_TRACKBALL 0
#define TB_UP 40     // IO40 - previous page / up
#define TB_DOWN 43   // IO43 - next page / down
#define TB_LEFT 1    // IO01 - back
#define TB_RIGHT 255 // unused
#define TB_PRESS 18  // IO18 - select
#define TB_DIRECTION FALLING

// -- Absent hardware ----------------------------------------------------------
// No GPS, no battery, no I2C sensors, no secondary radio.
#define GPS_DEFAULT_NOT_PRESENT 1
// Compiles the GPS driver and the GPS page out of the UI.
#define HAS_GPS 0
// No I2C bus is wired on this board. This disables Meshtastic's own I2C
// scanning/usage in main.cpp; it does not affect the SDA/SCL macros needed
// below purely to let the Arduino Wire library compile.
#define HAS_WIRE 0

#endif // _VARIANT_BADGE_JSCON2026_H_
