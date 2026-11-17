import { createServer } from "node:http";
import { randomUUID, timingSafeEqual } from "node:crypto";
import { createOpenAICompatibleProvider } from "./ai-provider.js";
import { inspectPcm16Mono16kWav, processVoiceTurn, VoiceRequestError } from "./voice-service.js";
import { WebSocketServer, WebSocket } from "ws";

const MAX_CAMERA_BYTES = 2 * 1024 * 1024;
const MAX_WS_BYTES = 16 * 1024;
const MAX_CHAT_BYTES = 8 * 1024;
const MAX_AUDIO_BYTES = 512 * 1024;
const AUTH_TIMEOUT_MS = 5_000;

function safeEqual(left, right) {
  if (typeof left !== "string" || typeof right !== "string") return false;
  const a = Buffer.from(left);
  const b = Buffer.from(right);
  return a.length === b.length && timingSafeEqual(a, b);
}

function sendJson(response, status, payload) {
  response.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "cache-control": "no-store",
    "x-content-type-options": "nosniff"
  });
  response.end(JSON.stringify(payload));
}

function readLimitedBody(request, limit) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    request.on("data", (chunk) => {
      size += chunk.length;
      if (size > limit) {
        reject(Object.assign(new Error("Payload too large"), { statusCode: 413 }));
        request.destroy();
        return;
      }
      chunks.push(chunk);
    });
    request.on("end", () => resolve(Buffer.concat(chunks)));
    request.on("error", reject);
  });
}

export function createRavaServer({
  deviceToken = process.env.RAVA_DEVICE_TOKEN,
  aiProvider = createOpenAICompatibleProvider(),
  speechProvider = null
} = {}) {
  if (!deviceToken) {
    throw new Error("RAVA_DEVICE_TOKEN is required; refusing to start without device authentication.");
  }

  const server = createServer(async (request, response) => {
    const url = new URL(request.url ?? "/", "http://localhost");

    if (request.method === "GET" && url.pathname === "/health") {
      return sendJson(response, 200, { status: "ok", service: "ravatronix-backend" });
    }

    if (request.method === "POST" && url.pathname === "/api/voice/turn") {
      const authorization = request.headers.authorization ?? "";
      const suppliedToken = authorization.startsWith("Bearer ") ? authorization.slice(7) : "";
      if (!safeEqual(suppliedToken, deviceToken)) {
        return sendJson(response, 401, { error: "unauthorized" });
      }
      if (!String(request.headers["content-type"] ?? "").toLowerCase().startsWith("audio/wav")) {
        return sendJson(response, 415, { error: "content_type_must_be_audio_wav" });
      }
      let audio;
      try {
        audio = await readLimitedBody(request, MAX_AUDIO_BYTES);
      } catch (error) {
        if (error.statusCode === 413) return sendJson(response, 413, { error: "audio_too_large" });
        if (!response.headersSent && !response.destroyed) return sendJson(response, 400, { error: "audio_upload_failed" });
        return;
      }

      if (!inspectPcm16Mono16kWav(audio)) {
        return sendJson(response, 400, { error: "invalid_wav_format" });
      }
      if (!speechProvider || typeof speechProvider.transcribeAudio !== "function" ||
          typeof speechProvider.synthesizeSpeech !== "function" ||
          !aiProvider || typeof aiProvider.completeChat !== "function") {
        return sendJson(response, 503, { error: "voice_provider_unavailable" });
      }

      try {
        const responseAudio = await processVoiceTurn({ audio, speechProvider, aiProvider });
        response.writeHead(200, {
          "content-type": "audio/wav",
          "content-length": responseAudio.length,
          "cache-control": "no-store",
          "x-content-type-options": "nosniff"
        });
        return response.end(responseAudio);
      } catch (error) {
        if (error instanceof VoiceRequestError) {
          return sendJson(response, 400, { error: error.code });
        }
        return sendJson(response, 502, { error: "voice_processing_failed" });
      }
    }

    if (request.method === "POST" && url.pathname === "/api/ai/chat") {
      const authorization = request.headers.authorization ?? "";
      const suppliedToken = authorization.startsWith("Bearer ") ? authorization.slice(7) : "";
      if (!safeEqual(suppliedToken, deviceToken)) {
        return sendJson(response, 401, { error: "unauthorized" });
      }
      if (!String(request.headers["content-type"] ?? "").toLowerCase().startsWith("application/json")) {
        return sendJson(response, 415, { error: "content_type_must_be_application_json" });
      }

      let payload;
      try {
        const rawBody = await readLimitedBody(request, MAX_CHAT_BYTES);
        payload = JSON.parse(rawBody.toString("utf8"));
      } catch (error) {
        if (error.statusCode === 413) return sendJson(response, 413, { error: "request_too_large" });
        if (!response.headersSent && !response.destroyed) return sendJson(response, 400, { error: "invalid_json" });
        return;
      }

      if (typeof payload?.message !== "string" || !payload.message.trim() || payload.message.length > 4_000) {
        return sendJson(response, 400, { error: "invalid_message" });
      }
      if (!aiProvider || typeof aiProvider.completeChat !== "function") {
        return sendJson(response, 503, { error: "ai_provider_unavailable" });
      }

      try {
        const reply = await aiProvider.completeChat({
          messages: [
            { role: "system", content: "تو RAVA، دستیار هوشمند ربات Ravatronix هستی. به‌طور پیش‌فرض فارسی، روشن و دقیق پاسخ بده. ادعای انجام عملیاتی را که واقعاً انجام نشده مطرح نکن." },
            { role: "user", content: payload.message }
          ]
        });
        if (typeof reply !== "string" || !reply.trim()) {
          return sendJson(response, 502, { error: "ai_provider_invalid_response" });
        }
        return sendJson(response, 200, {
          reply: reply.trim(),
          conversation_id: typeof payload.conversation_id === "string" && payload.conversation_id
            ? payload.conversation_id
            : randomUUID()
        });
      } catch {
        return sendJson(response, 502, { error: "ai_provider_failed" });
      }
    }

    if (request.method === "POST" && url.pathname === "/api/camera") {
      const authorization = request.headers.authorization ?? "";
      const suppliedToken = authorization.startsWith("Bearer ") ? authorization.slice(7) : "";
      if (!safeEqual(suppliedToken, deviceToken)) {
        return sendJson(response, 401, { error: "unauthorized" });
      }
      if (!String(request.headers["content-type"] ?? "").toLowerCase().startsWith("image/jpeg")) {
        return sendJson(response, 415, { error: "content_type_must_be_image_jpeg" });
      }

      try {
        const image = await readLimitedBody(request, MAX_CAMERA_BYTES);
        if (image.length < 4 || image[0] !== 0xff || image[1] !== 0xd8 ||
            image[image.length - 2] !== 0xff || image[image.length - 1] !== 0xd9) {
          return sendJson(response, 400, { error: "invalid_jpeg" });
        }
        return sendJson(response, 202, { accepted: true, bytes: image.length });
      } catch (error) {
        if (error.statusCode === 413) {
          return sendJson(response, 413, { error: "image_too_large" });
        }
        if (!response.headersSent && !response.destroyed) {
          return sendJson(response, 400, { error: "image_upload_failed" });
        }
      }
      return;
    }

    return sendJson(response, 404, { error: "not_found" });
  });

  const wss = new WebSocketServer({ noServer: true, maxPayload: MAX_WS_BYTES });

  server.on("upgrade", (request, socket, head) => {
    const url = new URL(request.url ?? "/", "http://localhost");
    if (url.pathname !== "/ws") {
      socket.write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
      socket.destroy();
      return;
    }
    wss.handleUpgrade(request, socket, head, (webSocket) => {
      wss.emit("connection", webSocket, request);
    });
  });

  wss.on("connection", (socket) => {
    let authenticated = false;
    const authTimer = setTimeout(() => {
      if (!authenticated && socket.readyState === WebSocket.OPEN) socket.close(1008, "authentication required");
    }, AUTH_TIMEOUT_MS);
    authTimer.unref();

    socket.on("message", (raw) => {
      let message;
      try {
        message = JSON.parse(raw.toString());
      } catch {
        socket.close(1003, "invalid JSON");
        return;
      }

      if (!authenticated) {
        if (message?.type !== "auth" || message.role !== "device" || !safeEqual(message.token, deviceToken)) {
          clearTimeout(authTimer);
          socket.close(1008, "authentication failed");
          return;
        }
        authenticated = true;
        clearTimeout(authTimer);
        socket.send(JSON.stringify({ type: "auth_ok", device_id: "rava-001" }));
        return;
      }

      if (message?.type === "chat" && typeof message.message_id === "string" &&
          typeof message.text === "string") {
        socket.send(JSON.stringify({
          type: "chat_response",
          message_id: message.message_id,
          text: "پیام دریافت شد؛ اتصال زنده است."
        }));
        return;
      }

      socket.send(JSON.stringify({ type: "error", error: "unsupported_message" }));
    });

    socket.on("close", () => clearTimeout(authTimer));
    socket.on("error", () => clearTimeout(authTimer));
  });

  return { server, wss };
}

if (process.argv[1] && new URL(import.meta.url).pathname === process.argv[1]) {
  const { server } = createRavaServer();
  const port = Number(process.env.PORT ?? 10000);
  server.listen(port, "0.0.0.0", () => {
    console.log(`[RAVA] backend listening on port ${port}`);
  });
}
