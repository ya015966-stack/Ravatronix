import test from "node:test";
import assert from "node:assert/strict";
import { createChatProvider } from "../src/chat-provider.js";

test("demo provider clearly identifies replies as non-AI test responses", async () => {
  const provider = createChatProvider({ mode: "demo" });
  const reply = await provider.reply({ messageId: "m-1", text: "سلام" });

  assert.deepEqual(reply, {
    type: "chat_response",
    message_id: "m-1",
    text: "حالت آزمایشی فعال است؛ هنوز مدل هوش مصنوعی متصل نشده است.",
    mode: "demo",
    provider: "demo"
  });
});

test("demo provider preserves the supplied message id", async () => {
  const provider = createChatProvider({ mode: "demo" });
  const reply = await provider.reply({ messageId: "request-42", text: "وضعیت" });
  assert.equal(reply.message_id, "request-42");
});

test("unsupported provider modes fail explicitly", () => {
  assert.throws(() => createChatProvider({ mode: "openai" }), /Unsupported chat provider mode/);
});
