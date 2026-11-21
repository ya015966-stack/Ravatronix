# RAVA voice client integration

The ESP32-S3 voice client implements the backend contract in backend/README.md:

- capture a bounded 250–8000 ms microphone segment at 16 kHz;
- convert current 32-bit I2S RX samples to PCM16 using an explicit caller-selected right shift;
- package mono PCM16 in a RIFF/WAVE file;
- upload to POST /api/voice/turn using HTTPS, the ESP certificate bundle, and a runtime bearer token;
- bound the response to 1 MiB, validate PCM16/mono/16 kHz WAV metadata, and play it through the MAX98357A output.

The provider API key remains on the backend. The device token is passed to voice_client_turn at runtime; it is not a firmware default or source-code constant. The application does not start recording automatically at boot. A trusted control-plane event, such as Android Push-to-Talk, must call voice_client_turn with provisioned endpoint/token values.

## Current integration status

This component is an explicit callable firmware API. It is not yet wired to Android Push-to-Talk or a physical button, and app startup does not capture audio automatically. The A7670E link must provide an IP interface usable by ESP-IDF esp_http_client; an AT NETOPEN diagnostic alone does not prove PPP/IP routing into the ESP32 lwIP stack. Confirm the active network mode before enabling this call on the robot.

The 32-bit microphone-to-16-bit conversion shift is configurable. Shift 14 matches the current firmware diagnostic RMS calculation, but must still be checked against a captured INMP441 waveform before production use.

## Host test

Run pytest on test_voice_client_contract.py and test_wav_codec.py in firmware/esp32_s3/tests/host. The WAV codec test compiles production C with address/undefined-behavior sanitizers and checks header creation, parsing, and sample saturation.
