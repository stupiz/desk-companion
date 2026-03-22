# Desk Companion

An ESP32-S3 desk companion for the LilyGO T-Display S3 Long.

The current build is designed around a compact always-on desk display that shows:

- a character-like home screen with animated pixel eyes
- a detail dashboard for CO2, temperature, humidity, and overall air quality
- a focus timer with separate focus and break durations
- a standalone countdown timer
- a large status screen for desk presence

## Current Hardware Target

The most complete target right now is:

- LilyGO T-Display S3 Long
- Sensirion SCD41 breakout

Optional:

- Adafruit SGP40 for VOC readings

Current board support in `platformio.ini`:

- `desk_companion`
- `desk_companion_upload`
- `desk_companion_lilygo_s3`
- `desk_companion_lilygo_s3_upload`
- `desk_companion_t_display_long`
- `desk_companion_t_display_long_upload`

The active, tested path is `desk_companion_t_display_long`.

## BOM

Core hardware for the current build:

- LilyGO T-Display S3 Long: [AliExpress](https://s.click.aliexpress.com/e/_c3GPFosb)
- Sensirion SCD41 breakout: [AliExpress](https://s.click.aliexpress.com/e/_c2yRMQG7)

Optional sensor:

- Sensirion SGP40 breakout: [AliExpress](https://s.click.aliexpress.com/e/_c3Gblwkj)

Note:

- `SGP40` is listed in the BOM, but it is not part of the current implemented and tested hardware path yet.

## What Works

- Stable display output on the T-Display S3 Long
- Touch input with built-in default calibration for the current hardware
- SCD41 readings on the Long board
- Persistent presets for:
  - focus duration
  - break duration
  - timer duration
- Main pages:
  - `HOME EYES`
  - `HOME DETAIL`
  - `FOCUS`
  - `TIMER`
  - `STATUS`
  - `FOCUS SETTINGS`
  - `TIMER SETTINGS`
  - `TIME EDITOR`

## Air Quality Model

The air model uses four severity levels:

- `GREAT`
- `NORMAL`
- `STUFFY`
- `BAD`

Current thresholds:

| Metric | GREAT | NORMAL | STUFFY | BAD |
| --- | --- | --- | --- | --- |
| CO2 | `< 801 ppm` | `801-1000 ppm` | `1001-1400 ppm` | `>= 1401 ppm` |
| VOC Index | `< 100` | `100-149` | `150-249` | `>= 250` |
| Temperature | comfort zone | near edge | warm/cool edge | `< 18 C` or `>= 30 C` |
| Humidity | comfort zone | near edge | dry/humid edge | `< 20%` or `> 70%` |

Overall severity is based on:

- `CO2` and `VOC` as the base severity
- `temperature` and `humidity` as comfort penalties

If only `SCD41` is connected:

- `CO2`, `temperature`, and `humidity` are live
- `VOC` is shown as unavailable/offline

## UI Flow

Main navigation:

- `HOME`: tap toggles `EYES` and `DETAIL`
- `HOME`: swipe up goes to `FOCUS`
- `FOCUS`: tap opens settings when idle, or pause/resume when active
- `FOCUS`: long press resets the current focus session
- `FOCUS`: swipe up goes to `TIMER`
- `FOCUS`: swipe down goes back to `HOME`
- `TIMER`: tap opens settings when idle, or pause/resume when active
- `TIMER`: long press resets the timer
- `TIMER`: swipe up goes to `STATUS`
- `TIMER`: swipe down goes back to `FOCUS`
- `STATUS`: tap cycles the status text
- `STATUS`: long press cycles the text effect
- `STATUS`: swipe down goes back to `TIMER`

Settings pages:

- `FOCUS SETTINGS`: tap `FOCUS` or `BREAK` to edit time, tap `START SESSION` to begin
- `TIMER SETTINGS`: tap `DURATION` to edit time, tap `START TIMER` to begin
- `TIME EDITOR`: tap `HOUR`, `MIN`, or `SEC`, then use `+` and `-`, then `SAVE` or `BACK`

## Hardware Notes

For `DESKCOMPANION_BOARD_T_DISPLAY_S3_LONG`, the current firmware uses:

- I2C `SDA=15`
- I2C `SCL=10`
- touch controller address `0x3B`
- display backlight pin `1`

These values live in [src/config/app_config.h](src/config/app_config.h).

The firmware also probes a second known QWIIC bus on the Long board:

- `Wire1(43,44)`

Current behavior:

- `SCD41` is detected and used when available
- `SGP40` is planned/optional, but not implemented as part of the current tested build
- the LED on most `SCD41` breakout boards is usually a board power LED, not something controlled by the sensor over I2C

## Build

Compile only:

```bash
pio run -e desk_companion_t_display_long
```

Flash to a connected board:

```bash
pio run -e desk_companion_t_display_long_upload -t upload --upload-port <port>
```

Open serial monitor:

```bash
pio device monitor -b 115200 -p <port>
```

The default PlatformIO environment is compile-only on purpose to avoid accidental uploads.

If `pio` is not in your shell `PATH`, use the PlatformIO CLI installed on your machine instead.

## Useful Serial Commands

At `115200`, the firmware supports several debug and control commands:

- `1` `2` `3` `4` `5` jump to the main pages
- `f` start focus using the current preset
- `F` open focus settings
- `t` start timer using the current preset
- `T` open timer settings
- `i` rescan I2C buses and sensor devices
- `d` toggle touch debug logs
- `c` start touch calibration
- `C` restore built-in touch calibration
- `e` print help

There are also extra serial shortcuts for settings and time editing, mainly for debugging.

## Touch Calibration

The current firmware includes:

- built-in default calibration values for the current T-Display S3 Long setup
- a runtime calibration flow triggered by serial command `c`

Calibration is useful if:

- touch feels mirrored
- tap targets do not line up with the visible cards
- a different touch panel or board revision is used

## Project Structure

- [src/main.cpp](src/main.cpp): app loop, serial commands, page routing, touch actions
- [src/page_manager.cpp](src/page_manager.cpp): navigation, timers, settings state, persistence
- [src/sensor_manager.cpp](src/sensor_manager.cpp): SCD41/SGP40 init and sensor polling
- [src/display_manager.cpp](src/display_manager.cpp): display setup and frame presentation
- [src/input_manager.cpp](src/input_manager.cpp): touch mapping, tap/swipe/long-press detection
- [src/render/](src/render): UI renderers for each page
- [bringup/t_display_long_smoke](bringup/t_display_long_smoke): isolated panel smoke-test project for display bring-up

## Known Limitations

- The project is currently tuned for one specific T-Display S3 Long setup and touch calibration.
- `SGP40` is not implemented as part of the current tested hardware setup, so `VOC` may remain offline.
- No firmware-level power-off flow is implemented.
- Sensor breakout power LEDs usually cannot be disabled from software.

## Status

This project is in active hardware bring-up and UX iteration, but the main workflow is already usable on the current target hardware.

## Credits

This project was developed with coding assistance from Codex GPT-5.4.
