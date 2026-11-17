import test from "node:test";
import assert from "node:assert/strict";
import { once } from "node:events";
import { createRavaServer } from "../src/server.js";

function makeWav(seconds = 1) {
  const sampleRate = 16_000;
  const dataSize = sampleRate * 2 * seconds;
  const wav = Buffer.alloc(44 + dataSize);
  wav.write("RIFF", 0);
  wav.writeUInt32LE(36 + dataSize, 4);
  wav.write("WAVE", 8);
  wav.write("fmt ", 12);
  wav.writeUInt32LE(16, 16);
  wav.writeUInt16LE(1, 20);
  wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(sampleRate, 24);
  wav.writeUInt32LE(sampleRate * 2, 28);
  wav.writeUInt16LE(2, 32);
  wav.writeUInt16LE(16, 34);
  wav.write("data", 36);
  wav.writeUInt32LE(dataSize, 40);
  return wav;
}

async function withVoiceServer(run, { speechProvider, aiProvider } = {}) {
  const app = createRavaServer({
    deviceToken: "test-device-token",
    speechProvider,
    aiProvider
  });
  await new Promise((resolve) => app.server.listen(0, "127.0.0.1", resolve));
  const address = app.server.address();
  try {
    await run(`http://127.0.0.1:${address.port}`);
  } finally {
    await new Promise((resolve) => app.wss.close(() => app.server.close(resolve)));
  }
}

test("voice turn requires device authentication", async () => {
  await withVoiceServer(async (base) => {
    const response = await fetch(`${base}/api/voice/turn`, {
      method: "POST",
      headers: { "content-type": "audio/wav" },
      body: makeWav()
    });
    assert.equal(response.status, 401);
  });
});

test("voice turn rejects audio that is not mono 16 kHz PCM16 WAV", async () => {
  await withVoiceServer(async (base) => {
    const invalid = Buffer.from("not-a-wave-file");
    const response = await fetch(`${base}/api/voice/turn`, {
      method: "POST",
      headers: {
        "content-type": "audio/wav",
        authorization: "Bearer test-device-token"
      },
      body: invalid
    });
    assert.equal(response.status, 400);
    assert.deepEqual(await response.json(), { error: "invalid_wav_format" });
  });
});

test("voice turn performs STT, AI response, and TTS and returns WAV audio", async () => {
  const calls = [];
  const speechProvider = {
    transcribeAudio: async ({ audio }) => {
      calls.push(["stt", audio.length]);
      return "سلام راوا";
    },
    synthesizeSpeech: async ({ text }) => {
      calls.push(["tts", text]);
      return makeWav();
    }
  };
  const aiProvider = {
    completeChat: async ({ messages }) => {
      calls.push(["ai", messages.at(-1).content]);
      return "سلام! صدایت را دریافت کردم.";
    }
  };

  await withVoiceServer(async (base) => {
    const response = await fetch(`${base}/api/voice/turn`, {
      method: "POST",
      headers: {
        "content-type": "audio/wav",
        authorization: "Bearer test-device-token"
      },
      body: makeWav()
    });
    assert.equal(response.status, 200);
    assert.equal(response.headers.get("content-type"), "audio/wav");
    assert.deepEqual(Buffer.from(await response.arrayBuffer()), makeWav());
    assert.deepEqual(calls.map((call) => call[0]), ["stt", "ai", "tts"]);
    assert.equal(calls[0][1], makeWav().length);
    assert.equal(calls[1][1], "سلام راوا");
    assert.equal(calls[2][1], "سلام! صدایت را دریافت کردم.");
  }, { speechProvider, aiProvider });
});

test("voice turn reports unavailable when speech or AI provider is missing", async () => {
  await withVoiceServer(async (base) => {
    const response = await fetch(`${base}/api/voice/turn`, {
      method: "POST",
      headers: {
        "content-type": "audio/wav",
        authorization: "Bearer test-device-token"
      },
      body: makeWav()
    });
    assert.equal(response.status, 503);
    assert.deepEqual(await response.json(), { error: "voice_provider_unavailable" });
  });
});

test("voice turn does not return partial audio when speech processing fails", async () => {
  const speechProvider = {
    transcribeAudio: async () => { throw new Error("speech provider unavailable"); },
    synthesizeSpeech: async () => makeWav()
  };
  const aiProvider = { completeChat: async () => "پاسخ" };

  await withVoiceServer(async (base) => {
    const response = await fetch(`${base}/api/voice/turn`, {
      method: "POST",
      headers: {
        "content-type": "audio/wav",
        authorization: "Bearer test-device-token"
      },
      body: makeWav()
    });
    assert.equal(response.status, 502);
    assert.deepEqual(await response.json(), { error: "voice_processing_failed" });
  }, { speechProvider, aiProvider });
});
