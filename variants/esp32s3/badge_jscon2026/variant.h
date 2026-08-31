#ifndef _VARIANT_BADGE_JSCON2026_H_
#define _VARIANT_BADGE_JSCON2026_H_

// Badge JSCON 2026 - Electronic Cats
// ESP32-S3-WROOM-1-N8: 8 MB flash, no PSRAM.
// Pinout verified against the KiCad netlist at
// Documents/electronicCats/Hw/badge_JSCON_2026/hardware/

// -- LoRa: VGdd79S915N0S1 915 MHz module (SX126x family) ---------------------
#define USE_SX1262

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

// WARNING: the two settings below are UNVERIFIED for this module. The
// VGdd79S915N0S1 datasheet was not available when this variant was written.
// They are copied from the Tropicon board's SX1262 and must be confirmed
// against the part before any RF result is trusted. See Task 6.
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

// -- Display: ER-TFT2.79-1, NV3007 controller, on HSPI ------------------------
// USE_ARDUINO_GFX selects the Arduino_GFX-typed `tft` member and code paths
// in TFTDisplay.cpp/.h (as opposed to the LGFX fallback). Not called out
// explicitly in the task brief, but required: hackaday-communicator, which
// shares the same NV3007 connect() code path, sets this too.
#define USE_ARDUINO_GFX 1
#define TFT_MOSI 11  // IO11 = SDA
#define TFT_SCLK 12  // IO12 = SCL
#define TFT_DC 9     // IO09 = RS  (data/command)
#define TFT_RST 10   // IO10 = display reset
#define TFT_CS 21    // IO21 = chip select
#define TFT_MISO -1  // not connected
#define TFT_BL 2     // IO02 = backlight, PWM through Q2

#define TFT_HEIGHT 428   // physical rows
#define TFT_WIDTH 142    // physical columns
#define TFT_OFFSET_X1 12
#define TFT_OFFSET_Y1 0
#define TFT_OFFSET_X2 14
#define TFT_OFFSET_Y2 0
#define SCREEN_ROTATE    // swap logical dimensions for the Meshtastic UI
#define TFT_ROTATION 1
#define SCREEN_TRANSITION_FRAMERATE 5
#define HAS_SCREEN 1
#define TFT_BLACK 0
#define BRIGHTNESS_DEFAULT 254
#define USE_TFTDISPLAY 1

// -- Buttons ------------------------------------------------------------------
// All buttons are switch-to-GND with a debounce cap and no external pull
// resistors, so they need internal pullups.
//
// Primary button (IO40): short press advances the frame, long press opens the
// menu, very long press shuts down. Handled by Meshtastic ButtonThread.
#define BUTTON_PIN 40
#define BUTTON_ACTIVE_LOW true
#define BUTTON_ACTIVE_PULLUP true

// Directional buttons are routed through TrackballInterruptImpl1, which emits
// INPUT_BROKER_UP / DOWN / LEFT / RIGHT into InputBroker with no custom code.
//
// WARNING: IO43 is UART0 TX. It floats high during boot because the ROM
// bootloader drives it, and this board has no PCB pull-down. Validate this
// button explicitly on hardware. See Task 4.
#define HAS_TRACKBALL 1
#define TB_UP 255    // no dedicated pin; IO40 is owned by ButtonThread
#define TB_DOWN 43   // IO43 - UART0 TX, see warning above
#define TB_LEFT 1    // IO01
#define TB_RIGHT 18  // IO18
#define TB_PRESS 255 // IO40 press timing is owned by ButtonThread
#define TB_DIRECTION FALLING

// -- Absent hardware ----------------------------------------------------------
// No GPS, no battery, no I2C sensors, no secondary radio.
#define GPS_DEFAULT_NOT_PRESENT 1
// No I2C bus is wired on this board. This disables Meshtastic's own I2C
// scanning/usage in main.cpp; it does not affect the SDA/SCL macros needed
// below purely to let the Arduino Wire library compile.
#define HAS_WIRE 0

#endif // _VARIANT_BADGE_JSCON2026_H_
