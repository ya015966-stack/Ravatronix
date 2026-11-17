export class VoiceRequestError extends Error {
  constructor(code) {
    super(code);
    this.code = code;
  }
}

export function inspectPcm16Mono16kWav(audio, maxSeconds = 8) {
  if (!Buffer.isBuffer(audio) || audio.length < 44) return null;
  if (audio.toString("ascii", 0, 4) !== "RIFF" || audio.toString("ascii", 8, 12) !== "WAVE") return null;

  let offset = 12;
  let format = null;
  let dataSize = null;

  while (offset + 8 <= audio.length) {
    const chunkId = audio.toString("ascii", offset, offset + 4);
    const chunkSize = audio.readUInt32LE(offset + 4);
    const chunkStart = offset + 8;
    const chunkEnd = chunkStart + chunkSize;
    if (chunkEnd > audio.length) return null;

    if (chunkId === "fmt " && chunkSize >= 16) {
      format = {
        encoding: audio.readUInt16LE(chunkStart),
        channels: audio.readUInt16LE(chunkStart + 2),
        sampleRate: audio.readUInt32LE(chunkStart + 4),
        byteRate: audio.readUInt32LE(chunkStart + 8),
        blockAlign: audio.readUInt16LE(chunkStart + 12),
        bitsPerSample: audio.readUInt16LE(chunkStart + 14)
      };
    } else if (chunkId === "data") {
      dataSize = chunkSize;
    }

    offset = chunkEnd + (chunkSize % 2);
  }

  if (!format || dataSize === null) return null;
  if (format.encoding !== 1 || format.channels !== 1 ||
      format.sampleRate !== 16_000 || format.bitsPerSample !== 16 ||
      format.blockAlign !== 2 || format.byteRate !== 32_000) return null;

  const durationSeconds = dataSize / format.byteRate;
  if (dataSize <= 0 || durationSeconds > maxSeconds || dataSize % 2 !== 0) return null;
  return { durationSeconds, dataSize };
}

export async function processVoiceTurn({ audio, speechProvider, aiProvider }) {
  if (!inspectPcm16Mono16kWav(audio)) {
    throw new VoiceRequestError("invalid_wav_format");
  }

  const transcript = await speechProvider.transcribeAudio({ audio, mimeType: "audio/wav" });
  if (typeof transcript !== "string" || !transcript.trim()) {
    throw new Error("Speech provider returned an empty transcript");
  }

  const reply = await aiProvider.completeChat({
    messages: [
      { role: "system", content: "تو RAVA، دستیار هوشمند ربات Ravatronix هستی. به‌طور پیش‌فرض فارسی، روشن و دقیق پاسخ بده. ادعای انجام عملیاتی را که واقعاً انجام نشده مطرح نکن." },
      { role: "user", content: transcript.trim() }
    ]
  });
  if (typeof reply !== "string" || !reply.trim()) {
    throw new Error("AI provider returned an empty response");
  }

  const responseAudio = await speechProvider.synthesizeSpeech({ text: reply.trim() });
  if (!inspectPcm16Mono16kWav(responseAudio, 30)) {
    throw new Error("Speech provider returned audio outside the PCM16 mono 16 kHz WAV contract");
  }
  return responseAudio;
}
