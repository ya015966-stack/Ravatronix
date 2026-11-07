import { createServer } from "node:http";
import { timingSafeEqual } from "node:crypto";
import { WebSocketServer, WebSocket } from "ws";
import { createChatProvider } from "./chat-provider.js";

const MAX_CAMERA_BYTES = 2 * 1024 * 1024;
const MAX_WS_BYTES = 16 * 1024;
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
  chatProvider = createChatProvider({ mode: process.env.RAVA_CHAT_MODE ?? "demo" })
} = {}) {
  if (!deviceToken) {
    throw new Error("RAVA_DEVICE_TOKEN is required; refusing to start without device authentication.");
  }

  const server = createServer(async (request, response) => {
    const url = new URL(request.url ?? "/", "http://localhost");

    if (request.method === "GET" && url.pathname === "/health") {
      return sendJson(response, 200, { status: "ok", service: "ravatronix-backend" });
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

    socket.on("message", async (raw) => {
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
        try {
          const reply = await chatProvider.reply({
            messageId: message.message_id,
            text: message.text
          });
          if (socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify(reply));
        } catch {
          if (socket.readyState === WebSocket.OPEN) {
            socket.send(JSON.stringify({
              type: "chat_error",
              message_id: message.message_id,
              error: "chat_provider_unavailable"
            }));
          }
        }
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
