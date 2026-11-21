# Ravatronix ESP32-S3 integrated firmware

ESP-IDF v5.3.2 project for the ESP32-S3-N16R8. The default profile boots
with memory diagnostics and performs one OV3660 JPEG capture. Audio and
cellular startup remain disabled until their hardware-specific checks pass.

## Current service status

- OV3660 one-shot JPEG capture uses the previously used board map; independent
  schematic confirmation is still pending.
- Wi-Fi station/provisioning service is available. Its temporary AP password
  is not printed in normal builds; debug-only serial disclosure is an explicit
  Kconfig option and should remain disabled outside controlled bench sessions.
- A7670E service currently performs bounded UART diagnostics only (AT, ATE0,
  AT+GMR). It does not open a cellular data session or perform HTTPS requests.
- PPP is retired and is not used by the application. The A7670E internal AT
  data stack is the intended future transport.
- Audio and cellular remain disabled by default.

## Build

From this directory in an ESP-IDF v5.3.2 shell:

    idf.py set-target esp32s3
    idf.py reconfigure
    idf.py build
    idf.py merge-bin -f raw

The board profile selects 16 MB flash and 8 MB octal PSRAM, with DIO/40 MHz
flash and 40 MHz PSRAM based on the known-good diagnostic boot. Managed
dependencies are declared in component manifests. The retired PPP component
is no longer required by the application.

## Hardware map

Camera: XCLK 15, SCCB 4/5, data 16/17/18/12/10/8/9/11, VSYNC 6, HREF 7,
PCLK 13. Microphone candidate: I2S0 BCLK/WS/DOUT 38/39/40. MAX98357A
candidate: I2S1 BCLK/WS/DIN 1/2/21. A7670E UART candidate: ESP TX 48,
ESP RX 47.

The camera GPIO map is the previously used map from the working camera
firmware. Audio and modem assignments remain provisional until their exact
module wiring and electrical details are confirmed.

## Explicit hardware gates

- Camera requires RAVA_ENABLE_CAMERA and RAVA_CAMERA_PINMAP_CONFIRMED.
- Audio requires RAVA_ENABLE_AUDIO, RAVA_AUDIO_WIRING_CONFIRMED, and
  RAVA_MIC_FORMAT_CONFIRMED; all are disabled by default.
- A7670E AT diagnostics require RAVA_ENABLE_CELLULAR,
  RAVA_MODEM_ELECTRICAL_CONFIRMED, and RAVA_MODEM_AT_UART_CONFIRMED;
  all are disabled by default.
- RAVA_WIFI_LOG_SETUP_SECRET is disabled by default and should not be
  enabled in routine builds.

The A7670E APN is intentionally blank. Never power the modem from an ESP32
GPIO or an undersized 3.3 V rail. The modem carrier-board input rating does
not establish UART logic-level compatibility.

## Backend and privacy

The backend client accepts an HTTPS URL and bearer token at runtime; no
provider API key or device token is embedded in source defaults. The device
does not expose an unauthenticated camera server or public MJPEG endpoint.

The older Arduino camera Wi-Fi sketch is retained as a legacy diagnostic only.

## Not yet implemented

The AT diagnostic is not a cellular internet transport. Remaining work:
confirm the installed modem firmware revision and native HTTPS/file-transfer
capabilities, implement a binary-safe AT HTTP(S) adapter with certificate
verification, add gateway health probes, and connect a network manager to the
voice transport. A successful compile alone cannot validate these
hardware-dependent behaviors.
