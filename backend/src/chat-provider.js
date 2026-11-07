const DEMO_REPLY = "حالت آزمایشی فعال است؛ هنوز مدل هوش مصنوعی متصل نشده است.";
const REQUEST_TIMEOUT_MS = 15_000;

function normalizeBaseUrl(value) {
  let url;
  try {
    url = new URL(value);
  } catch {
    throw new Error("RAVA_AI_BASE_URL must be a valid HTTPS URL.");
  }
  if (url.protocol !== "https:") {
    throw new Error("RAVA_AI_BASE_URL must use HTTPS.");
  }
  return url.toString().replace(/\/$/, "");
}

function createOpenAICompatibleProvider({ baseUrl, apiKey, model, fetchImpl = fetch }) {
  if (!baseUrl || !apiKey || !model) {
    throw new Error("OpenAI-compatible provider requires RAVA_AI_BASE_URL, RAVA_AI_API_KEY, and RAVA_AI_MODEL.");
  }
  const endpoint = `${normalizeBaseUrl(baseUrl)}/chat/completions`;

  return {
    mode: "openai-compatible",
    async reply({ messageId, text }) {
      const response = await fetchImpl(endpoint, {
        method: "POST",
        headers: {
          "content-type": "application/json",
          authorization: `Bearer ${apiKey}`
        },
        body: JSON.stringify({
          model,
          messages: [
            {
              role: "system",
              content: "You are RAVA, a helpful Persian-speaking assistant. Reply in Persian unless the user asks otherwise. Be concise, honest, and safety-conscious."
            },
            { role: "user", content: text }
          ],
          temperature: 0.4,
          max_tokens: 500
        }),
        signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS)
      });

      if (!response.ok) {
        throw new Error(`AI provider request failed with status ${response.status}.`);
      }

      const payload = await response.json();
      const answer = payload?.choices?.[0]?.message?.content;
      if (typeof answer !== "string" || !answer.trim()) {
        throw new Error("AI provider returned an empty or invalid response.");
      }

      return {
        type: "chat_response",
        message_id: messageId,
        text: answer.trim(),
        mode: "ai",
        provider: "openai-compatible"
      };
    }
  };
}

export function createChatProvider(options = {}) {
  const mode = options.mode ?? "demo";
  if (mode === "demo") {
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

  if (mode === "openai-compatible") {
    return createOpenAICompatibleProvider({
      baseUrl: options.baseUrl ?? process.env.RAVA_AI_BASE_URL,
      apiKey: options.apiKey ?? process.env.RAVA_AI_API_KEY,
      model: options.model ?? process.env.RAVA_AI_MODEL,
      fetchImpl: options.fetchImpl
    });
  }

  throw new Error(`Unsupported chat provider mode: ${mode}`);
}
