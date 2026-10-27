/*
  RAVA TRONIX — ESP32-S3 serial-only smoke test
  Purpose: confirm the sketch compiles and the board boots.
  No GPIO, camera, modem, Wi-Fi, or external hardware is used.
  Open Serial Monitor at 115200 baud.
*/

#include <Arduino.h>

static unsigned long lastReportMs = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("================================");
  Serial.println("RAVA TRONIX ESP32-S3 SERIAL TEST");
  Serial.println("Mode: serial-only; no peripherals");
  Serial.println("================================");

  Serial.print("Chip model: ");
  Serial.println(ESP.getChipModel());

  Serial.print("Chip revision: ");
  Serial.println(ESP.getChipRevision());

  Serial.print("CPU MHz: ");
  Serial.println(ESP.getCpuFreqMHz());

  Serial.print("Flash bytes: ");
  Serial.println(ESP.getFlashChipSize());

  Serial.print("PSRAM bytes: ");
  Serial.println(ESP.getPsramSize());

  Serial.print("Free heap bytes: ");
  Serial.println(ESP.getFreeHeap());

  Serial.println("RAVA_TEST_BOOT_OK");
}

void loop() {
  const unsigned long now = millis();

  if (now - lastReportMs >= 5000) {
    lastReportMs = now;
    Serial.print("RAVA_TEST_OK uptime_ms=");
    Serial.print(now);
    Serial.print(" free_heap=");
    Serial.println(ESP.getFreeHeap());
  }

  delay(10);
}
