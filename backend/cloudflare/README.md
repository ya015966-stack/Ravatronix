# Ravatronix Cloudflare Workers backend

This is a Cloudflare Workers port of the initial RAVA backend. It is separate from the ESP32 camera firmware and does not modify it.

## Endpoints

- `GET /health` — health check.
- `POST /api/camera` — accepts authenticated JPEG payloads up to 2 MiB and acknowledges validation. It does not store or forward images yet.
- `/ws` — authenticated WebSocket endpoint. The first JSON message must be `{"type":"auth","role":"device","token":"..."}`. Chat currently returns a connectivity acknowledgement only; it is not connected to an AI provider.

## Deploy

1. Create a Cloudflare account and select the Workers Free plan if available. Do not add a payment method or choose a paid plan.
2. Install Wrangler in a Node.js 20+ environment.
3. From this directory run `npx wrangler login`.
4. Set the secret: `npx wrangler secret put RAVA_DEVICE_TOKEN`.
5. Deploy: `npx wrangler deploy`.

The ESP32 firmware is unchanged. After deployment, test `/health` before configuring any device URL.

## Free-tier notes

Cloudflare documents a Workers Free allowance of 100,000 requests/day and a 10 ms CPU-time limit per invocation. WebSocket upgrades count as requests; messages on an established Worker WebSocket are not separately counted as Worker requests. Limits and account verification requirements can change, so confirm the dashboard shows a $0 Free plan before proceeding.

This backend is an early connectivity stage, not a production camera relay or AI service. Do not put device tokens in source code or public chat.
