# Ravatronix Dual-Network Transport Implementation Plan

> **For agentic workers:** Use the host's available task-by-task implementation workflow. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a transport-neutral voice request path that can use either ESP32 Wi-Fi or the A7670E internal AT HTTP(S) stack and automatically select the healthiest route.

**Architecture:** A pure-C network policy module selects between independent Wi-Fi and modem adapters using gateway health, latency hysteresis and failover state. Wi-Fi uses ESP-IDF's station/netif/socket stack; the A7670E adapter owns its UART and uses only documented modem HTTP(S) AT capabilities after runtime capability checks. The voice client consumes one bounded streaming transport interface and never retries an ambiguous POST without gateway idempotency support.

**Tech Stack:** C11, ESP-IDF v5.3.2, FreeRTOS, ESP-IDF Wi-Fi/netif/event APIs, ESP-IDF HTTP client/TLS bundle, SIMCom A76XX AT commands, Python 3.11/pytest, host GCC, GitHub Actions.

## Global Constraints

- All changes stay on `feat/rava-ai-voice-unified-firmware`; `main` remains unchanged.
- Preserve existing camera, INMP441 microphone, MAX98357A audio, WAV codec and voice request behavior.
- The A7670E uses its internal AT data stack, not PPP. Do not switch it to PPP implicitly.
- Wi-Fi and modem are independent routes; automatic selection requires verified gateway reachability, not just Wi-Fi association or cellular registration.
- HTTPS certificate verification remains enabled. Never log tokens, credentials, Authorization headers, SIM identifiers or audio payloads.
- Wi-Fi credentials and device token are runtime-provisioned and never committed.
- Bound all buffers, request/response sizes, timeouts, retries and health probes.
- An in-flight POST must not be replayed automatically after ambiguous failure unless the gateway implements and verifies idempotency keys.
- Keep camera/audio startup independent from network availability.
- Keep modem startup behind electrical/UART confirmation gates until the board wiring is verified.
- Do not flash hardware during repository implementation.
- A successful CI build is not proof of physical modem/Wi-Fi operation.

---

### Task 1: Add executable host tests for network policy and AT parsing

**Files:**
- Create: `firmware/esp32_s3/tests/host/test_network_policy.py`
- Create: `firmware/esp32_s3/tests/host/test_modem_at_parser_contract.py`
- Create: `firmware/esp32_s3/tests/host/native_test_support.py`
- Modify: `firmware/esp32_s3/tests/host/test_board_config.py` only when new Kconfig safety gates are introduced.

**Interfaces:**
- Tests target the planned pure-C interface `network_policy_select(...)` and pure-C parser feed/consume API; they compile production C with host GCC in a temporary directory and call it through a small C harness.
- The first red test must fail because the production policy/parser source does not yet exist, not because GCC or pytest setup is broken.

- [ ] **Step 1: Add focused failing tests**

Add deterministic cases:
- No route READY -> `NETWORK_ROUTE_NONE`.
- Only Wi-Fi READY -> Wi-Fi.
- Only modem READY -> modem.
- Both READY -> lower measured gateway latency wins.
- Latency difference inside hysteresis margin -> retain current route.
- Active route DOWN/DEGRADED -> switch to the other READY route.
- Both routes unavailable -> no route.
- An in-flight non-idempotent POST is marked ambiguous and is not replayed.
- AT parser handles fragmented CR/LF lines, `OK`, `ERROR`, `DOWNLOAD` prompt, unsolicited result lines, timeout, and binary-length boundaries without interpreting binary bytes as text.

- [ ] **Step 2: Verify the relevant failure**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_policy.py firmware/esp32_s3/tests/host/test_modem_at_parser_contract.py -q`

Expected: pytest reports the intentionally missing production policy/parser source or symbols. The failure must be an assertion/import/file-not-found tied to missing behavior, not an environment error.

- [ ] **Step 3: Implement minimum pure-C seams**

Create `components/network_manager/include/network_policy.h` and `components/network_manager/network_policy.c` with a deterministic route-selection function that accepts current route, per-route state, measured latency, hysteresis margin and dwell eligibility. Create a separate parser module with bounded line parsing and explicit raw-data mode boundaries; keep ESP-IDF dependencies out of these pure-C modules.

- [ ] **Step 4: Verify focused pass**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_policy.py firmware/esp32_s3/tests/host/test_modem_at_parser_contract.py -q`

Expected: all focused policy/parser tests pass against compiled production C.

- [ ] **Step 5: Run affected integration check**

Run: `python -m pytest firmware/esp32_s3/tests/host -q`

Expected: existing board and voice contract tests plus new host tests pass.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/network_manager firmware/esp32_s3/tests/host
git commit -m "test: add host-tested network policy and modem parser"
```

### Task 2: Introduce transport-neutral streaming HTTP interface and migrate voice client

**Files:**
- Create: `firmware/esp32_s3/components/network_transport/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/network_transport/include/network_transport.h`
- Create: `firmware/esp32_s3/components/network_transport/network_transport.c`
- Modify: `firmware/esp32_s3/components/voice_client/voice_client.c`
- Modify: `firmware/esp32_s3/components/voice_client/CMakeLists.txt`
- Create: `firmware/esp32_s3/tests/host/test_network_transport_contract.py`
- Modify: `firmware/esp32_s3/tests/host/test_voice_client_contract.py`

**Interfaces:**
- Define a request descriptor with HTTPS URL, bounded headers, content length and timeout.
- Define adapter callbacks for begin, write, finish/execute, read, status, abort and cleanup.
- Expose one `network_transport_perform_stream(...)` contract to the voice client.
- The transport manager chooses one READY adapter before request start; the selected adapter remains pinned for that request.

- [ ] **Step 1: Add failing contract tests**

Assert that voice client no longer directly initializes or writes through `esp_http_client`, that WAV request bytes and Authorization/Content-Type headers are passed through the transport interface, and that response bytes/status are consumed through bounded reads. Add failure cases for partial write, timeout, non-200 status, truncated response and cleanup.

- [ ] **Step 2: Verify red**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_transport_contract.py firmware/esp32_s3/tests/host/test_voice_client_contract.py -q`

Expected: new assertions fail because voice client still owns `esp_http_client` directly.

- [ ] **Step 3: Implement minimum transport seam**

Add the transport API and a Wi-Fi/ESP-IDF HTTP adapter placeholder registration seam. Migrate voice client to call the common interface while preserving capture, WAV formatting, status checks, response bounds and playback. Do not implement hidden retries.

- [ ] **Step 4: Verify focused pass**

Run the same focused pytest command.

Expected: all transport and voice contract assertions pass; no credential or API key literal is introduced.

- [ ] **Step 5: Run affected integration check**

Run: `python -m pytest firmware/esp32_s3/tests/host -q`

Expected: all host tests pass.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/network_transport firmware/esp32_s3/components/voice_client firmware/esp32_s3/tests/host
git commit -m "refactor: route voice requests through network transport"
```

### Task 3: Implement Wi-Fi station, provisioning and health reporting

**Files:**
- Create: `firmware/esp32_s3/components/wifi_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/wifi_service/idf_component.yml` only if a managed component dependency is required.
- Create: `firmware/esp32_s3/components/wifi_service/include/wifi_service.h`
- Create: `firmware/esp32_s3/components/wifi_service/wifi_service.c`
- Create: `firmware/esp32_s3/components/wifi_service/wifi_provisioning.c`
- Modify: `firmware/esp32_s3/sdkconfig.defaults`
- Modify: `firmware/esp32_s3/main/CMakeLists.txt`
- Modify: `firmware/esp32_s3/tests/host/test_wifi_service_contract.py`

**Interfaces:**
- `wifi_service_start()`, `wifi_service_get_state()`, `wifi_service_get_probe_metrics()`, and event callback to network manager.
- Provisioned credentials are loaded from NVS; no compile-time SSID/password.
- Wi-Fi transport uses ESP-IDF socket/TLS facilities and the existing certificate bundle.

- [ ] **Step 1: Add failing tests**

Assert station mode is used, credentials are read from NVS rather than literals, IP acquisition is required before READY, disconnect/IP loss moves route out of READY, reconnect can restore READY, and provisioning mode is bounded by explicit timeout and does not log credentials.

- [ ] **Step 2: Verify red**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_wifi_service_contract.py -q`

Expected: tests fail because the Wi-Fi component and lifecycle do not exist.

- [ ] **Step 3: Implement Wi-Fi lifecycle**

Initialize shared netif/event infrastructure once in a network bootstrap, create default STA netif, initialize Wi-Fi driver, load NVS credentials, connect in STA mode, and publish event-driven state. Add a minimal temporary SoftAP setup portal for first-time credential entry; close it after save or timeout. Avoid a captive portal dependency unless the repository can support it without adding a large unmanaged dependency.

- [ ] **Step 4: Verify focused pass**

Run the focused Wi-Fi pytest command.

Expected: source-contract and state-transition tests pass.

- [ ] **Step 5: Run affected integration check**

Run: `python -m pytest firmware/esp32_s3/tests/host -q`

Expected: all host tests pass.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/wifi_service firmware/esp32_s3/main/CMakeLists.txt firmware/esp32_s3/sdkconfig.defaults firmware/esp32_s3/tests/host
git commit -m "feat: add Wi-Fi station and provisioning service"
```

### Task 4: Implement A7670E AT HTTP(S) adapter with strict capability gates

**Files:**
- Create: `firmware/esp32_s3/components/modem_at_service/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/modem_at_service/include/modem_at_service.h`
- Create: `firmware/esp32_s3/components/modem_at_service/modem_at_service.c`
- Create: `firmware/esp32_s3/components/modem_at_service/modem_http_transport.c`
- Modify: `firmware/esp32_s3/components/board_config/include/board_config.h` only to centralize existing UART configuration; do not mark pins electrically confirmed.
- Modify: `firmware/esp32_s3/sdkconfig.defaults`
- Create: `firmware/esp32_s3/tests/host/test_modem_transport_contract.py`

**Interfaces:**
- `modem_at_service_start()`, `modem_at_service_get_state()`, `modem_at_service_get_firmware_info()`, and transport adapter registration.
- One UART owner, serialized AT command engine, parser isolated from FreeRTOS/UART code.
- HTTP(S) operations are enabled only when firmware capability checks confirm HTTPS, Authorization header, binary body/response handling and configured payload limits.

- [ ] **Step 1: Add failing tests**

Assert UART ownership is exclusive, commands are serialized, fragmented responses are assembled, prompt/data mode transitions are respected, unsolicited events do not corrupt transactions, and unsupported modem capability leaves adapter DISABLED. Assert that PPP startup is not called and TLS verification is not disabled.

- [ ] **Step 2: Verify red**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_modem_transport_contract.py -q`

Expected: focused tests fail because the AT modem adapter does not exist.

- [ ] **Step 3: Implement AT service and HTTP adapter**

Use SIMCom A76XX documented HTTP(S) command sequence only after validating the installed A7670E firmware revision. Support `HTTPINIT`, `HTTPPARA` URL/content/custom headers/SSL context, `HTTPDATA` binary upload, `HTTPACTION` POST, `HTTPHEAD`, and bounded `HTTPREAD` response retrieval only where confirmed in the actual modem manual/revision. Configure SSL context with certificate validation; if modem certificate validation cannot be configured and verified, keep HTTPS adapter disabled. Do not print AT command arguments that contain credentials. Never fall back to PPP.

- [ ] **Step 4: Verify focused pass**

Run the focused modem pytest command.

Expected: parser and modem transport contract tests pass; unsupported capability paths fail closed.

- [ ] **Step 5: Run affected integration check**

Run: `python -m pytest firmware/esp32_s3/tests/host -q`

Expected: all host tests pass.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3/components/modem_at_service firmware/esp32_s3/components/board_config firmware/esp32_s3/sdkconfig.defaults firmware/esp32_s3/tests/host
git commit -m "feat: add gated A7670E AT HTTPS transport"
```

### Task 5: Integrate automatic selection, end-to-end checks and firmware artifact

**Files:**
- Create: `firmware/esp32_s3/components/network_manager/CMakeLists.txt`
- Create: `firmware/esp32_s3/components/network_manager/network_manager.c`
- Create: `firmware/esp32_s3/components/network_manager/include/network_manager.h`
- Modify: `firmware/esp32_s3/components/network_transport/network_transport.c`
- Modify: `firmware/esp32_s3/main/app_main.c`
- Modify: `firmware/esp32_s3/main/CMakeLists.txt`
- Modify: `firmware/esp32_s3/tests/host/test_network_manager_integration.py`
- Modify: `.github/workflows/build-ravatronix-idf.yml` only if additional verification/artifact checks are needed.

**Interfaces:**
- `network_manager_start()`, `network_manager_get_active_route()`, `network_manager_get_route_snapshot()`, and `network_manager_select_transport()`.
- Both adapters publish READY only after gateway health probe success.
- Voice transport is pinned per request and does not replay ambiguous POSTs.

- [ ] **Step 1: Add failing integration tests**

Assert both adapters register independently, health probes—not raw link state—gate READY, automatic selection follows policy/hysteresis, failover selects the remaining READY route, no-route is reported cleanly, voice requests remain pinned to one adapter, and camera/audio startup continues when network startup fails.

- [ ] **Step 2: Verify red**

Run: `python -m pytest firmware/esp32_s3/tests/host/test_network_manager_integration.py -q`

Expected: tests fail because the network manager integration is absent.

- [ ] **Step 3: Implement integration**

Start shared networking bootstrap once, start Wi-Fi service and gated modem AT service, register adapters, run bounded gateway health probes, and connect network manager to transport. Keep camera/audio boot independent. Remove or isolate legacy PPP startup so it cannot be accidentally selected by the default build.

- [ ] **Step 4: Verify focused pass**

Run the focused network manager pytest command.

Expected: integration contract tests pass.

- [ ] **Step 5: Run complete host and firmware build checks**

Run: `python -m pytest firmware/esp32_s3/tests/host -q`

Expected: all host tests pass.

Run: repository GitHub Actions workflow `Build Ravatronix ESP-IDF firmware` on `feat/rava-ai-voice-unified-firmware`.

Expected: host tests pass, ESP-IDF v5.3.2 build and merge-bin succeed, merged binary and `flasher_args.json` exist, and artifact upload succeeds. Inspect job logs and artifact metadata before reporting.

- [ ] **Step 6: Commit the passing deliverable**

```bash
git add firmware/esp32_s3 .github/workflows/build-ravatronix-idf.yml
git commit -m "feat: add automatic Wi-Fi and A7670E network selection"
```

## Unresolved verification facts

- The installed A7670E firmware revision and exact supported AT HTTP(S)/SSL capabilities must be read from the physical modem before enabling the modem transport.
- GPIO48/47 UART mapping and electrical levels must be physically confirmed before opening the modem gate.
- Maximum modem request-body size, response-body size and reliable binary transfer behavior must be proven with the target WAV sizes.
- Live Wi-Fi provisioning, modem HTTPS, gateway reachability and voice playback require hardware tests; GitHub CI cannot prove them.
- Android-app integration for Wi-Fi provisioning is a later milestone; this firmware phase provides standalone SoftAP provisioning.
