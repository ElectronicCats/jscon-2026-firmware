# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

> **TL;DR**
>
> |                |                                                                                                                         |
> | -------------- | ----------------------------------------------------------------------------------------------------------------------- |
> | Local tests    | `./bin/run-tests.sh` (exit 0 GREEN, 1 RED, 2 AMBER, 3 FILTERED). Linux-only; elsewhere `./bin/test-native-docker.sh`    |
> | Hardware tests | [meshtastic/meshtastic-mcp](https://github.com/meshtastic/meshtastic-mcp) (`MESHTASTIC_FIRMWARE_ROOT` -> this checkout) |
> | Format         | `trunk fmt`                                                                                                             |
> | Mirror docs    | `.github/copilot-instructions.md` (canonical), `AGENTS.md`                                                              |
>
> **Need this? It's here.**
>
> |                                                           |                                                            |
> | --------------------------------------------------------- | ---------------------------------------------------------- |
> | General helpers (clamp, UTF-8, string fmt...)             | `src/meshUtils.h`                                          |
> | Logging macros (LOG_DEBUG / INFO / WARN...)               | `src/DebugConfiguration.h`                                 |
> | Elapsed time / deadlines (never bare `millis()` compares) | `src/mesh/Throttle.h`                                      |
> | New module skeleton                                       | inherit `ProtobufModule<T>` in `src/mesh/ProtobufModule.h` |
> | Observer / event wiring                                   | `src/Observer.h`                                           |

**Read `.github/copilot-instructions.md` first.** That file is the canonical agent-facing document for this repo. It covers project layout, coding conventions, the build system, CI/CD, the native C++ test suite, and the MCP Server & Hardware Test Harness. Read it top-to-bottom before starting any non-trivial change. `AGENTS.md` holds the quick command reference, MCP workflows, and recovery one-liners.

This file (`CLAUDE.md`) is a short pointer for Claude Code sessions. The `/test`, `/diagnose`, `/repro` and `/leakhunt` workflows ship as skills in the [meshtastic-mcp](https://github.com/meshtastic/meshtastic-mcp) repo, not in this checkout; the MCP tools themselves are registered here via `.mcp.json`.

## Commands

| Action                       | Command                                                                  |
| ---------------------------- | ------------------------------------------------------------------------ |
| Build one variant            | `pio run -e <env>` (a bare `pio run` builds `default_envs`, `heltec-v3`) |
| Build + flash                | `pio run -e <env> -t upload --upload-port <port>`                        |
| Clean                        | `pio run -e <env> -t clean`                                              |
| All native tests (Linux)     | `./bin/run-tests.sh`                                                     |
| Single suite (Linux)         | `./bin/run-tests.sh -f test_<name>` (yields FILTERED, not a gate)        |
| Native tests (macOS/Windows) | `./bin/test-native-docker.sh [-f test_<name>] [--rebuild]`               |
| Format                       | `trunk fmt` (also run on every Write/Edit by `.claude/settings.json`)    |
| Regenerate protobufs         | `bin/regen-protos.sh` (never hand-edit `src/mesh/generated/`)            |
| CI build matrix              | `./bin/generate_ci_matrix.py all [--level pr]`                           |

Do not pipe `pio test`; redirect to a file and grep it. Test authoring guide: `test/README.md`.

## Architecture in one screen

- `src/main.cpp` boots the hardware, then `src/modules/Modules.cpp` constructs every module, each guarded by its `MESHTASTIC_EXCLUDE_*` flag. Excluding a module leaves its global pointer null, so check for unguarded dereferences elsewhere before excluding one.
- Packets flow radio interface (`src/mesh/*Interface.*`) -> `Router` (decrypt via `CryptoEngine`, dedup, routing) -> `MeshModule` chain (`wantPacket` / `handleReceived`) -> `PhoneAPI` for clients over BLE, serial or TCP.
- Periodic work runs on cooperative `concurrency::OSThread` instances (`runOnce()` returns the next interval in ms), not on preemptive tasks.
- A board is a directory under `variants/<arch>/<name>/` holding `variant.h` (pins, capability defines) and `platformio.ini` (env, `extends` an arch base, build flags). Board-specific behavior in shared sources is selected by `#ifdef` on a define that the variant sets.
- Input sources are `Observable<const InputEvent *>` objects registered with `InputBroker` (`src/input/InputBroker.cpp`), which fans events out to the UI.
- `NodeDB` is the persisted state store; its v25 layout (slim `NodeInfoLite` plus satellite maps behind copy-out accessors) is described in `.github/copilot-instructions.md` and must be read before touching node fields.

## Badge JSCON 2026 variant

Work on this branch targets the Electronic Cats JSCON 2026 badge: env `badge_jscon2026`, files in `variants/esp32s3/badge_jscon2026/`. Shared sources are touched only inside `#ifdef BADGE_JSCON2026` blocks (`src/main.cpp`, `src/graphics/TFTDisplay.cpp`, `src/input/InputBroker.cpp`).

```bash
pio run -e badge_jscon2026
pio run -e badge_jscon2026 -t upload --upload-port <port>
```

Constraints recorded in `variant.h` and `platformio.ini`, which hold the full rationale:

- **MCU**: ESP32-S3-WROOM-1-N8, 8 MB flash, no PSRAM. RAM is the scarce resource; the build trims packet pools and excludes modules to leave heap for a planned JS runtime. `main.cpp` logs the post-setup heap baseline.
- **Radio**: LLCC68, not SX1262, with no TCXO. Never define `SX126X_DIO3_TCXO_VOLTAGE`. SF12 is unsupported and BW 250 tops out at SF10, so `LONG_FAST`, `LONG_SLOW` and `VLONG_SLOW` fail radio init. The default preset is `SHORT_FAST`, chosen for about 100 badges in one room, so the badge does not interoperate with nodes left on stock `LONG_FAST`. A default only applies to a badge with no saved config.
- **Display**: NV3007 panel on HSPI through Arduino_GFX (`USE_ARDUINO_GFX`), constructed in `TFTDisplay.cpp` from the `TFT_*` values in `variant.h`. `HAS_SPI_TFT` must stay defined or `main.cpp` never creates the `Screen`.
- **Buttons**: four pushbuttons driven by `src/input/BadgeJsconButtons.{h,cpp}`, a polled and debounced `OSThread` emitting one event per press: Up = `ALT_PRESS`, Down = `USER_PRESS`, Select = `SELECT`, Back = `CANCEL`. `ALT_PRESS` / `USER_PRESS` switch frames on normal screens and move up / down inside menus; plain `UP` / `DOWN` never switch frames. `HAS_TRACKBALL` is 0 and `BUTTON_PIN` is undefined on purpose; the `TB_*` defines are kept only as the pin source for `BadgeJsconButtons`.
- **Branding**: `jsconf_branding.h` (included by `variant.h`) defines the `USERPREFS_OEM_*` boot logo and the `JSCONF_COLOR_*` values. The boot screen shows only that logo; the Meshtastic splash is skipped. The UI is a 1-bit framebuffer tinted per rectangle (`TFTColorRegions`), so normal artwork must be 1-bit XBM. Full-color art goes through `src/graphics/BadgeSprites.{h,cpp}`: frame code calls `queueBadgeSprite()` and `TFTDisplay::display()` paints the sprite only where a pixel still has the body background color, so text and bars always stay on top. Sprite data for the 11 distinct axolotls, at three sizes, is in `jsconf_sprites.h`; `currentBadgeAxolotl()` rotates through them in shuffled order every 5 s. Every frame gets a full-screen background cut from the kit's blue pattern, queued in `prepareFrameColorRegions()`; the pattern's words are removed on data pages because they collide with text. The Chirpy frame is a name tag on the badge: owner long name in the Righteous font (`jsconf_font.h`, OLEDDisplay font layout), on a card baked into its own background. On the badge the theme with id 0 is the JSConf palette (navy `#17334b` body, darker navy bars, gold accents), defined under a `BADGE_JSCON2026` guard in `TFTColorRegions.cpp`. GPS is compiled out (`HAS_GPS 0`). Source SVGs from the JSConf Mexico 2026 Asset Kit (CC BY-NC-SA 4.0) are in `assets/jsconfmx-2026/`.
- **Axolotl game**: rules in `src/modules/badge/AxolotlGame.h` (pure logic, header only), radio and flash in `AxolotlModule.{h,cpp}`, pages in `src/graphics/draw/BadgeAxolotlRenderer.{h,cpp}`. Each badge owns one of 11 axolotls (the kit has 13 files, but costume = fan and faq = bubbles are the same drawings), derived from its MAC address; the node number cannot be used because it is a hash of the public key and changes when keys are regenerated. Hellos go out on `PRIVATE_APP` as broadcasts with `hop_limit = 0`; two hellos at or above -80 dBm within 30 s unlock the sender's axolotl. The collection is saved to `/prefs/axolotl`. The -80 dBm threshold has not been calibrated on hardware. `test/test_axolotl_game` does not link in the repo's Docker harness yet.
- **Do not add `-DMESHTASTIC_EXCLUDE_EXTERNALNOTIFICATION=1`**: `graphics/SharedUIDisplay.cpp` dereferences `externalNotificationModule` without a null check and the badge boot-loops on the first UI frame.
- **Do not redefine `SERIAL_BUFFER_SIZE`** in the variant env: `esp32s3_base` already sets it, and the redefinition warning is a hard error in `-Werror` ESP-IDF translation units.
- No GPS, no battery gauge, no I2C bus (`HAS_WIRE 0`).

## House rule: documentation does not live in this repo

This repository holds firmware code. There is no `docs/` directory - the design documents that used to sit there were published to [meshtastic/meshtastic](https://github.com/meshtastic/meshtastic) in #11488 and the directory was deleted - and it must not come back. Do not create a `.md` file to describe a feature, a configuration surface, an API, a wire format, or a design; write it in the docs repo and link that PR instead. Never leave a write-up behind in the tree: no investigation notes, no mitigation plans, no migration checklists, no "how we got here" narrative, no summaries of what a change did. That is what the PR description and the commit message are for, and they are the only place it belongs. When you do write documentation upstream, write a technical manual, not a novel - what the feature does, the settings it exposes in the user's terms, and the exact API or protocol a client speaks. No story of the debugging journey, no rationale essays, no changelog prose. Concise and factual, as short as the facts allow.
