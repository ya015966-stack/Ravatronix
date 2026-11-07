const DEMO_REPLY = "حالت آزمایشی فعال است؛ هنوز مدل هوش مصنوعی متصل نشده است.";

export function createChatProvider({ mode = "demo" } = {}) {
  if (mode !== "demo") {
    throw new Error(`Unsupported chat provider mode: ${mode}`);
  }

  return {
    mode,
    async reply({ messageId }) {
      return {
        type: "chat_response",
        message_id: messageId,
        text: DEMO_REPLY,
        mode: "demo",
        provider: "demo"
      };
    }
  };
}
