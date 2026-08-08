# Waveshare ESP32-S3 Touch 3.5 + nRF24L01+PA+LNA Port Spec

This spec describes the current firmware and wiring implemented in this repository for the Waveshare `ESP32-S3-Touch-LCD-3.5-C` with one external `nRF24L01+PA+LNA` module.

## Board Target

- Board name: `Waveshare ESP32-S3-Touch-LCD-3.5-C`
- MCU family: `ESP32-S3`
- Framework: `Arduino`
- PlatformIO board target: `esp32-s3-devkitc-1`
- Flash size configured: `16MB`
- PSRAM configured: `8MB`
- Memory type: `qio_opi`
- USB mode: `USB-Serial/JTAG`
- USB CDC on boot: enabled
- Serial baud: `921600`

## Build Environment

- Default PlatformIO environment: `waveshare_touch_35_c`
- Display library: `Arduino_GFX 1.5.5`
- Touch library: `SensorLib`
- IO expander library: `TCA9554`
- Included application sources:
  - `src/main.cpp`
  - `src/esp32_nrf24_jammer/esp32_nrf24_jammer.cpp`
  - `src/esp32_nrf24_jammer/NRF24L01.cpp`
  - `src/esp32_nrf24_jammer/WaveshareTouchUI.cpp`
- Excluded in this build:
  - `src/esp32_nrf24_jammer/WiFiController.cpp`

## External Radio Module

- Radio family: `nRF24L01+`
- Module class: `PA+LNA` variant with external SMA antenna
- Product style: `1100m` class Amazon PA/LNA module
- Logic level: `3.3V only`
- Antenna: SMA antenna must be attached during operation
- Recommended local decoupling at module:
  - minimum: `10uF`
  - preferred: `47uF` to `100uF`
  - placement: directly across radio `VCC` and `GND`

## External nRF24 Wiring

The current firmware expects the radio on a dedicated `HSPI` bus.

- `VCC -> 3V3`
- `GND -> GND`
- `CE -> GPIO38`
- `CSN -> GPIO39`
- `SCK -> GPIO40`
- `MOSI -> GPIO41`
- `MISO -> GPIO42`

### Boot-Time Probe Variants

The firmware probes these mappings in order:

1. `CE=38 CSN=39 SCK=40 MOSI=41 MISO=42`
2. `CE=38 CSN=39 SCK=40 MOSI=42 MISO=41`
3. `CE=39 CSN=38 SCK=40 MOSI=41 MISO=42`
4. `CE=39 CSN=38 SCK=40 MOSI=42 MISO=41`

### April 5, 2026 Early-Morning Wiring Note

This is the code-level effect of the wiring change made between 12:00 AM and 2:00 AM on April 5, 2026:

- `SCK` remains fixed on `GPIO40`
- the firmware now tolerates `CE` / `CSN` being landed in either order
- the firmware now tolerates `MOSI` / `MISO` being landed in either order
- practical result: if those paired nRF24 leads were combined, crossed, or re-landed during rewiring, boot-time probing can still detect the radio

## Radio Driver Settings

- Radio SPI bus: `HSPI`
- Radio SPI clock: `4MHz`
- Radio boot settle delay before reset: `8ms`
- Radio initialization retries: `3`
- Retry delay between init attempts: `40ms`
- Presence test:
  - writes `0x2A` to `CONFIG`
  - reads `CONFIG` back
  - success means SPI/register access is working

## Radio Scan Behavior

- Scan mode: background task
- Scan task core: `Core 1`
- Scan task stack: `4096`
- Scan task priority: `1`
- Scan channel range: `0-125`
- Samples per channel: `16`
- Listen time per sample: `260us`
- Scan result output: `0-100` percent activity per channel
- Scan task idle delay between sweeps: `100ms`
- Detection source: `RPD` register bit 0
- Detection meaning:
  - energy detect only
  - no packet decode
  - no protocol identification at radio layer

### Scan Persistence

- UI data uses peak-hold decay
- If a new sample is weaker than the last one:
  - old value decays by `4`
  - displayed value becomes `max(current, decayed_previous)`

## Radio TX Behavior

- Firmware mode: analyzer only
- Safe mode: forced enabled at startup
- TX / jam entrypoints remain in code but are software-blocked
- No touch or button path enables radio transmit in the current build

## Display Hardware

- Display controller: `ST7796`
- Resolution: `320x480`
- Rotation: `0`
- Display bus: `FSPI`

### LCD Pin Map

- `SCK -> GPIO5`
- `MOSI -> GPIO1`
- `MISO -> GPIO2`
- `DC -> GPIO3`
- `CS -> software value -1`
- `RST -> software value -1`
- `Backlight -> GPIO6`

### LCD Reset Path

- Reset controller: onboard `TCA9554`
- Expander address: `0x20`
- LCD reset pin on expander: bit `1`
- Reset sequence:
  - `HIGH` for `10ms`
  - `LOW` for `10ms`
  - `HIGH` for `200ms`

## Touch Hardware

- Touch controller family: `FT6336 / FT6X36`
- Touch bus: `I2C`
- I2C pins:
  - `SDA -> GPIO8`
  - `SCL -> GPIO7`
- Touch address used by library: `FT6X36_SLAVE_ADDRESS`
- Touch mode used by UI:
  - first touch point only
  - no custom calibration matrix applied in firmware
  - touch is status-only in the current build

## Onboard Controls

- Onboard NeoPixel / LED: disabled in this port
  - code value: `kLedPin = -1`

## UI Behavior

- UI refresh target: every `125ms` or on state change
- Status banner shows:
  - radio ready state
  - analyzer mode
  - active channel count
  - peak frequency / peak strength
  - scan live / pending
  - touch online / offline
- Spectrum graph:
  - 126 bars
  - one bar per channel
- Summary list max entries: `3`

### Summary Detection Heuristics

- Signal start threshold: `12`
- Signal keep threshold: `6`
- Signal bridge threshold: `10`
- Allowed weak-gap bridge: `2 bins`
- Strength formula:
  - weighted by peak
  - weighted by average
  - small width bonus up to `16`
- Type classification:
  - `Bluetooth` if width `<= 4`
  - `WiFi` if width is `8-28`
  - `Wide Band` if width `> 25`
  - else `RF Signal`

## Runtime Flow

1. Serial starts at `921600`
2. UI initializes first
3. Touch and LCD come up even if radio is missing
4. Radio probe tries the configured pin mappings
5. If radio is found:
   - background scanning starts automatically
6. If radio is not found:
   - UI remains active
   - radio state shows offline
   - analyzer stays on-screen

## Practical Hardware Notes For The PA/LNA Module

- This module is less tolerant of weak `3.3V` rails than the small PCB-antenna nRF24 boards.
- If the display works but the radio says not found, suspect power integrity first.
- The first hardware fix should be local decoupling directly at the radio.
- Keep jumper wires short.
- Keep the antenna attached.
- Do not power the module from `5V`.
