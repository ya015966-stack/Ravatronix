# RAVA — Initial System Architecture

## Goal
Build RAVA as a connected robot with an ESP32-S3 controller, an OV3660 camera, and a separate A7670E cellular modem. Priorities are camera access over the web, cellular connectivity, and an online AI service. Keep secrets out of firmware and Git.

## Proposed components
- **ESP32-S3-N16R8:** real-time controller, camera capture, local status/API endpoints, and hardware coordination.
- **OV3660:** image capture, subject to confirming the exact sensor and board pin mapping.
- **A7670E (no GPS):** cellular data and, if supported by the exact board/carrier and firmware, voice calling. The modem is separate from the ESP32-S3.
- **Backend service:** securely brokers requests to an online AI provider and can relay camera snapshots/status. API keys belong on the backend, not in public firmware or the Android app.
- **Android client:** text/voice interaction and camera/status viewing. Implement after the hardware communication path is verified.

## Data flow (target)
1. ESP32-S3 boots and reports health/status.
2. Cellular modem establishes a data connection, or Wi-Fi is used during bench testing.
3. Camera capture is verified locally, then a protected snapshot endpoint is exposed through a backend/tunnel rather than exposing the device directly to the public internet.
4. The app sends user requests to the backend.
5. Backend forwards permitted requests to the online AI service and returns responses.

## Build order
1. Confirm board revision, flash/PSRAM settings, USB port, and camera pin map.
2. Compile and flash a minimal ESP32-S3 serial/LED test.
3. Verify camera initialization and capture independently.
4. Verify A7670E power, UART, SIM/APN, and data connection independently.
5. Connect camera + modem to the controller without blocking the camera task.
6. Add a small authenticated backend API.
7. Connect the Android app to the backend.
8. Add voice features and robot drive control only after safe stop and power behavior are tested.

## Important constraints
- Do not guess GPIO assignments from a different ESP32-S3 board. Record the exact board schematic/pinout first.
- Do not power the cellular modem from an ESP32 GPIO or an undersized 3.3 V rail. Follow the modem carrier board's power requirements and allow for transmit-current peaks.
- Do not commit SIM PINs, Wi-Fi passwords, API keys, tokens, or private certificates.
- Do not expose an unauthenticated camera or control endpoint to the internet.
- This document is a plan, not a claim that the firmware or hardware has already been tested.
