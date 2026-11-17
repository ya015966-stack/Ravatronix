# Ravatronix Dual-Network Transport Design

**Date:** 2026-11-17  
**Status:** Approved architecture; implementation pending  
**Target branch:** `feat/rava-ai-voice-unified-firmware`

## Goal

Allow the ESP32-S3 firmware to reach the Ravatronix AI gateway over either Wi-Fi or the A7670E modem's internal AT-command data stack, choosing the healthiest usable route automatically without changing the voice-client contract or requiring PPP.

## Repository findings

The current firmware has the following relevant behavior:

- `components/network_service/network_service.c` implements an ESP-Modem PPP path only. Cellular startup is gated by `CONFIG_RAVA_ENABLE_CELLULAR` and `CONFIG_RAVA_MODEM_ELECTRICAL_CONFIRMED`.
- `components/network_service/include/network_service.h` exposes only `network_service_start()` and `network_service_is_ready()`.
- `components/voice_client/voice_client.c` uses `esp_http_client` directly for HTTPS streaming upload and response download. This works only when the ESP-IDF TCP/IP stack has a usable route.
- `main/app_main.c` starts camera and audio services, and conditionally starts the current PPP service. It does not initialize Wi-Fi.
- `components/board_config/include/board_config.h` records modem UART1 TX GPIO48, RX GPIO47, 115200 baud, but explicitly describes hardware mappings as provisional and not electrically verified.
- `sdkconfig.defaults` enables camera/audio and leaves cellular disabled.

The user has confirmed that the actual A7670E internet path is the modem's internal AT data stack, not PPP. Therefore the implementation must not treat PPP as the modem transport.

## Architecture

### 1. Transport boundary

Introduce a transport-neutral request interface used by `voice_client`. The public contract must support:

- HTTPS POST to the configured gateway URL.
- Request headers including Content-Type and Authorization.
- A known request body length and bounded streaming writes.
- Bounded streaming response reads.
- HTTP status reporting and explicit timeout/error results.
- Cancellation/cleanup that leaves the modem UART and Wi-Fi driver in a known state.

The voice layer remains responsible for WAV capture, WAV validation, and audio playback. It must not know whether Wi-Fi or the modem carried the request.

### 2. Wi-Fi transport

Add a Wi-Fi station service using ESP-IDF 5.3 APIs:

- Initialize NVS, netif, default event loop (with idempotent handling where already initialized), Wi-Fi driver, and default station netif in a single coordinated network bootstrap.
- Read credentials from provisioned NVS configuration; never embed credentials in source, build logs, or repository files.
- Connect in station mode and track association, IP acquisition, disconnect, and reconnect states.
- Use the normal ESP-IDF socket/HTTP stack for HTTPS and certificate verification.
- Provide a provisioning mode through a temporary SoftAP/local setup portal only when no usable Wi-Fi credentials exist. The portal must not expose stored credentials after setup and must close after successful provisioning or timeout.
- Avoid reinitializing global ESP-IDF netif/event infrastructure independently in multiple services.

### 3. A7670E AT transport

Add a modem AT service which owns UART1 exclusively while active. It must not enter PPP mode.

SIMCom's A7670X product page publishes the A76XX AT Command Manual and HTTP(S), SSL, TCP/IP and UART application notes. The published A76XX HTTP(S) command family includes HTTP service initialization, parameter configuration, request action, body upload, response-header and response-body reading. These commands are candidate implementation primitives, not assumed to be supported identically by every installed modem firmware revision.

Implementation requirements:

- Query and log non-secret modem identity/firmware information during diagnostic startup, without logging SIM identifiers or credentials.
- Use a serialized AT transaction engine with bounded command/response timeouts, explicit prompt/data mode handling, and robust parsing of unsolicited result codes.
- Use the modem's native HTTPS client path only after confirming the installed firmware supports the required HTTPS, custom Authorization header, binary request-body size, and binary response-body size.
- Verify TLS certificate validation behavior against the installed modem firmware; do not silently disable certificate verification.
- Stream binary WAV data in bounded chunks; do not assume line-oriented text parsing can safely carry arbitrary binary payload.
- Recover cleanly from ERROR, timeout, modem reset, partial upload, and response truncation.
- Do not fall back to PPP as an implicit workaround.
- Keep modem startup disabled behind explicit Kconfig electrical/UART confirmation gates until physical wiring and UART operation are verified.

If the modem's native HTTP(S) AT interface cannot meet the binary payload, header, or TLS requirements, the AT adapter must fail closed and report a capability error. A separately reviewed TCP/TLS socket adapter may be evaluated later; it must not be silently substituted.

### 4. Automatic network manager

The manager maintains independent link state for Wi-Fi and A7670E:

- `DOWN`: no link or no data service.
- `LINK_UP`: association/registration exists, but gateway reachability has not been proven.
- `READY`: a bounded health probe to the configured gateway succeeds.
- `DEGRADED`: link exists but probe or recent request health fails.
- `DISABLED`: configuration or safety gate prevents startup.

Selection policy:

1. Prefer a route in READY state.
2. If both are READY, select the route with the better recent end-to-end gateway probe latency, using a stability margin and minimum dwell interval to prevent flapping.
3. If the selected route becomes DEGRADED or DOWN, select the other READY route.
4. If neither is READY, report network unavailable and do not attempt an AI request.
5. Do not switch transports in the middle of a non-idempotent POST and replay it automatically. A failed request may have reached the gateway. Request retry requires an explicit idempotency key supported and enforced by the gateway.
6. Keep per-route metrics bounded and avoid excessive health probes or data usage.

Health checks must use a lightweight endpoint such as the gateway health route, with bounded timeout and response validation. A DNS lookup or modem registration alone is not proof of gateway reachability.

### 5. Credentials and provisioning

- Wi-Fi SSID/password are provisioned locally and stored in NVS using ESP-IDF-supported secure storage options where available.
- Gateway URL and device token are runtime configuration, not source literals.
- Device token must never be printed in logs or committed to Git.
- AI provider API keys remain on the backend and never enter firmware or Android.
- The temporary Wi-Fi provisioning portal is a setup interface, not an Internet gateway.

### 6. Startup and integration

Network bootstrap must be called once from `app_main.c` after NVS initialization and before any voice request can be accepted. Camera and audio initialization remain independent and retain their existing gates. Network startup failure must not prevent camera/audio boot; it must leave the network manager in a diagnosable unavailable state and continue retrying according to bounded backoff.

The old PPP implementation must not be activated by the new default configuration. Preserve it only if it remains useful for a separately gated future board variant; otherwise remove dead PPP dependencies as part of a separately tested change, not as incidental cleanup.

## Proposed module boundaries

Observed paths:

- `firmware/esp32_s3/components/network_service/network_service.c`
- `firmware/esp32_s3/components/network_service/include/network_service.h`
- `firmware/esp32_s3/components/voice_client/voice_client.c`
- `firmware/esp32_s3/main/app_main.c`
- `firmware/esp32_s3/components/board_config/include/board_config.h`
- `firmware/esp32_s3/sdkconfig.defaults`

Proposed paths (subject to existing component conventions):

- `components/network_manager/`: route state machine, health checks, selection and failover.
- `components/wifi_service/`: Wi-Fi STA and provisioning lifecycle.
- `components/modem_at_service/`: UART AT parser, command engine and A7670E HTTP(S) adapter.
- `components/network_transport/`: common request/response streaming interface.
- `tests/host/`: deterministic state-machine, parser, transport-contract and regression tests.

## Test strategy and acceptance criteria

### Host tests before production implementation

- Route selection chooses the only READY route.
- When both routes are READY, lower stable probe latency wins.
- Hysteresis/dwell prevents oscillation when measurements are close.
- DOWN/DEGRADED active route causes selection of the other READY route.
- Neither READY returns unavailable.
- In-flight POST is never automatically replayed on route change.
- AT parser handles fragmented lines, CR/LF, prompts, unsolicited result codes, ERROR, timeout, and binary payload boundaries.
- Transport contract preserves request bytes, headers, status and response bytes across both adapters.
- Wi-Fi event state transitions handle disconnect/reconnect and IP loss/acquisition.
- Existing camera/audio/voice contract tests remain green.

### Build and integration checks

- Run focused host tests and observe expected red failures before implementing each behavior.
- Run the complete host test suite after each integrated subsystem.
- Build the ESP32-S3 firmware using the repository's ESP-IDF v5.3.2 GitHub Actions workflow.
- Inspect the complete job log, merged binary, flasher metadata, artifact contents and file sizes.
- Confirm that the default build keeps the modem electrical confirmation gate closed unless separately validated.
- Confirm the firmware starts camera/audio even if neither network is available.

### Hardware acceptance (not provable by CI)

- Verify modem UART wiring and actual AT response with the board powered safely.
- Capture the exact modem firmware revision and confirm supported HTTP(S)/TLS/body-size/header behavior.
- Provision Wi-Fi and verify a real gateway health check.
- Verify A7670E AT HTTPS gateway health check.
- Test Wi-Fi-only, modem-only, both-ready selection, and forced link loss/failover.
- Complete a voice request over each transport and verify returned WAV playback.
- Verify no duplicate AI turn occurs after ambiguous transport failure.

A successful CI build is not a substitute for these physical checks. Do not flash the board or enable the modem electrical gate until hardware evidence is available.

## Security and operational constraints

- Never commit Wi-Fi credentials, device tokens, backend API keys, SIM secrets, or private user data.
- Never log Authorization headers or binary audio.
- Keep HTTPS certificate verification enabled.
- Bound request/response memory and UART buffers; allocate large WAV buffers from PSRAM where available.
- Apply timeouts to every AT transaction, health probe, and HTTP operation.
- Keep all changes on `feat/rava-ai-voice-unified-firmware`; do not modify `main`.
- Do not flash hardware as part of repository implementation.
- Produce a downloadable GitHub Actions artifact only after build and artifact inspection succeed.

## References

- ESP-IDF v5.3 Wi-Fi station lifecycle: https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/api-guides/wifi.html
- ESP-IDF v5.3 HTTP client and HTTPS certificate bundle: https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32/api-reference/protocols/esp_http_client.html
- SIMCom A7670X product page and official A76XX manuals/application notes: https://www.simcom.com/product/A7670X.html
- SIMCom A76XX technical file index: https://www.simcom.com/technical_files-p92.html

## A7670E payload-size and TLS findings

The A76XX Series AT Command Manual V1.09 describes two possible data paths that must be capability-probed on the installed A7670E firmware:

- Direct `AT+HTTPDATA` upload has a documented body-size range of 1 to 153,600 bytes. This is smaller than an 8-second PCM16 mono 16 kHz WAV request (about 256 KiB plus the WAV header).
- The manual documents `AT+HTTPPOSTFILE` and `AT+HTTPREADFILE` for file-based HTTP(S) request/response handling, plus `AT+CFTRANRX` and `AT+CFTRANTX` for transferring files between the host and modem EFS. UART file transfer is documented as potentially supporting larger files, but the manual warns that practical size and pacing depend on system resources.
- The manual notes a 64 KiB protocol-stack/cache constraint for large HTTP responses in the ordinary read mode; the file-based response path is therefore preferred for large WAV responses if the installed firmware supports it.
- If the file-based commands are unavailable or fail capability tests, the modem adapter must expose its smaller request/response limits and the route selector must reject payloads that exceed them. It must not truncate or silently retry over another transport after an ambiguous POST.
- SIMCom's SSL configuration distinguishes no authentication from server authentication. Server authentication requires a root CA file; SNI and modem time/certificate validity must also be configured and verified. TLS verification must never be disabled as a workaround.

Preferred modem sequence, only after runtime capability confirmation: transfer request WAV to modem EFS, configure HTTPS URL/content/custom Authorization header and verified SSL context, POST the EFS file, save response to EFS, transfer response bytes back to ESP32, validate WAV, then remove temporary files. Direct HTTPDATA/HTTPREAD is a bounded fallback only when the exact request/response sizes fit the modem's verified limits.

References: SIMCom A76XX AT Command Manual V1.09 sections 13, 16 and 19; official A7670X product downloads page.

## Open verification items

These are hardware/firmware facts to establish during staged verification, not reasons to guess:

- Exact A7670E firmware revision and supported AT HTTP(S) feature set.
- Confirmation that ESP32 GPIO48/47 are the physically connected modem UART pins and that signal levels/power are correct.
- Whether the installed modem firmware can carry the current maximum WAV request and response sizes while preserving binary bytes and TLS verification.
- Final Wi-Fi provisioning UX integration with the Android app; initial SoftAP provisioning is the standalone firmware fallback.
