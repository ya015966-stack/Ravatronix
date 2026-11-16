# Ravatronix ESP32-S3 firmware

ESP-IDF v5.3.2 application for the ESP32-S3-N16R8 controller.

## Build

From this directory in an ESP-IDF v5.3.2 shell:

```sh
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

The board profile uses 16 MB flash and 8 MB octal PSRAM. The initial
configuration selects DIO/40 MHz flash and 40 MHz PSRAM, matching the known
successful diagnostic boot profile. Component versions are pinned in
`idf_component.yml`.

## Recorded hardware map

Camera: XCLK 15, SCCB 4/5, data 16/17/18/12/10/8/9/11, VSYNC 6, HREF 7,
PCLK 13. Microphone: I2S0 BCLK/WS/DOUT 38/39/40. MAX98357A: I2S1
BCLK/WS/DIN 1/2/21. A7670E UART: ESP TX 48, ESP RX 47.

## Provisioning and safety

APN and backend URL/credentials are runtime configuration. Never commit SIM
PINs, bearer tokens, private keys, or certificates. Camera data is not exposed
through an unauthenticated public HTTP endpoint.

The older Arduino camera Wi-Fi sketch is retained as a legacy diagnostic only.
