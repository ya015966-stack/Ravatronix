# Ravatronix ESP32-S3 integrated firmware

ESP-IDF v5.3.2 project for the ESP32-S3-N16R8. The current default profile is
a safe diagnostic boot: all peripheral services are disabled until their
hardware-specific confirmation gates are deliberately enabled.

## Services prepared in this branch

- OV3660 one-shot JPEG capture using PSRAM frame buffers.
- Independent I2S microphone capture and MAX98357A playback interfaces.
- A7670E UART/PPPoS cellular networking integration.
- HTTPS snapshot upload client with certificate-bundle validation and a
  runtime bearer token.
- Serial health, memory, camera-capture, and microphone diagnostics.

**Important:** compiling the project does not prove the board pin map, audio
wiring, modem carrier power, or UART voltage levels. The integrated image has
not yet been built or tested on the physical board.

## Build

From this directory in an ESP-IDF v5.3.2 shell:

```sh
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

The board profile selects 16 MB flash and 8 MB octal PSRAM, with DIO/40 MHz
flash and 40 MHz PSRAM based on the known-good diagnostic boot. Component
versions are pinned in `idf_component.yml`.

## Hardware map (provisional)

Camera: XCLK 15, SCCB 4/5, data 16/17/18/12/10/8/9/11, VSYNC 6, HREF 7,
PCLK 13. Microphone candidate: I2S0 BCLK/WS/DOUT 38/39/40. MAX98357A
candidate: I2S1 BCLK/WS/DIN 1/2/21. A7670E UART candidate: ESP TX 48,
ESP RX 47.

These assignments must be confirmed against the exact board and module
documentation before enabling the related service.

## Explicit hardware gates

All gates default to disabled in `Kconfig.projbuild`:

- Camera requires `RAVA_ENABLE_CAMERA` and `RAVA_CAMERA_PINMAP_CONFIRMED`.
- Audio requires `RAVA_ENABLE_AUDIO`, `RAVA_AUDIO_WIRING_CONFIRMED`,
  and `RAVA_MIC_FORMAT_CONFIRMED`.
- Cellular requires `RAVA_ENABLE_CELLULAR` and
  `RAVA_MODEM_ELECTRICAL_CONFIRMED`.

The A7670E APN is intentionally blank until the SIM operator/APN are confirmed.
Never power the modem from an ESP32 GPIO or an undersized 3.3 V rail. The
modem's carrier-board input rating does not establish UART logic-level
compatibility.

## Backend and privacy

The backend client accepts an HTTPS URL and bearer token at runtime; no
provider API key or device token is embedded in source defaults. The Android
application/backend provisioning path must supply those values before
snapshot uploads can be used. The device does not expose an unauthenticated
camera server or public MJPEG endpoint.

The older Arduino camera Wi-Fi sketch is retained as a legacy diagnostic only.
