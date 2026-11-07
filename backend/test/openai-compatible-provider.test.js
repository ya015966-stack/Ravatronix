import test from "node:test";
import assert from "node:assert/strict";
import { createChatProvider } from "../src/chat-provider.js";

test("openai-compatible provider sends the chat and returns the model reply", async () => {
  let request;
  const provider = createChatProvider({
    mode: "openai-compatible",
    baseUrl: "https://llm.example.test/v1",
    apiKey: "test-secret",
    model: "small-chat-model",
    fetchImpl: async (url, options) => {
      request = { url, options };
      return {
        ok: true,
        status: 200,
        json: async () => ({ choices: [{ message: { content: "سلام! چطور کمک کنم؟" } }] })
      };
    }
  });

  const reply = await provider.reply({ messageId: "m-9", text: "سلام" });
  assert.equal(reply.type, "chat_response");
  assert.equal(reply.message_id, "m-9");
  assert.equal(reply.text, "سلام! چطور کمک کنم؟");
  assert.equal(reply.mode, "ai");
  assert.equal(reply.provider, "openai-compatible");
  assert.equal(request.url, "https://llm.example.test/v1/chat/completions");
  assert.equal(request.options.headers.authorization, "Bearer test-secret");
  assert.equal(JSON.parse(request.options.body).model, "small-chat-model");
  assert.deepEqual(JSON.parse(request.options.body).messages.at(-1), { role: "user", content: "سلام" });
});

test("openai-compatible provider requires endpoint, key, and model", () => {
  assert.throws(
    () => createChatProvider({ mode: "openai-compatible", baseUrl: "", apiKey: "", model: "" }),
    /requires RAVA_AI_BASE_URL, RAVA_AI_API_KEY, and RAVA_AI_MODEL/
  );
});

test("provider reports upstream failures without leaking credentials", async () => {
  const provider = createChatProvider({
    mode: "openai-compatible",
    baseUrl: "https://llm.example.test/v1",
    apiKey: "private-test-key",
    model: "small-chat-model",
    fetchImpl: async () => ({ ok: false, status: 429, json: async () => ({}) })
  });
  await assert.rejects(
    provider.reply({ messageId: "m-10", text: "سلام" }),
    (error) => !error.message.includes("private-test-key")
  );
});
