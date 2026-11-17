import test from "node:test";
import assert from "node:assert/strict";
import { createAzureSpeechProvider } from "../src/azure-speech-provider.js";

test("Azure Speech provider is unavailable without key and region", () => {
  assert.equal(createAzureSpeechProvider({ apiKey: "", region: "westeurope" }), null);
  assert.equal(createAzureSpeechProvider({ apiKey: "secret", region: "" }), null);
});

test("Azure Speech provider transcribes Persian WAV using fa-IR", async () => {
  let request;
  const provider = createAzureSpeechProvider({
    apiKey: "test-secret",
    region: "westeurope",
    fetchImpl: async (url, options) => {
      request = { url: String(url), options };
      return new Response(JSON.stringify({
        RecognitionStatus: "Success",
        DisplayText: "سلام راوا"
      }), { status: 200, headers: { "content-type": "application/json" } });
    }
  });

  const transcript = await provider.transcribeAudio({
    audio: Buffer.from("test-audio"),
    mimeType: "audio/wav"
  });

  assert.equal(transcript, "سلام راوا");
  assert.match(request.url, /speech\/recognition\/conversation\/cognitiveservices\/v1/);
  assert.equal(new URL(request.url).searchParams.get("language"), "fa-IR");
  assert.equal(request.options.headers["Ocp-Apim-Subscription-Key"], "test-secret");
  assert.equal(request.options.headers["Content-Type"], "audio/wav");
});

test("Azure Speech provider synthesizes Persian as 16 kHz PCM WAV", async () => {
  let request;
  const wav = Buffer.from("RIFF-test");
  const provider = createAzureSpeechProvider({
    apiKey: "test-secret",
    region: "westeurope",
    fetchImpl: async (url, options) => {
      request = { url: String(url), options };
      return new Response(wav, { status: 200 });
    }
  });

  const result = await provider.synthesizeSpeech({ text: "سلام & راوا" });

  assert.deepEqual(result, wav);
  assert.match(request.url, /cognitiveservices\/v1$/);
  assert.equal(request.options.headers["X-Microsoft-OutputFormat"], "riff-16khz-16bit-mono-pcm");
  assert.match(request.options.body, /xml:lang="fa-IR"/);
  assert.match(request.options.body, /سلام &amp; راوا/);
  assert.equal(request.options.headers["Ocp-Apim-Subscription-Key"], "test-secret");
});

test("Azure Speech provider rejects unsuccessful recognition responses", async () => {
  const provider = createAzureSpeechProvider({
    apiKey: "test-secret",
    region: "westeurope",
    fetchImpl: async () => new Response(JSON.stringify({
      RecognitionStatus: "NoMatch",
      DisplayText: ""
    }), { status: 200 })
  });

  await assert.rejects(
    provider.transcribeAudio({ audio: Buffer.from("x"), mimeType: "audio/wav" }),
    /did not recognize speech/
  );
});

test("Azure Speech provider surfaces non-success HTTP status without response body", async () => {
  const provider = createAzureSpeechProvider({
    apiKey: "test-secret",
    region: "westeurope",
    fetchImpl: async () => new Response("sensitive upstream body", { status: 401 })
  });

  await assert.rejects(
    provider.synthesizeSpeech({ text: "سلام" }),
    /HTTP 401/
  );
});
