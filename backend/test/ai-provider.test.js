import test from "node:test";
import assert from "node:assert/strict";
import { createOpenAICompatibleProvider } from "../src/ai-provider.js";

test("provider is unavailable until API key and model are configured", () => {
  assert.equal(createOpenAICompatibleProvider({ apiKey: "", model: "" }), null);
});

test("provider sends configured model and messages to the OpenAI-compatible endpoint", async () => {
  let observed;
  const provider = createOpenAICompatibleProvider({
    baseUrl: "https://example.ai/v1/",
    apiKey: "server-only-test-secret",
    model: "test-model",
    fetchImpl: async (url, options) => {
      observed = { url: String(url), options };
      return {
        ok: true,
        json: async () => ({ choices: [{ message: { content: "  پاسخ آزمایشی  " } }] })
      };
    }
  });

  const reply = await provider.completeChat({
    messages: [{ role: "user", content: "سلام" }]
  });

  assert.equal(reply, "پاسخ آزمایشی");
  assert.equal(observed.url, "https://example.ai/v1/chat/completions");
  assert.equal(observed.options.method, "POST");
  assert.equal(observed.options.headers.authorization, "Bearer server-only-test-secret");
  assert.deepEqual(JSON.parse(observed.options.body), {
    model: "test-model",
    messages: [{ role: "user", content: "سلام" }]
  });
});

test("provider rejects upstream HTTP errors without returning response content", async () => {
  const provider = createOpenAICompatibleProvider({
    apiKey: "server-only-test-secret",
    model: "test-model",
    fetchImpl: async () => ({ ok: false, status: 429 })
  });

  await assert.rejects(
    provider.completeChat({ messages: [{ role: "user", content: "سلام" }] }),
    /HTTP 429/
  );
});

test("provider rejects an empty model response", async () => {
  const provider = createOpenAICompatibleProvider({
    apiKey: "server-only-test-secret",
    model: "test-model",
    fetchImpl: async () => ({
      ok: true,
      json: async () => ({ choices: [{ message: { content: " " } }] })
    })
  });

  await assert.rejects(
    provider.completeChat({ messages: [{ role: "user", content: "سلام" }] }),
    /empty or unsupported response/
  );
});
