const DEFAULT_BASE_URL = "https://api.openai.com/v1";
const DEFAULT_TIMEOUT_MS = 25_000;

export function createOpenAICompatibleProvider({
  baseUrl = process.env.RAVA_AI_BASE_URL ?? DEFAULT_BASE_URL,
  apiKey = process.env.RAVA_AI_API_KEY,
  model = process.env.RAVA_AI_MODEL,
  fetchImpl = fetch,
  timeoutMs = DEFAULT_TIMEOUT_MS
} = {}) {
  if (!apiKey || !model) return null;

  let endpoint;
  try {
    endpoint = new URL(`${String(baseUrl).replace(/\/+$/, "")}/chat/completions`);
  } catch {
    throw new Error("RAVA_AI_BASE_URL must be a valid absolute URL");
  }

  return {
    async completeChat({ messages }) {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), timeoutMs);
      try {
        const response = await fetchImpl(endpoint, {
          method: "POST",
          headers: {
            authorization: `Bearer ${apiKey}`,
            "content-type": "application/json"
          },
          body: JSON.stringify({ model, messages }),
          signal: controller.signal
        });

        if (!response.ok) {
          throw new Error(`AI provider returned HTTP ${response.status}`);
        }

        const payload = await response.json();
        const reply = payload?.choices?.[0]?.message?.content;
        if (typeof reply !== "string" || !reply.trim()) {
          throw new Error("AI provider returned an empty or unsupported response");
        }
        return reply.trim();
      } finally {
        clearTimeout(timeout);
      }
    }
  };
}
