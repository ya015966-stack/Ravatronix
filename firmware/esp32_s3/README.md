# Ravatronix ESP32-S3 integrated firmware

ESP-IDF v5.3.2 application for the ESP32-S3-N16R8.

## Included services

- OV3660 one-shot JPEG capture using PSRAM frame buffers.
- I2S microphone capture and MAX98357A playback interface.
- A7670E UART DTE and PPPoS network startup through Espressif `esp_modem`.
- HTTPS JPEG upload client with certificate-bundle validation and a runtime bearer token.
- Periodic serial health, camera-capture, and microphone RMS diagnostics.

## Build

From this directory in an ESP-IDF v5.3.2 shell:

```sh
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

The board profile selects 16 MB flash and 8 MB octal PSRAM, with DIO/40 MHz
flash and 40 MHz PSRAM based on the known-good diagnostic boot. The component
versions are pinned in `idf_component.yml`.

## Recorded hardware map

Camera: XCLK 15, SCCB 4/5, data 16/17/18/12/10/8/9/11, VSYNC 6, HREF 7,
PCLK 13. Microphone: I2S0 BCLK/WS/DOUT 38/39/40. MAX98357A: I2S1
BCLK/WS/DIN 1/2/21. A7670E UART: ESP TX 48, ESP RX 47.

## Runtime integration

The cellular APN defaults to the public Irancell APN `mtnirancell`; it can be
changed from project configuration. The backend client accepts an HTTPS URL
and bearer token at runtime. No backend URL or token is embedded in the image.
The Android/backend provisioning path must supply those values before snapshot
uploads can be used. The firmware does not expose an unauthenticated camera
server or public MJPEG stream.

The older Arduino camera Wi-Fi sketch is retained as a legacy diagnostic only.
