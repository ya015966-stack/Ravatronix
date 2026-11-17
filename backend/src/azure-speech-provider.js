const DEFAULT_LOCALE = "fa-IR";
const DEFAULT_VOICE = "fa-IR-DilaraNeural";
const DEFAULT_TIMEOUT_MS = 20_000;
const OUTPUT_FORMAT = "riff-16khz-16bit-mono-pcm";

function escapeXml(value) {
  return value.replace(/[<>&"']/g, (character) => ({
    "<": "&lt;",
    ">": "&gt;",
    "&": "&amp;",
    '"': "&quot;",
    "'": "&apos;"
  })[character]);
}

export function createAzureSpeechProvider({
  apiKey = process.env.AZURE_SPEECH_KEY,
  region = process.env.AZURE_SPEECH_REGION,
  locale = process.env.AZURE_SPEECH_LOCALE ?? DEFAULT_LOCALE,
  voice = process.env.AZURE_SPEECH_VOICE ?? DEFAULT_VOICE,
  fetchImpl = fetch,
  timeoutMs = DEFAULT_TIMEOUT_MS
} = {}) {
  if (!apiKey || !region) return null;
  if (!/^[a-z0-9-]+$/i.test(region)) {
    throw new Error("AZURE_SPEECH_REGION must be a valid Azure region identifier");
  }

  const sttEndpoint = new URL(
    `https://${region}.stt.speech.microsoft.com/speech/recognition/conversation/cognitiveservices/v1`
  );
  sttEndpoint.searchParams.set("language", locale);
  sttEndpoint.searchParams.set("format", "simple");

  const ttsEndpoint = new URL(`https://${region}.tts.speech.microsoft.com/cognitiveservices/v1`);

  async function request(url, options) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), timeoutMs);
    try {
      const response = await fetchImpl(url, {
        ...options,
        signal: controller.signal
      });
      if (!response.ok) {
        throw new Error(`Azure Speech returned HTTP ${response.status}`);
      }
      return response;
    } finally {
      clearTimeout(timeout);
    }
  }

  return {
    async transcribeAudio({ audio, mimeType = "audio/wav" }) {
      if (!Buffer.isBuffer(audio) || audio.length === 0) {
        throw new Error("Azure Speech requires a non-empty audio buffer");
      }
      if (mimeType !== "audio/wav") {
        throw new Error("Azure Speech input must be audio/wav");
      }

      const response = await request(sttEndpoint, {
        method: "POST",
        headers: {
          "Ocp-Apim-Subscription-Key": apiKey,
          "Content-Type": "audio/wav",
          Accept: "application/json"
        },
        body: audio
      });
      const payload = await response.json();
      if (payload?.RecognitionStatus !== "Success" ||
          typeof payload?.DisplayText !== "string" || !payload.DisplayText.trim()) {
        throw new Error("Azure Speech did not recognize speech");
      }
      return payload.DisplayText.trim();
    },

    async synthesizeSpeech({ text }) {
      if (typeof text !== "string" || !text.trim()) {
        throw new Error("Azure Speech requires non-empty text");
      }
      if (text.length > 4_000) {
        throw new Error("Azure Speech text exceeds the 4000 character limit");
      }

      const ssml = `<speak version="1.0" xml:lang="${escapeXml(locale)}"><voice xml:lang="${escapeXml(locale)}" name="${escapeXml(voice)}">${escapeXml(text.trim())}</voice></speak>`;
      const response = await request(ttsEndpoint, {
        method: "POST",
        headers: {
          "Ocp-Apim-Subscription-Key": apiKey,
          "Content-Type": "application/ssml+xml",
          "X-Microsoft-OutputFormat": OUTPUT_FORMAT,
          "User-Agent": "Ravatronix-RAVA"
        },
        body: ssml
      });
      return Buffer.from(await response.arrayBuffer());
    }
  };
}
