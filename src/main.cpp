#include "ess.h"
#include <Arduino.h>
#include <WiFi.h>

#define DEBUG 1
#define HOSTNAME "ess_monitor"

#define SSID "SHoCK"
#define PASS "gfhjkmyfWIFI13"

void logBatteryState();

void setup() {
  Serial.begin(115200);

  Serial.print("Conecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK.");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString());

  Ess::begin();
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Every 1 second
  if (currentMillis - previousMillis >= 1000 * 1) {
    previousMillis = currentMillis;

    // writeCAN();

    if (DEBUG) {
      logBatteryState();
    }
  }
}

void logBatteryState() {
  Ess::BatteryState Battery = Ess::getState();

  Serial.printf("Load: %.1f amps\t", Battery.current / 10.0);
  Serial.printf("RC: %.1f/%.1f\t", Battery.ratedChargeCurrent / 10.0,
                Battery.ratedDischargeCurrent / 10.0);
  Serial.printf("SOC: %d%% SOH: %d%% T: %d\t", Battery.charge, Battery.health,
                Battery.temperature);
  Serial.printf("V: %.2f of %.2f\n", Battery.voltage / 100.0,
                Battery.ratedVoltage / 10.0);
}
