# Ravatronix backend — RAVA AI Voice v1

Node.js gateway for authenticated RAVA device requests. The gateway keeps provider credentials on the server; do not place provider keys in ESP32 firmware, Android builds, or Git.

## Included

- `GET /health`: public health check.
- `POST /api/ai/chat`: authenticated text request to the configured OpenAI-compatible chat provider.
- `POST /api/voice/turn`: authenticated audio turn: Persian speech recognition (STT) → AI response → speech synthesis (TTS). The request and response use WAV over HTTPS.
- `POST /api/camera`: authenticated JPEG validation/acknowledgement only; images are not yet sent to a vision model or persisted.
- `WSS /ws`: authenticated device signaling. The current WebSocket `chat` message is a transport acknowledgement only; it is not the AI chat endpoint.

## Runtime requirements

- Node.js 20+
- Install: `npm install`
- Test: `npm test`
- Start: `npm start`

## Required server environment

Set these values in the backend host's secret/environment settings. Never commit real credentials.

| Variable | Purpose |
|---|---|
| `RAVA_DEVICE_TOKEN` | Long random device bearer token |
| `RAVA_AI_BASE_URL` | OpenAI-compatible API base URL; defaults to `https://api.openai.com/v1` |
| `RAVA_AI_API_KEY` | Provider key, server-side only |
| `RAVA_AI_MODEL` | Model identifier supported by the configured provider |
| `AZURE_SPEECH_KEY` | Azure Speech resource key |
| `AZURE_SPEECH_REGION` | Azure Speech resource region |
| `AZURE_SPEECH_LOCALE` | Optional; defaults to `fa-IR` |
| `AZURE_SPEECH_VOICE` | Optional; defaults to `fa-IR-DilaraNeural` |
| `PORT` | Hosting platform port; defaults to `10000` |

The text chat provider is unavailable until both `RAVA_AI_API_KEY` and `RAVA_AI_MODEL` are configured. Voice is unavailable until the AI provider and Azure Speech key/region are configured.

## Voice contract

Request:

- `POST /api/voice/turn`
- `Authorization: Bearer YOUR_DEVICE_TOKEN`
- `Content-Type: audio/wav`
- WAV encoding: PCM signed 16-bit little-endian, mono, 16 kHz
- Maximum input duration: 8 seconds
- Maximum request body: 512 KiB

Response:

- Success: `200 audio/wav`, PCM16 mono 16 kHz WAV audio
- `400`: invalid WAV format or speech request
- `401`: missing/invalid device token
- `413`: audio body exceeds the configured limit
- `415`: content type is not `audio/wav`
- `502`: an upstream AI or speech provider failed
- `503`: required provider configuration is unavailable

Example using a WAV file:

```sh
curl --fail-with-body \
  -X POST "https://YOUR_BACKEND_HOST/api/voice/turn" \
  -H "Authorization: Bearer YOUR_DEVICE_TOKEN" \
  -H "Content-Type: audio/wav" \
  --data-binary "@input.wav" \
  --output response.wav
```

Do not put the token in a URL or log request authorization headers. Use HTTPS only.

## Text chat contract

Send JSON to `POST /api/ai/chat` with the same device bearer token:

```json
{"message":"سلام راوا","conversation_id":"optional-session-id"}
```

The response contains `reply` and `conversation_id`. The current implementation sends the supplied message to the configured chat provider; conversation history is not persisted by the gateway.

## Hosting

For Render, create a Node web service from this repository and select:

- Root directory: `backend`
- Build command: `npm install --no-audit --no-fund`
- Start command: `npm start`
- Health check path: `/health`

Configure all required secrets in the host dashboard. This documentation does not imply that a service has been deployed or that live provider credentials have been configured.

## Current integration boundary

The backend voice pipeline exists, but the ESP32 firmware still needs a tested WAV capture/upload/playback client and the Android app still needs the Push-to-Talk control flow. Do not flash a new firmware image until those changes build and the audio format has been verified with the actual microphone.
