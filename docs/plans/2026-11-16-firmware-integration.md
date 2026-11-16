# Ravatronix ESP32-S3 Firmware Integration Implementation Plan

> **For agentic workers:** Use the host's available task-by-task implementation workflow. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a staged ESP-IDF v5.3.2 firmware for ESP32-S3-N16R8 that captures OV3660 images, captures and plays I2S audio, connects through A7670E cellular data, and securely transfers user-requested data to a backend.

**Architecture:** Keep the application in small ESP-IDF components with explicit ownership, bounded queues, timeouts, and independent hardware bring-up. The first deliverables are independently buildable diagnostics; cellular networking and backend transfer are enabled only after the relevant electrical and compatibility gates pass. The device never exposes an unauthenticated camera service to the public internet.

**Tech Stack:** ESP-IDF v5.3.2, C, FreeRTOS, ESP-IDF I2S standard driver, ESP camera driver/component compatible with OV3660, UART AT control, PPPoS through a pinned compatible `esp_modem` release, TLS/HTTP client, Unity component tests, and host-side Python tests.

## Global Constraints

- Work only on `feat/firmware-integration`; do not modify or merge into `main`.
- Target the reported ESP32-S3-N16R8: 16 MB flash and 8 MB octal PSRAM. Reconcile QIO/DIO and flash/PSRAM clock settings against the known-good board boot log before choosing final sdkconfig values; do not copy settings blindly from the Arduino sketch or audio test.
- The board diagnostic previously detected 8 MB PSRAM and passed a memory test. Compilation alone does not prove physical hardware operation.
- Camera GPIO assignments (XCLK 15; SCCB 4/5; data 11,9,8,10,12,18,17,16; VSYNC 6; HREF 7; PCLK 13) remain provisional until the exact board schematic or reliable board documentation confirms them.
- Audio wiring is documented as microphone GPIO 38/39/40 and MAX98357A GPIO 1/2/21. The exact microphone model is not confirmed; do not assume INMP441 format or L/R channel behavior until identified.
- A7670E UART direction is owner-confirmed as modem TX → ESP32 GPIO47 and modem RX → GPIO48, with common ground. UART voltage compatibility, level translation, carrier-board input range, regulator capability, and transmit-current headroom remain unverified. Do not enable modem power or AT tests until those electrical facts are confirmed from the exact carrier-board documentation.
- Never power the modem from an ESP32 GPIO or an undersized 3.3 V rail. Do not run camera, audio, and modem simultaneously until their independent tests pass and power/memory headroom is measured.
- No API keys, access tokens, SIM PINs, APN credentials, private certificates, or Wi-Fi passwords in source control, firmware defaults, or serial logs. Provision secrets outside the repository.
- No raw unauthenticated camera/MJPEG endpoint on the public internet. Send only user-requested snapshots/audio/status through authenticated TLS backend requests.
- Do not run GitHub Actions or other potentially billable cloud builds until the user has been told the possible quota/cost impact and explicitly approves that run. Do not flash the integrated image until build, image-integrity, and relevant hardware gates pass.
- Keep the last known working serial/PSRAM diagnostic image and its logs available as recovery evidence.

---

## Repository layout and interfaces

Observed existing paths:
- `README.md`
- `docs/architecture.md`
- `docs/hardware-checklist.md`
- `docs/specs/2026-11-16-firmware-integration-design.md`
- `firmware/esp32_s3/camera_wifi_ap/camera_wifi_ap.ino` (legacy Arduino camera/AP sketch; not the integrated application)
- `.github/workflows/build-esp32s3.yml` (legacy camera-only Arduino build; do not present its artifact as integrated firmware)

Proposed ESP-IDF paths:
- `firmware/esp32_s3/CMakeLists.txt`, `firmware/esp32_s3/sdkconfig.defaults`, `firmware/esp32_s3/partitions.csv`: reproducible project, board configuration, and partition layout.
- `firmware/esp32_s3/main/app_main.c`: startup, health reporting, and task orchestration only.
- `firmware/esp32_s3/components/board_config/include/board_config.h`: named GPIO/configuration constants with provisional hardware flags.
- `firmware/esp32_s3/components/camera_service/{include/camera_service.h,camera_service.c}`: sensor initialization, capture, frame metadata, and frame-buffer ownership.
- `firmware/esp32_s3/components/audio_service/{include/audio_service.h,audio_service.c}`: independent I2S microphone RX and speaker TX, PCM format conversion, and diagnostics.
- `firmware/esp32_s3/components/modem_service/{include/modem_service.h,modem_service.c}`: UART transport, AT state machine, SIM and registration status.
- `firmware/esp32_s3/components/network_service/{include/network_service.h,network_service.c}`: PPPoS lifecycle, reconnect state, and network readiness.
- `firmware/esp32_s3/components/backend_client/{include/backend_client.h,backend_client.c}`: authenticated TLS snapshot/status transfer.
- `firmware/esp32_s3/tests/host/`: host-side tests for parsers, state transitions, bounds, and configuration.
- `firmware/esp32_s3/tests/`: ESP-IDF Unity tests for target-specific behavior where hardware-independent testing is possible.

Proposed public interfaces (names are the contract for the implementation tasks):
- `esp_err_t camera_service_init(const camera_service_config_t *config)`
- `esp_err_t camera_service_capture_jpeg(camera_frame_t *out, TickType_t timeout)`
- `void camera_service_release(camera_frame_t *frame)`
- `esp_err_t audio_service_init(const audio_service_config_t *config)`
- `esp_err_t audio_service_read_pcm(int32_t *dst, size_t capacity_samples, size_t *samples_read, TickType_t timeout)`
- `esp_err_t audio_service_write_pcm(const int16_t *src, size_t samples, size_t *samples_written, TickType_t timeout)`
- `esp_err_t modem_service_init(const modem_service_config_t *config)`
- `esp_err_t modem_service_command(const char *command, char *response, size_t response_capacity, TickType_t timeout)`
- `esp_err_t network_service_start(const network_service_config_t *config)`
- `bool network_service_is_ready(void)`
- `esp_err_t backend_client_upload_snapshot(const camera_frame_t *frame, backend_result_t *result, TickType_t timeout)`
- `esp_err_t backend_client_send_status(const device_status_t *status, backend_result_t *result, TickType_t timeout)`

All interfaces return explicit ESP-IDF errors, use caller-supplied bounded buffers or explicit ownership, and accept timeouts for blocking operations. Configuration structs must not contain literal secrets; secret values are loaded from an approved provisioning source at runtime.

## Implementation tasks

### Task 1: Reproducible ESP-IDF baseline and board configuration

**Files:**
- Create: `firmware/esp32_s3/CMakeLists.txt`
- Create: `firmware/esp32_s3/main/CMakeLists.txt`
- Create: `firmware/esp32_s3/main/app_main.c`
- Create: `firmware/esp32_s3/components/board_config/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/board_config/include/board_config.h`
- Create: `firmware/esp32_s3/sdkconfig.defaults`
- Create: `firmware/esp32_s3/partitions.csv`
- Create: `firmware/esp32_s3/tests/host/test_board_config.py`
- Modify: `firmware/esp32_s3/README.md`

**Interfaces:**
- Consumes: the confirmed target facts in the design document and the known-good serial/PSRAM diagnostic evidence.
- Produces: an ESP-IDF project whose `app_main` reports firmware version, target, free heap, PSRAM size, and enabled feature flags without initializing unverified peripherals.

- [ ] **Step 1: Add the focused failing test**

Add host tests asserting that the board configuration labels camera pins as provisional, records the target as ESP32-S3, declares 16 MB flash and octal PSRAM as intended settings, and does not contain credentials or the legacy AP password.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_board_config.py -q`

Expected: the test command reports missing proposed configuration files before implementation.

- [ ] **Step 3: Implement the minimum behavior**

Create the ESP-IDF project skeleton and a minimal `app_main` that prints a stable boot banner and memory diagnostics. Set target ESP32-S3 and 16 MB flash; choose PSRAM and flash mode/frequency only after reconciling the successful board log (which reported DIO boot and 8 MB PSRAM) with the exact build configuration. Do not initialize camera, I2S, UART modem, Wi-Fi, or PPP in this task. Add an explicit compile-time flag that keeps all hardware services disabled by default. The partition table must reserve adequate application space and must be checked against the merged-image offsets before release.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_board_config.py -q`

Expected: all board-configuration and no-secret assertions pass.

- [ ] **Step 5: Run the affected integration check**

Run in an ESP-IDF v5.3.2 environment: `cd firmware/esp32_s3 && idf.py build`

Expected: a successful ESP32-S3 build and a generated application image. This is a compile check only; no claim of physical boot is made. Cloud execution requires separate cost/quota approval.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/CMakeLists.txt firmware/esp32_s3/main firmware/esp32_s3/components/board_config firmware/esp32_s3/sdkconfig.defaults firmware/esp32_s3/partitions.csv firmware/esp32_s3/tests/host/test_board_config.py firmware/esp32_s3/README.md
git commit -m "build: add reproducible ESP-IDF baseline"
```

### Task 2: OV3660 capture service

**Files:**
- Create: `firmware/esp32_s3/components/camera_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/camera_service/include/camera_service.h`
- Create: `firmware/esp32_s3/components/camera_service/camera_service.c`
- Create: `firmware/esp32_s3/tests/host/test_camera_frame_contract.py`
- Modify: `firmware/esp32_s3/components/board_config/include/board_config.h`
- Modify: `firmware/esp32_s3/main/app_main.c`

**Interfaces:**
- Consumes: `board_config.h` and the baseline diagnostics.
- Produces: `camera_service_init`, `camera_service_capture_jpeg`, `camera_service_release`, and `camera_frame_t` containing data pointer, byte length, dimensions, format, and ownership state.

- [ ] **Step 1: Add the focused failing test**

Test that capture output cannot be treated as valid when the data pointer is null, length is zero, or format is not JPEG; test that every successful frame has a matching release operation in the API contract; test that a failed capture leaves the output frame empty.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_camera_frame_contract.py -q`

Expected: tests fail because the camera frame contract and service do not yet exist.

- [ ] **Step 3: Implement the minimum behavior**

Use a pinned ESP camera driver/component version compatible with ESP-IDF v5.3.2 and OV3660. Isolate the provisional GPIO map in board configuration and refuse initialization unless an explicit build-time hardware-map confirmation flag is enabled. Capture JPEG into PSRAM, return explicit frame metadata, and guarantee frame-buffer return on every failure and release path. Start with one-shot capture and bounded timeouts; do not implement public HTTP streaming or Wi-Fi AP endpoints.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_camera_frame_contract.py -q`

Expected: all frame validity, empty-on-failure, and ownership-contract tests pass.

- [ ] **Step 5: Run the affected integration check**

Run: `cd firmware/esp32_s3 && idf.py build`

Expected: the camera-enabled target compiles with the pinned driver. Physical acceptance remains a separate bench gate: confirm exact pin map, OV3660 sensor identity, JPEG dimensions/size, repeated capture stability, and no PSRAM corruption.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/camera_service firmware/esp32_s3/components/board_config/include/board_config.h firmware/esp32_s3/main/app_main.c firmware/esp32_s3/tests/host/test_camera_frame_contract.py
git commit -m "feat: add OV3660 snapshot capture service"
```

### Task 3: Independent I2S microphone and speaker services

**Files:**
- Create: `firmware/esp32_s3/components/audio_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/audio_service/include/audio_service.h`
- Create: `firmware/esp32_s3/components/audio_service/audio_service.c`
- Create: `firmware/esp32_s3/tests/host/test_audio_format.py`
- Modify: `firmware/esp32_s3/components/board_config/include/board_config.h`
- Modify: `firmware/esp32_s3/main/app_main.c`

**Interfaces:**
- Consumes: `board_config.h` and ESP-IDF I2S standard driver.
- Produces: `audio_service_init`, `audio_service_read_pcm`, `audio_service_write_pcm`, and diagnostic counters for sample rate, slot width, peak, RMS, clipping, timeouts, and I/O errors.

- [ ] **Step 1: Add the focused failing test**

Add tests for signed sample conversion, buffer-capacity bounds, partial I2S reads/writes, zero-length requests, timeout reporting, and clipping-count calculation. Test both the documented 32-bit microphone input path and 16-bit speaker output path without assuming an unverified microphone bit alignment.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_audio_format.py -q`

Expected: tests fail because the audio conversion and bounded-buffer helpers do not yet exist.

- [ ] **Step 3: Implement the minimum behavior**

Use I2S0 RX on GPIO38/39/40 and I2S1 TX on GPIO1/2/21 as documented wiring candidates. Keep microphone RX and speaker TX independently startable and stop-able. Make microphone slot/channel, data alignment, sample width, and sample rate explicit configuration, not hidden assumptions. Start with a quiet test tone and PCM diagnostics; do not route live microphone input to the speaker. Do not increase gain to disguise a bad capture format. Bound DMA buffers and task stacks, handle timeouts/partial transfers, and report clipping and read/write errors.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_audio_format.py -q`

Expected: all sample conversion, bounds, and diagnostic calculation tests pass.

- [ ] **Step 5: Run the affected integration check**

Run: `cd firmware/esp32_s3 && idf.py build`

Expected: the audio-enabled target compiles. Physical acceptance requires confirming the microphone's exact model and its datasheet format, capturing a short PCM sample for inspection, checking noise/clipping, and verifying quiet speaker playback without resets.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/audio_service firmware/esp32_s3/components/board_config/include/board_config.h firmware/esp32_s3/main/app_main.c firmware/esp32_s3/tests/host/test_audio_format.py
git commit -m "feat: add independent I2S audio services"
```

### Task 4: A7670E UART and AT command service

**Files:**
- Create: `firmware/esp32_s3/components/modem_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/modem_service/include/modem_service.h`
- Create: `firmware/esp32_s3/components/modem_service/modem_service.c`
- Create: `firmware/esp32_s3/tests/host/test_at_parser.py`
- Modify: `firmware/esp32_s3/components/board_config/include/board_config.h`
- Modify: `firmware/esp32_s3/main/app_main.c`

**Interfaces:**
- Consumes: UART driver, confirmed GPIO47/48 signal direction, and explicit UART/power compatibility approval.
- Produces: `modem_service_init`, `modem_service_command`, parsed AT response/status structures, and registration/SIM state reporting.

- [ ] **Step 1: Add the focused failing test**

Test AT response parsing for `OK`, `ERROR`, echo lines, multi-line responses, empty responses, truncated input, and timeout. Confirm that parser output never logs or persists raw SIM identifiers or credentials.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_at_parser.py -q`

Expected: tests fail because the parser and modem state model do not yet exist.

- [ ] **Step 3: Implement the minimum behavior**

Implement a bounded UART transport and serialized AT command state machine with explicit response timeout and maximum response length. Keep modem initialization disabled by default and gate it on a compile-time hardware-confirmed flag. Do not toggle modem power, transmit AT commands, or attempt SIM registration until carrier-board supply range/regulator/current capability and UART logic levels/level shifting are verified from the exact board documentation. APN and SIM PIN values must come from runtime provisioning and must never be printed.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_at_parser.py -q`

Expected: parser and state-transition tests pass for success, modem error, malformed/truncated response, and timeout cases.

- [ ] **Step 5: Run the affected integration check**

Run: `cd firmware/esp32_s3 && idf.py build`

Expected: modem service compiles while remaining disabled by default. Hardware acceptance is allowed only after the electrical gate is documented; then verify UART AT response, SIM readiness, and registration state without exposing identifiers in logs.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/modem_service firmware/esp32_s3/components/board_config/include/board_config.h firmware/esp32_s3/main/app_main.c firmware/esp32_s3/tests/host/test_at_parser.py
git commit -m "feat: add gated A7670E AT service"
```

### Task 5: PPPoS cellular data session

**Files:**
- Create: `firmware/esp32_s3/components/network_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/network_service/include/network_service.h`
- Create: `firmware/esp32_s3/components/network_service/network_service.c`
- Create: `firmware/esp32_s3/tests/host/test_network_state.py`
- Modify: `firmware/esp32_s3/main/app_main.c`
- Modify: `firmware/esp32_s3/README.md`

**Interfaces:**
- Consumes: modem UART transport and AT command state model from Task 4.
- Produces: `network_service_start`, `network_service_is_ready`, and explicit states for stopped, starting, connected, reconnecting, and failed.

- [ ] **Step 1: Add the focused failing test**

Test state transitions for successful connect, registration loss, PPP disconnect, reconnect timeout, repeated failure, and explicit stop. Assert bounded retry delay and that network readiness is false until PPP reports an established session.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_state.py -q`

Expected: tests fail because network state transitions are not yet implemented.

- [ ] **Step 3: Implement the minimum behavior**

Check the official component registry/source compatibility for a specific `esp_modem` version with ESP-IDF v5.3.2 before adding it; pin the selected version and its transitive dependencies. Implement PPPoS startup/stop, DNS readiness, bounded reconnect backoff, and clear status events. Do not add an unpinned dependency or embed APN credentials. Keep cellular networking behind the same hardware-confirmed gate as the modem service.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_state.py -q`

Expected: all state transition and retry-bound tests pass.

- [ ] **Step 5: Run the affected integration check**

Run: `cd firmware/esp32_s3 && idf.py build`

Expected: pinned PPPoS dependency resolves and the ESP32-S3 image compiles. Hardware acceptance requires documented SIM readiness, registered network state, PPP session, DNS resolution, and stable reconnect behavior. No online transfer is claimed at this stage.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/network_service firmware/esp32_s3/tests/host/test_network_state.py firmware/esp32_s3/main/app_main.c firmware/esp32_s3/README.md firmware/esp32_s3/idf_component.yml
git commit -m "feat: add gated A7670E PPPoS networking"
```

### Task 6: Authenticated backend snapshot and status client

**Files:**
- Create: `firmware/esp32_s3/components/backend_client/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/backend_client/include/backend_client.h`
- Create: `firmware/esp32_s3/components/backend_client/backend_client.c`
- Create: `firmware/esp32_s3/tests/host/test_backend_request.py`
- Modify: `firmware/esp32_s3/main/app_main.c`
- Modify: `firmware/esp32_s3/README.md`
- Modify: `docs/architecture.md`

**Interfaces:**
- Consumes: `camera_frame_t`, `device_status_t`, `network_service_is_ready`, and runtime-provisioned backend identity/credentials.
- Produces: `backend_client_upload_snapshot`, `backend_client_send_status`, and `backend_result_t` with HTTP status, bounded response summary, and transport error.

- [ ] **Step 1: Add the focused failing test**

Test request construction for JPEG content type and declared length, authorization header presence without credential leakage, rejection when network is not ready, bounded response handling, TLS certificate verification configuration, and cleanup after timeout or failed transfer.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_backend_request.py -q`

Expected: tests fail because backend request construction and result types do not yet exist.

- [ ] **Step 3: Implement the minimum behavior**

Use ESP-IDF HTTPS client with certificate verification enabled. Accept only a configured HTTPS backend origin and runtime-provisioned authentication material. Upload one explicitly requested JPEG snapshot and a small status payload; enforce request/response size limits and timeouts. Return the camera frame to its owner on every completion path. Do not expose camera endpoints on the device, auto-upload continuously, include AI provider keys in the device, or log bearer tokens/identifiers.

- [ ] **Step 4: Verify the focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_backend_request.py -q`

Expected: request construction, TLS policy, response bounds, and cleanup tests pass.

- [ ] **Step 5: Run the affected integration check**

Run: `cd firmware/esp32_s3 && idf.py build`

Expected: complete firmware compiles with all feature modules gated by explicit configuration. End-to-end acceptance requires a controlled backend test with valid TLS, authenticated snapshot upload, status response, rejected unauthenticated request, and confirmation that no secrets appear in serial logs.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/backend_client firmware/esp32_s3/tests/host/test_backend_request.py firmware/esp32_s3/main/app_main.c firmware/esp32_s3/README.md docs/architecture.md
git commit -m "feat: add authenticated backend snapshot client"
```

---

## Cross-task acceptance

A release candidate is not complete until all of the following evidence exists:
1. Reproducible ESP-IDF v5.3.2 build for ESP32-S3-N16R8, with recorded flash/PSRAM configuration and verified partition/merged-image offsets.
2. OV3660 identity and repeated JPEG capture verified on the exact board.
3. Microphone model and I2S format confirmed; captured PCM inspected; quiet MAX98357A playback stable.
4. A7670E carrier-board electrical specifications and UART logic levels documented before modem power-up; AT, SIM, and registration states verified.
5. Pinned PPPoS component builds and a real cellular data session passes DNS/TLS checks.
6. Authenticated snapshot/status transfer succeeds through the backend; unauthenticated access is rejected.
7. No secrets are present in Git history, firmware defaults, artifacts, or serial logs.
8. The integrated image is not flashed until the user approves the hardware test after the relevant gates and recovery method are explained.

## Unresolved product and hardware decisions

1. What exact ESP32-S3 board manufacturer/revision and schematic confirm the OV3660 pin map?
2. What is the exact microphone model/IC marking, and what does its datasheet specify for I2S slot/channel and data alignment?
3. What is the exact A7670E carrier-board model and its specified input voltage/current capability and UART logic voltage?
4. Which backend URL and device-authentication/provisioning mechanism will be used? This must be supplied without placing secrets in the repository.
5. Which exact `esp_modem` release is compatible with ESP-IDF v5.3.2 and the chosen A7670E PPP mode? Confirm with upstream release notes and a build before pinning.
6. Which final flash mode/frequency and PSRAM frequency match the physical board's successful diagnostic boot and remain stable under camera/audio memory load?
