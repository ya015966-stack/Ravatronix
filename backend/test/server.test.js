import test from "node:test";
import assert from "node:assert/strict";
import { once } from "node:events";
import { WebSocket } from "ws";
import { createRavaServer } from "../src/server.js";

async function withServer(run) {
  const app = createRavaServer({ deviceToken: "test-device-token" });
  await new Promise((resolve) => app.server.listen(0, "127.0.0.1", resolve));
  const address = app.server.address();
  try {
    await run({ base: `http://127.0.0.1:${address.port}`, wsUrl: `ws://127.0.0.1:${address.port}/ws` });
  } finally {
    await new Promise((resolve) => app.wss.close(() => app.server.close(resolve)));
  }
}

test("GET /health reports an operational service without exposing secrets", async () => {
  await withServer(async ({ base }) => {
    const response = await fetch(`${base}/health`);
    assert.equal(response.status, 200);
    assert.deepEqual(await response.json(), { status: "ok", service: "ravatronix-backend" });
  });
});

test("device authenticates over WebSocket and receives a realtime message acknowledgement", async () => {
  await withServer(async ({ wsUrl }) => {
    const socket = new WebSocket(wsUrl);
    await once(socket, "open");
    socket.send(JSON.stringify({ type: "auth", role: "device", token: "test-device-token" }));
    const [authRaw] = await once(socket, "message");
    assert.equal(JSON.parse(authRaw.toString()).type, "auth_ok");

    socket.send(JSON.stringify({ type: "chat", message_id: "m-1", text: "سلام RAVA" }));
    const [replyRaw] = await once(socket, "message");
    const reply = JSON.parse(replyRaw.toString());
    assert.equal(reply.type, "chat_response");
    assert.equal(reply.message_id, "m-1");
    assert.equal(reply.text, "پیام دریافت شد؛ اتصال زنده است.");
    socket.close();
  });
});

test("unauthenticated WebSocket clients are rejected", async () => {
  await withServer(async ({ wsUrl }) => {
    const socket = new WebSocket(wsUrl);
    await once(socket, "open");
    socket.send(JSON.stringify({ type: "auth", role: "device", token: "wrong-token" }));
    const [code] = await once(socket, "close");
    assert.equal(code, 1008);
  });
});

test("POST /api/camera requires device authentication and accepts JPEG bytes", async () => {
  await withServer(async ({ base }) => {
    const jpeg = Buffer.from([0xff, 0xd8, 0xff, 0xd9]);
    const denied = await fetch(`${base}/api/camera`, {
      method: "POST", headers: { "content-type": "image/jpeg" }, body: jpeg
    });
    assert.equal(denied.status, 401);

    const accepted = await fetch(`${base}/api/camera`, {
      method: "POST",
      headers: { "content-type": "image/jpeg", authorization: "Bearer test-device-token" },
      body: jpeg
    });
    assert.equal(accepted.status, 202);
    assert.deepEqual(await accepted.json(), { accepted: true, bytes: 4 });
  });
});


test("POST /api/ai/chat requires device authentication", async () => {
  await withServer(async ({ base }) => {
    const response = await fetch(`${base}/api/ai/chat`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ message: "سلام" })
    });
    assert.equal(response.status, 401);
    assert.deepEqual(await response.json(), { error: "unauthorized" });
  });
});

test("POST /api/ai/chat returns a real provider reply", async () => {
  const app = createRavaServer({
    deviceToken: "test-device-token",
    aiProvider: { completeChat: async ({ messages }) => {
      assert.equal(messages.at(-1).content, "سلام RAVA");
      return "سلام! من راوا هستم.";
    } }
  });
  await new Promise((resolve) => app.server.listen(0, "127.0.0.1", resolve));
  const address = app.server.address();
  try {
    const response = await fetch(`http://127.0.0.1:${address.port}/api/ai/chat`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
        authorization: "Bearer test-device-token"
      },
      body: JSON.stringify({ message: "سلام RAVA", conversation_id: "conv-1" })
    });
    assert.equal(response.status, 200);
    assert.deepEqual(await response.json(), {
      reply: "سلام! من راوا هستم.",
      conversation_id: "conv-1"
    });
  } finally {
    await new Promise((resolve) => app.wss.close(() => app.server.close(resolve)));
  }
});

test("POST /api/ai/chat reports unavailable when no provider is configured", async () => {
  await withServer(async ({ base }) => {
    const response = await fetch(`${base}/api/ai/chat`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
        authorization: "Bearer test-device-token"
      },
      body: JSON.stringify({ message: "سلام" })
    });
    assert.equal(response.status, 503);
    assert.deepEqual(await response.json(), { error: "ai_provider_unavailable" });
  });
});

test("POST /api/ai/chat reports provider failures without fabricating a reply", async () => {
  const app = createRavaServer({
    deviceToken: "test-device-token",
    aiProvider: { completeChat: async () => { throw new Error("provider offline"); } }
  });
  await new Promise((resolve) => app.server.listen(0, "127.0.0.1", resolve));
  const address = app.server.address();
  try {
    const response = await fetch(`http://127.0.0.1:${address.port}/api/ai/chat`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
        authorization: "Bearer test-device-token"
      },
      body: JSON.stringify({ message: "سلام" })
    });
    assert.equal(response.status, 502);
    assert.deepEqual(await response.json(), { error: "ai_provider_failed" });
  } finally {
    await new Promise((resolve) => app.wss.close(() => app.server.close(resolve)));
  }
});
