#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include "variant.h"
#include <stdint.h>

// Badge JSCON 2026 - ESP32-S3-WROOM-1-N8

// No I2C bus is wired on this board (HAS_WIRE 0 in variant.h keeps Meshtastic
// from using it). These are unused spare GPIOs, defined only because the
// Arduino Wire library's TwoWire::initPins() references SDA/SCL
// unconditionally at compile time even when I2C is never invoked.
static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

// UART0. Note IO43 is also wired to the DOWN button; UART0 is not used by
// this firmware, which talks to the host over native USB CDC instead.
#define TX 43
#define RX 44

// Default SPI bus maps to the LoRa radio.
#define SS SX126X_CS
#define MOSI LORA_MOSI
#define MISO LORA_MISO
#define SCK LORA_SCK

#endif // Pins_Arduino_h
