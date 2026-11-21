# Ravatronix ESP32-S3 Firmware Integration — Design

**Date:** 2026-11-16  
**Status:** Proposed design for owner review  
**Target branch:** `feat/firmware-integration`  
**Target platform:** ESP32-S3-N16R8, ESP-IDF v5.3.2

## 1. Goal and scope

Create one maintainable ESP-IDF firmware for the Ravatronix controller that brings together:

- OV3660 image capture.
- A7670E cellular data connectivity over UART and PPPoS, subject to modem/carrier compatibility verification.
- I2S microphone capture.
- MAX98357A speaker playback.
- A network service layer that can send selected images/audio/status to an authenticated backend for future online-AI integration.

This document defines architecture and verification gates. It does not claim that the proposed GPIO assignments, modem electrical interface, carrier power, or integrated firmware have been tested on the physical device.

Out of scope for the first integrated milestone: autonomous driving, motor/servo control, voice calling, wake-word detection, continuous public MJPEG streaming, and embedding AI credentials in the device.

## 2. Current evidence and assumptions

### Confirmed from project records

- The controller is reported as ESP32-S3-N16R8 with 16 MB flash and 8 MB octal PSRAM.
- A prior ESP-IDF v5.3.2 diagnostic boot reached the second-stage bootloader; 8 MB PSRAM was detected and a memory test passed.
- The existing camera sketch contains an OV3660 configuration and the reported camera pin map, but the exact board schematic has not yet independently confirmed that map.
- The separate audio test project uses ESP-IDF v5.3.2 and documents an INMP441-compatible I2S microphone on I2S0 and MAX98357A playback on I2S1.
- The owner confirms the A7670E UART wiring as modem TX → ESP32-S3 GPIO47, modem RX → GPIO48, with common ground.

### Provisional hardware map

| Function | Signal | ESP32-S3 GPIO | Status |
|---|---|---:|---|
| OV3660 | XCLK | 15 | Reported; schematic confirmation pending |
| OV3660 | SCCB SDA / SCL | 4 / 5 | Reported; schematic confirmation pending |
| OV3660 | Y2–Y9 | 11, 9, 8, 10, 12, 18, 17, 16 | Reported; schematic confirmation pending |
| OV3660 | VSYNC / HREF / PCLK | 6 / 7 / 13 | Reported; schematic confirmation pending |
| Microphone | BCLK / WS / DOUT | 38 / 39 / 40 | Audio test wiring plan; verify exact microphone |
| Microphone | L/R select | GND (left) | Depends on microphone model |
| MAX98357A | BCLK / WS / DIN | 1 / 2 / 21 | Audio test wiring plan |
| A7670E | ESP RX / ESP TX | 47 / 48 | Owner-confirmed wire direction; electrical levels pending |

The A7670E carrier-board supply input, regulator capability, peak-current headroom, UART voltage levels, and need for level translation must be established from the exact carrier-board documentation before enabling the modem electrically. Do not power the modem from an ESP32 GPIO or an undersized 3.3 V rail.

## 3. Architecture

Use ESP-IDF 5.3.2 with small components and explicit interfaces rather than one large application file.

Proposed layout:

- `main/`: application startup and task orchestration.
- `components/board_config/`: board constants and compile-time configuration; uncertain pin maps remain clearly marked provisional.
- `components/camera_service/`: OV3660 initialization, frame capture, frame-buffer ownership, and capture diagnostics.
- `components/audio_service/`: microphone RX and speaker TX I2S configuration, PCM format conversion, bounded buffers, and audio diagnostics.
- `components/modem_service/`: UART transport, AT command state machine, SIM/network registration status, and modem error reporting.
- `components/network_service/`: cellular data session (candidate: ESP-IDF PPPoS with a compatible esp_modem component), reconnect policy, and network status.
- `components/backend_client/`: authenticated TLS requests for snapshots, status, and later audio exchanges.
- `docs/`: wiring evidence, test procedures, and integration decisions.

Tasks must use bounded queues and explicit timeouts. Camera capture, audio capture/playback, modem control, and network transfer must not block one another. Frame buffers and PCM buffers must have clear ownership and be returned/freed on every success and error path.

## 4. Data flow

1. Boot, initialize diagnostics, verify heap and PSRAM, and report firmware version.
2. Initialize the camera and verify a single JPEG capture locally.
3. Initialize microphone input and speaker output independently; measure microphone signal quality and verify low-level playback.
4. Initialize the modem UART and issue basic AT commands only after electrical compatibility is confirmed.
5. Verify SIM readiness and network registration, then establish a cellular data session.
6. Send an explicitly requested JPEG snapshot and device status to an authenticated backend over TLS.
7. Add online-AI request/response and interactive voice only as later milestones after the transport and privacy controls are verified.

The first cellular camera milestone is snapshot transfer, not an unauthenticated camera server exposed directly to the public internet. Continuous streaming, if later required, should be relayed through an authenticated backend and designed around cellular bandwidth and data use.

## 5. Audio quality and behavior

The first audio integration milestone is independent microphone capture plus speaker playback, not microphone-to-speaker live monitoring.

Diagnostics should record sample rate, sample width, selected I2S slot/channel, DMA/buffer settings, peak level, RMS, clipping count, and capture errors. RMS is only a relative activity measure, not a calibrated sound-pressure measurement.

To investigate the previously reported poor microphone quality, verify the exact microphone model and its I2S format, channel selection, bit alignment, sample conversion, clipping, noise floor, and power/ground quality. Do not mask a capture problem by arbitrarily increasing speaker volume or digital gain. Echo cancellation and full-duplex conversational audio are later work.

## 6. Security and privacy

- No API keys, access tokens, SIM PINs, APN credentials, private certificates, or Wi-Fi passwords in source control or firmware defaults.
- Backend credentials must be provisioned securely; the backend owns online-AI provider credentials.
- Use TLS and authenticated device requests. Do not expose the device's raw camera HTTP/MJPEG endpoints to the public internet.
- Transfer only the image/audio requested for a feature; document retention and access policy on the backend.
- Remove the hard-coded Wi-Fi AP password and avoid printing credentials in serial logs.

## 7. Build and release strategy

- Use ESP-IDF v5.3.2 and a reproducible build configuration for ESP32-S3, 16 MB flash, and octal PSRAM, after reconciling settings with the successful board diagnostic.
- Add compile-time configuration for provisional hardware features so camera/audio/modem modules can be built and tested in isolation.
- Keep `main` unchanged; work only on `feat/firmware-integration` until reviewed.
- Do not start GitHub Actions until the owner has been told whether the run may consume included Actions minutes or incur charges and has explicitly approved that run.
- Publish a merged flashing artifact only after a successful build and artifact integrity checks. Record flash offsets and board settings with the artifact.
- Preserve the last known working diagnostic firmware and its logs. Do not flash an integrated image until the relevant build and hardware gates pass.

## 8. Verification milestones and exit criteria

### M0 — Reproducible baseline
- Build an ESP-IDF 5.3.2 serial/PSRAM baseline.
- Confirm target, flash size/mode, PSRAM mode, and partition table.
- Exit: successful build and serial boot log; no hardware success inferred from compilation alone.

### M1 — Camera
- Verify exact board pinout against a schematic or reliable board documentation.
- Initialize OV3660 and capture a valid JPEG in PSRAM.
- Exit: sensor identity, frame dimensions, capture success, and repeated capture stability recorded.

### M2 — Audio
- Verify the actual microphone model and wiring.
- Capture PCM and inspect signal levels; play a low-level test tone through MAX98357A.
- Exit: repeatable capture without clipping/obvious format corruption and stable playback without resets.

### M3 — Modem control
- Verify carrier-board power input/regulator, peak-current capability, and UART logic levels.
- Test UART/AT response, SIM readiness, and network registration.
- Exit: stable AT communication and documented registration state. No modem power-up test before the electrical gate passes.

### M4 — Cellular data and secure snapshot
- Establish PPPoS using a compatible, pinned ESP-IDF component/version.
- Test DNS and TLS to the backend.
- Upload a single authenticated JPEG and status payload.
- Exit: successful authenticated transfer with bounded retries and no public unauthenticated device endpoint.

### M5 — Online AI foundation
- Route user-approved text/image/audio requests through the backend.
- Keep provider secrets on the backend.
- Exit: documented request/response path, access controls, error handling, and data-retention behavior.

## 9. Risks and open decisions

1. Exact ESP32-S3 board schematic and camera GPIO map are not yet independently verified.
2. Exact microphone IC/model is not confirmed; the existing audio test assumes INMP441-compatible behavior.
3. A7670E carrier-board electrical specifications and UART logic levels are unknown.
4. Compatibility of the selected `esp_modem` release with ESP-IDF v5.3.2 must be checked and pinned before implementation.
5. Simultaneous camera, audio, and modem activity may create memory, bandwidth, or power pressure; measure each milestone before enabling concurrent operation.
6. SIM/APN configuration must be supplied securely outside the repository.
7. Cellular data use, backend hosting cost, and GitHub Actions quota must be reviewed before enabling recurring or expensive services.

## 10. Approval boundary

This design is limited to architecture and staged verification. Implementation, new firmware commits beyond this design, GitHub Actions execution, hardware power-up tests, and flashing are separate steps. The owner should review this document before an implementation plan is written.
