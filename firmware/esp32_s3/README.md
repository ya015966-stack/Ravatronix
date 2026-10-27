# ESP32-S3 Firmware Bring-up

Target reported by the project owner: ESP32-S3-N16R8, with OV3660 camera connected by FFC. Cellular modem A7670E is a separate module.

## Phase 1 — safe baseline
- [ ] Confirm exact board manufacturer/revision and pinout.
- [ ] Select ESP32-S3 in the build environment; use 16 MB flash only if confirmed by the module/board specification.
- [ ] Confirm PSRAM type and enable the matching setting only when supported by this board.
- [ ] Build a serial-only test before adding camera or modem libraries.
- [ ] Flash through the correct USB connector and save the serial boot log.

## Phase 2 — camera
- [ ] Confirm the OV3660 sensor is supported by the selected camera driver/version.
- [ ] Obtain the exact camera GPIO map from the board schematic; do not copy pin maps from unrelated ESP32-S3 camera boards.
- [ ] Inspect/reseat the FFC with power disconnected.
- [ ] Test camera initialization and one frame before enabling networking.

## Phase 3 — modem
- [ ] Identify the A7670E carrier-board power input and UART logic levels from its documentation.
- [ ] Use a suitable supply with adequate current headroom; share ground with the ESP32-S3.
- [ ] Confirm UART pins and baud rate from the actual wiring.
- [ ] Test basic AT responses, SIM registration, APN, and data connection separately.

## Build environment
Use Arduino IDE-compatible tooling or PlatformIO. On Android, browser-based compilers may have limited board-menu and USB options; if a build cannot expose the required settings, use a reproducible cloud build rather than guessing configuration.

## Current status
No new firmware is assumed to be installed. GPIO mapping, camera detection, modem wiring, and successful compilation are **not yet verified**.
