# RAVA AI Voice v1.0 Implementation Plan

> **For agentic workers:** Use the host's available task-by-task implementation workflow. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the already-working Ravatronix robot to an authenticated online AI gateway and deliver a first Persian push-to-talk voice round trip without replacing its working firmware subsystems.

**Architecture:** Keep ESP32-S3 responsible for local capture/playback and transport; keep provider credentials and AI/STT/TTS calls on the backend. Preserve the existing authenticated WebSocket for small control messages and use authenticated raw HTTPS WAV uploads and audio responses for media. Make the AI, STT, and TTS provider adapters configurable so the provider can be changed without changing firmware.

**Tech Stack:** Node.js 20+, native fetch/FormData/Blob, existing `ws` dependency, Node built-in test runner, ESP-IDF firmware workspace.

## Global Constraints

- Preserve the working camera, microphone, speaker, and network firmware; do not replace or reflash it as part of backend work.
- Never commit API keys, device tokens, Wi-Fi credentials, SIM PINs, or certificates.
- Keep provider credentials exclusively in backend environment variables.
- Require device authentication for device endpoints; do not expose camera, audio, or chat endpoints publicly without authentication.
- WebSocket is for small control messages; do not send audio bytes as base64 JSON.
- First voice version uses push-to-talk, a bounded recording duration, WAV PCM16 mono 16 kHz, and half-duplex playback.
- A failed AI/STT/TTS call must return an explicit error; never fabricate a successful assistant answer.
- Keep all changes on `feat/rava-ai-voice-v1`; do not modify `main` or flash hardware.

---

### Task 1: Add an authenticated text AI gateway

**Files:**
- Modify: `backend/src/server.js`
- Create: `backend/src/ai-provider.js`
- Modify: `backend/test/server.test.js`
- Modify: `backend/package.json` only if a script or dependency is required

**Interfaces:**
- `createRavaServer({ deviceToken, aiProvider, speechProvider })` accepts injected providers for deterministic tests.
- Provider interface: `completeChat({ messages }) -> Promise<string>`.
- HTTP interface: `POST /api/ai/chat`, JSON body `{ "message": "...", "conversation_id": "..." }`, bearer device token; response `{ "reply": "...", "conversation_id": "..." }`.

- [x] Add tests for unauthenticated requests, valid chat request invoking the provider, provider failure returning a non-success response, and absent provider configuration returning a clear unavailable response.
- [x] Run focused tests and observe the missing route returning 404 before implementation.
- [x] Implement an OpenAI-compatible chat-completions adapter using server-only `RAVA_AI_BASE_URL`, `RAVA_AI_API_KEY`, and `RAVA_AI_MODEL` environment variables; use native fetch and a 25-second timeout.
- [x] Verify provider and route tests pass with injected fake providers; API keys are not returned in responses.
- [x] Run backend CI workflow: 17 tests passed, 0 failed.
- [x] Commit the passing task on the feature branch.

### Task 2: Add a bounded Persian speech round-trip service

**Files:**
- Create: `backend/src/voice-service.js`
- Modify: `backend/src/server.js`
- Create: `backend/test/voice.test.js`

**Interfaces:**
- `transcribeAudio({ audio, mimeType }) -> Promise<string>`
- `synthesizeSpeech({ text }) -> Promise<Buffer>`
- `POST /api/voice/turn`: authenticated raw WAV upload; returns a WAV audio response.
- Input contract: WAV PCM16, mono, 16 kHz, maximum 8 seconds and 512 KiB upload.
- Output contract: WAV PCM16, mono, 16 kHz.

- [x] Test unauthenticated requests, malformed WAV, provider unavailability/failure, and a successful injected STT→AI→TTS round trip.
- [x] Observe the missing voice route returning 404 before implementation.
- [ ] Select and implement a production speech-provider adapter behind the tested STT/TTS interfaces; never forward raw audio to the chat provider.
- [x] Return clear 4xx/5xx errors for invalid WAV and provider failures; do not return partial audio.
- [x] Verify backend CI workflow: 17 tests passed, 0 failed.
- [x] Commit the tested voice orchestration on the feature branch.

### Task 3: Integrate the existing ESP32 voice transport

**Files:**
- Modify only observed files under: `firmware/esp32_s3/components/audio_service/`, `backend_client/`, and the existing application entry point after inspection.
- Create tests under the existing host-test convention if a deterministic seam is available.

**Interfaces:**
- Control messages: `voice_start`, `voice_stop`, `voice_processing`, `voice_playback`, `voice_error`.
- Media transport: HTTPS upload of PCM16 WAV and HTTPS download of the WAV response.
- Preserve existing Wi-Fi and A7670E network selection; the voice client consumes the already-established network interface and does not own modem setup.

- [ ] Inspect current component code and exact I2S sample format before changing audio conversion.
- [ ] Add tests for WAV header fields, PCM length, maximum duration, HTTP status handling, and playback state transitions where host tests permit.
- [ ] Observe red tests, implement only the missing transport glue, and preserve current pin assignments.
- [ ] Build with the repository's ESP-IDF v5.3.2 workflow; do not flash the robot.
- [ ] Commit only after firmware build and host tests pass.

## Unresolved product decisions

- The production STT and TTS vendor/model, region, pricing, and data-retention terms must be selected after comparing Persian quality and availability. The implementation should keep these adapters configurable and must not assume a paid provider is approved.
- The exact WAV byte layout produced by the currently working microphone firmware must be verified from an actual capture before enabling the voice endpoint on the live robot.
- Automatic Wi-Fi↔4G failover behavior is owned by the existing network layer; this feature must use the currently active connection and must not change modem routing until its behavior is verified.


## Implementation progress (2026-11-17)

- Task 1 implemented on this branch: authenticated `POST /api/ai/chat`, injectable AI provider, and configurable OpenAI-compatible chat adapter.
- Task 2 orchestration implemented: authenticated raw WAV endpoint, WAV format/duration checks, STT→AI→TTS provider interface, and no-partial-audio failure behavior. Production STT/TTS adapters and firmware transport are not yet implemented.
- Backend CI run `44066141533` completed successfully with 17 tests passing and 0 failures.
- Production speech-provider adapter is intentionally not configured yet. Provider/model selection is an explicit decision because the currently documented OpenAI Speech API TTS models are scheduled for shutdown on 2027-01-06; a new integration needs a supported replacement path or another provider.
- No API key has been added, no server has been deployed, and no firmware has been flashed.
