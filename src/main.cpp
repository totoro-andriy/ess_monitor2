#include "can.h"
#include "hass.h"
#include "lcd.h"
#include "tg.h"
#include "types.h"
#include "web.h"
#include <Arduino.h>
#include <IPAddress.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

Preferences Pref;
Config Cfg;
volatile EssStatus Ess;

void initConfig();

bool wifiEverConnected = false;

bool needRestart = false;

bool initWiFi();
void logBatteryState();

void setup() {
  Serial.begin(115200);

  initConfig();

  if (initWiFi()) {
    if (Cfg.mqttEnabled) {
      HASS::begin(1, 1);
    }
    if (Cfg.tgEnabled) {
      TG::begin(1, 1);
    }
  }

  CAN::begin(1, 1); // TODO: wait for success for tg and hass to start
  WEB::begin(1, 1);
  LCD::begin(1, 1);
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // WiFi reconnection
  wifiMulti.run();

  // Every 5 seconds
  if (currentMillis - previousMillis >= 5000 * 1) {
    previousMillis = currentMillis;
  
    logBatteryState();
    //SoftReset
    if (needRestart){ESP.restart();};
  }
}

void initConfig() {
  Pref.begin("ess");

  Cfg.wifiSTA = Pref.getBool(CFG_WIFI_STA, Cfg.wifiSTA);
  Pref.getString(CFG_WIFI_SSID, Cfg.wifiSSID, sizeof(Cfg.wifiSSID));
  Pref.getString(CFG_WIFI_PASS, Cfg.wifiPass, sizeof(Cfg.wifiPass));

  Pref.getString(CFG_HOSTNAME, Cfg.hostname, sizeof(Cfg.hostname));

  Cfg.chargeLimit = Pref.getUChar(CFG_INVERTER_CHARGE_LIMIT, Cfg.chargeLimit);
  Cfg.dishargeLimit =
      Pref.getUChar(CFG_INVERTER_DISCHARGE_LIMIT, Cfg.dishargeLimit);

  Cfg.mqttEnabled = Pref.getBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
  Pref.getString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp,
                 sizeof(Cfg.mqttBrokerIp));

  Cfg.tgEnabled = Pref.getBool(CFG_TG_ENABLED, Cfg.tgEnabled);
  Pref.getString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken, sizeof(Cfg.tgBotToken));
  Pref.getString(CFG_TG_CHAT_ID, Cfg.tgChatID, sizeof(Cfg.tgChatID));
  Cfg.tgCurrentThreshold = Pref.getUChar(CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);
}

bool initWiFi() {
  if (Cfg.wifiSTA && Cfg.wifiSSID != NULL && Cfg.wifiSSID[0] != '\0') {
    Serial.printf("Connecting to %s...", Cfg.wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(Cfg.hostname);

    // Reset WiFiMulti
    wifiMulti = WiFiMulti();
    wifiMulti.addAP(Cfg.wifiSSID, Cfg.wifiPass);

    uint32_t previousMillis = millis();
    while (millis() - previousMillis < 20000) {
      if (wifiMulti.run() == WL_CONNECTED) {
        Serial.println(" OK.");
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.printf("mDNS hostname: '%s.local'\n", Cfg.hostname);
        wifiEverConnected = true;
        return true;
      }
      delay(500);
      Serial.print(".");
    }

    Serial.println("Could not connect to WiFi network in 20 seconds.");
  }

  if (!wifiEverConnected) {
    Serial.println("Starting WiFi access point.");
    IPAddress AP_IP(192, 168, 4, 1);
    IPAddress AP_PFX(255, 255, 255, 0);
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_PFX);
    WiFi.softAP(Cfg.hostname, "12345678");
    Serial.printf("Access point SSID: '%s'\n", Cfg.hostname);
    return false;
  }

  return false;
}

void logBatteryState() {
#ifdef DEBUG
  Serial.printf("[MAIN] Load %.1f | RC %.1f/%.1f | SOC %d%% | SOH %d%% | T "
                "%.1f°C | V %.2f/%.2f\n",
                Ess.current, Ess.ratedChargeCurrent, Ess.ratedDischargeCurrent,
                Ess.charge, Ess.health, Ess.temperature, Ess.voltage,
                Ess.ratedVoltage);
#endif
}
