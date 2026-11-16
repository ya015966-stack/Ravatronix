# Ravatronix ESP32-S3 integrated firmware

ESP-IDF v5.3.2 project for the ESP32-S3-N16R8. The default profile boots
with memory diagnostics and performs one OV3660 JPEG capture. Audio and
cellular startup remain disabled until their hardware-specific checks pass.

## Services prepared in this branch

- OV3660 one-shot JPEG capture using PSRAM frame buffers.
- Independent I2S microphone capture and MAX98357A playback interfaces.
- A7670E UART/PPPoS cellular networking integration.
- HTTPS snapshot upload client with certificate-bundle validation and a
  runtime bearer token.
- Serial health and memory diagnostics.

**Important:** the integrated image has not yet been built or tested on the
physical board. A successful compile will not prove audio wiring, modem carrier
power, UART voltage levels, or end-to-end cellular data.

## Build

From this directory in an ESP-IDF v5.3.2 shell:

```sh
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
idf.py merge-bin -f raw
```

The board profile selects 16 MB flash and 8 MB octal PSRAM, with DIO/40 MHz
flash and 40 MHz PSRAM based on the known-good diagnostic boot. Component
versions are pinned in `idf_component.yml`. Managed dependencies are declared in the manifest of the component that uses
them: `main/idf_component.yml` pins ESP-IDF compatibility,
`components/camera_service/idf_component.yml` pins `espressif/esp32-camera`
to `2.1.9`, and `components/network_service/idf_component.yml` pins
`espressif/esp_modem` to `1.4.0`. The camera release includes the JPEG EOI
fallback relevant to padded OV3660 frames.

## Hardware map

Camera: XCLK 15, SCCB 4/5, data 16/17/18/12/10/8/9/11, VSYNC 6, HREF 7,
PCLK 13. Microphone candidate: I2S0 BCLK/WS/DOUT 38/39/40. MAX98357A
candidate: I2S1 BCLK/WS/DIN 1/2/21. A7670E UART candidate: ESP TX 48,
ESP RX 47.

The camera GPIO map is the previously used map from the working camera
firmware. Audio and modem assignments remain provisional until their exact
module wiring and electrical details are confirmed.

## Explicit hardware gates

Kconfig gates are available for each service:

- Camera requires `RAVA_ENABLE_CAMERA` and `RAVA_CAMERA_PINMAP_CONFIRMED`.
  The default profile enables a single capture at boot.
- Audio requires `RAVA_ENABLE_AUDIO`, `RAVA_AUDIO_WIRING_CONFIRMED`,
  and `RAVA_MIC_FORMAT_CONFIRMED`; all are disabled by default.
- Cellular requires `RAVA_ENABLE_CELLULAR` and
  `RAVA_MODEM_ELECTRICAL_CONFIRMED`; both are disabled by default.

The A7670E APN is intentionally blank until the SIM operator/APN are confirmed.
Never power the modem from an ESP32 GPIO or an undersized 3.3 V rail. The
modem's carrier-board input rating does not establish UART logic-level
compatibility.

## Backend and privacy

The backend client accepts an HTTPS URL and bearer token at runtime; no
provider API key or device token is embedded in source defaults. The
application/backend provisioning path must supply those values before
snapshot uploads can be used. The device does not expose an unauthenticated
camera server or public MJPEG endpoint.

The older Arduino camera Wi-Fi sketch is retained as a legacy diagnostic only.
