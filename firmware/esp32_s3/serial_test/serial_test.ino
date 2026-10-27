/*
  RAVA — ESP32-S3 serial-only smoke test
  Purpose: confirm that a basic sketch compiles, flashes, and runs.
  No GPIO, camera, modem, Wi-Fi, or external hardware is used.
  Open Serial Monitor at 115200 baud.
*/

#include <Arduino.h>

static unsigned long lastReportMs = 0;
static unsigned long bootCount = 0;

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("================================");
  Serial.println("RAVA ESP32-S3 SERIAL TEST");
  Serial.println("Mode: serial-only; no peripherals");
  Serial.println("Target: confirm boot and serial");
  Serial.println("================================");

  bootCount++;
  Serial.print("Boot marker: ");
  Serial.println(bootCount);
  Serial.println("If this repeats after reset, serial output is working.");
}

void loop() {
  const unsigned long now = millis();

  if (now - lastReportMs >= 5000) {
    lastReportMs = now;
    Serial.print("RAVA_TEST_OK uptime_ms=");
    Serial.println(now);
  }

  delay(10);
}
