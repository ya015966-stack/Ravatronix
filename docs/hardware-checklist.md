# RAVA Hardware Verification Checklist

## Controller
- [ ] Photograph/record the exact ESP32-S3-N16R8 board label and both USB port labels.
- [ ] Confirm flash size and PSRAM specification from the board/module documentation.
- [ ] Record the working flash method, selected board, partition scheme, and serial log.

## Camera
- [ ] Confirm the sensor marking and FFC orientation.
- [ ] Find the exact board pin map for SCCB/I2C, XCLK, PCLK, VSYNC, HREF, and data pins.
- [ ] Run a camera-only initialization/capture test.

## A7670E modem
- [ ] Confirm carrier-board model and power-input voltage.
- [ ] Confirm UART pins, baud rate, and logic voltage.
- [ ] Confirm SIM format, antenna connection, and operator/APN settings.
- [ ] Test modem AT response before data networking.

## Power and safety
- [ ] Keep the modem supply separate from weak MCU regulator outputs.
- [ ] Verify polarity before connecting the battery.
- [ ] Do not connect/disconnect FFC or wiring while powered.
- [ ] Add a hardware motor-disable/stop path before enabling remote drive commands.

## Network and security
- [ ] Keep secrets in local environment variables or a secret manager.
- [ ] Require authentication for camera and control APIs.
- [ ] Do not publish raw modem/SIM identifiers or access tokens in repository logs.
