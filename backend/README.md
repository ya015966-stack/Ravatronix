# Ravatronix backend (phase 1)

Node.js service for the first authenticated connection between RAVA and the cloud.

## Included

- `GET /health`: public service health check.
- `WSS /ws`: authenticated device connection and realtime AI chat.
- `POST /api/camera`: authenticated JPEG upload (up to 2 MiB). This phase validates and acknowledges the image; it does not persist or forward it.
- Device authentication uses the first WebSocket JSON message, not a URL query parameter, so the secret is not placed in request URLs.

## Local run

Requires Node.js 20 or newer.

```sh
npm install
RAVA_DEVICE_TOKEN='your-long-random-secret' npm start
npm test
```

## WebSocket handshake

Connect to `wss://YOUR-SERVICE.onrender.com/ws`, then send:

```json
{"type":"auth","role":"device","token":"YOUR_DEVICE_TOKEN"}
```

The server replies with `auth_ok`. A device can then send:

```json
{"type":"chat","message_id":"msg-001","text":"سلام RAVA"}
```

The backend forwards the text to the configured AI provider and returns its generated answer as `chat_response`. If the provider is unavailable, it returns an explicit error instead of fabricating a reply.

The same service also exposes `POST /api/ai/chat` for authenticated JSON chat and `POST /api/voice/turn` for authenticated WAV speech turns. Voice input must be mono PCM16, 16 kHz WAV (maximum 8 seconds); the response is WAV audio. Configure `RAVA_AI_BASE_URL`, `RAVA_AI_API_KEY`, and `RAVA_AI_MODEL` in the host secret manager. For speech, configure `AZURE_SPEECH_KEY` and `AZURE_SPEECH_REGION`.

## Camera upload

Send a JPEG body to `POST /api/camera` with:

```
Authorization: Bearer YOUR_DEVICE_TOKEN
Content-Type: image/jpeg
```

The current phase responds with HTTP 202 and the received byte count. It does not store the image.

## Render

Create a Node web service from this repository and branch:

- Root directory: `backend`
- Build command: `npm install`
- Start command: `npm start`
- Health check path: `/health`
- Environment variable: `RAVA_DEVICE_TOKEN` (set a unique random secret in Render; do not commit it)

The service binds to Render's `PORT` and listens on `0.0.0.0`.

## Scope boundaries

This phase does not change the ESP32 camera firmware, implement AI-provider calls, expose a mobile app, store images, or enable motor commands. Those are separate stages after this transport layer is verified.
