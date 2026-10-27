# RAVA TRONIX

RAVA TRONIX is the software and hardware workspace for the RAVA connected-robot project.

## Project priorities

1. Bring up the ESP32-S3-N16R8 controller with a verified minimal firmware.
2. Confirm OV3660 camera initialization and image capture using the exact board pin map.
3. Bring up the separate A7670E cellular modem and verify stable power, UART/AT communication, SIM, and data connectivity.
4. Provide authenticated camera/status access through a backend; do not expose the device directly to the public internet.
5. Connect an Android client and online AI service without embedding API keys in firmware or the app.

## Repository guide

- [System architecture](docs/architecture.md) — intended components, data flow, and build order.
- [Hardware verification checklist](docs/hardware-checklist.md) — checks required before integrating components.
- `firmware/esp32_s3/` — ESP32-S3 firmware workspace.

## Current status

The repository contains the initial architecture and hardware checklist. Firmware and physical hardware must be verified on the actual board before any feature is marked complete. The target board is ESP32-S3-N16R8, the camera is OV3660, and the separate cellular module is A7670E (without GPS).

## Security

Never commit API keys, access tokens, SIM PINs, Wi-Fi credentials, or private certificates. Keep backend secrets in environment variables or a secret manager. Require authentication for camera and control endpoints.
