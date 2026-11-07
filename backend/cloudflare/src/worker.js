const MAX_CAMERA_BYTES = 2 * 1024 * 1024;
const MAX_WS_BYTES = 16 * 1024;

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store",
      "x-content-type-options": "nosniff",
    },
  });
}

function constantTimeEqual(a, b) {
  if (typeof a !== "string" || typeof b !== "string" || a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

function validJpeg(bytes) {
  return bytes.length >= 4 &&
    bytes[0] === 0xff && bytes[1] === 0xd8 &&
    bytes[bytes.length - 2] === 0xff && bytes[bytes.length - 1] === 0xd9;
}

async function handleCamera(request, env) {
  const auth = request.headers.get("authorization") || "";
  const token = auth.startsWith("Bearer ") ? auth.slice(7) : "";
  if (!constantTimeEqual(token, env.RAVA_DEVICE_TOKEN)) return json({ error: "unauthorized" }, 401);

  if (!(request.headers.get("content-type") || "").toLowerCase().startsWith("image/jpeg")) {
    return json({ error: "content_type_must_be_image_jpeg" }, 415);
  }

  const declaredLength = Number(request.headers.get("content-length") || 0);
  if (declaredLength > MAX_CAMERA_BYTES) return json({ error: "image_too_large" }, 413);

  const bytes = new Uint8Array(await request.arrayBuffer());
  if (bytes.length > MAX_CAMERA_BYTES) return json({ error: "image_too_large" }, 413);
  if (!validJpeg(bytes)) return json({ error: "invalid_jpeg" }, 400);

  // Current phase: validate and acknowledge only; image storage/forwarding is not enabled.
  return json({ accepted: true, bytes: bytes.length }, 202);
}

function handleWebSocket(request, env) {
  if (request.headers.get("Upgrade")?.toLowerCase() !== "websocket") {
    return json({ error: "websocket_upgrade_required" }, 426);
  }

  const pair = new WebSocketPair();
  const client = pair[0];
  const server = pair[1];
  server.accept();

  let authenticated = false;
  let closed = false;
  const authTimer = setTimeout(() => {
    if (!authenticated && !closed) {
      closed = true;
      try { server.close(1008, "authentication required"); } catch {}
    }
  }, 5000);

  server.addEventListener("message", (event) => {
    if (closed) return;
    if (typeof event.data !== "string" || new TextEncoder().encode(event.data).length > MAX_WS_BYTES) {
      closed = true;
      clearTimeout(authTimer);
      server.close(1009, "message too large");
      return;
    }

    let message;
    try {
      message = JSON.parse(event.data);
    } catch {
      closed = true;
      clearTimeout(authTimer);
      server.close(1003, "invalid JSON");
      return;
    }

    if (!authenticated) {
      if (message?.type !== "auth" || message.role !== "device" ||
          !constantTimeEqual(message.token, env.RAVA_DEVICE_TOKEN)) {
        closed = true;
        clearTimeout(authTimer);
        server.close(1008, "authentication failed");
        return;
      }
      authenticated = true;
      clearTimeout(authTimer);
      server.send(JSON.stringify({ type: "auth_ok", device_id: "rava-001" }));
      return;
    }

    if (message?.type === "chat" && typeof message.message_id === "string" &&
        typeof message.text === "string") {
      server.send(JSON.stringify({
        type: "chat_response",
        message_id: message.message_id,
        text: "پیام دریافت شد؛ اتصال زنده است.",
      }));
      return;
    }

    server.send(JSON.stringify({ type: "error", error: "unsupported_message" }));
  });

  server.addEventListener("close", () => {
    closed = true;
    clearTimeout(authTimer);
  });
  server.addEventListener("error", () => {
    closed = true;
    clearTimeout(authTimer);
  });

  return new Response(null, { status: 101, webSocket: client });
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    if (request.method === "GET" && url.pathname === "/health") {
      return json({ status: "ok", service: "ravatronix-backend", runtime: "cloudflare-workers" });
    }

    if (request.method === "POST" && url.pathname === "/api/camera") {
      if (!env.RAVA_DEVICE_TOKEN) return json({ error: "server_not_configured" }, 503);
      return handleCamera(request, env);
    }

    if (url.pathname === "/ws") {
      if (!env.RAVA_DEVICE_TOKEN) return json({ error: "server_not_configured" }, 503);
      return handleWebSocket(request, env);
    }

    return json({ error: "not_found" }, 404);
  },
};
