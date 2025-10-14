#include "web.h"
#include "types.h"
#include <GyverPortal.h>
#include <HardwareSerial.h>
#include <Preferences.h>

extern Config Cfg;
extern Preferences Pref;
extern volatile EssStatus Ess;

namespace WEB {

GyverPortal portal;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void buildPortal();
void onPortalUpdate();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "web_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[WEB] Task running in core %d.\n", (uint32_t)xPortGetCoreID());
  portal.enableOTA();
  portal.attachBuild(buildPortal);
  portal.attach(onPortalUpdate);
  portal.start(Cfg.hostname);

  while (1) {
    loop();
  }

  Serial.println("[WEB] Task exited.");
  vTaskDelete(NULL);
}

void loop() { portal.tick(); }

void buildPortal() {
  GP.BUILD_BEGIN(GP_DARK);
  GP.setTimeout(1000);
  GP.ONLINE_CHECK(10000);
  GP.UPDATE("state.charge,state.health,state.voltage,state.current,state."
            "temperature,state.limits",
            3000);
  GP.PAGE_TITLE(Cfg.hostname);
  GP.NAV_TABS("ESS,WiFi,Telegram,MQTT,System");
  // Status tab
  GP.NAV_BLOCK_BEGIN();
  GP.GRID_BEGIN();
  M_BLOCK(GP_TAB, "", "Charge", GP.TITLE("-", "state.charge"));
  M_BLOCK(GP_TAB, "", "Load", GP_DEFAULT, GP.TITLE("-", "state.current"));
  GP.GRID_END();
  GP.BLOCK_BEGIN(GP_THIN);
  GP.TABLE_BEGIN();
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Voltage"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.voltage");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Temperature"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.temperature");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Health"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.health");
  GP.TR();
  GP.TD(GP_LEFT);
  GP.PLAIN(F("Battery limits"));
  GP.TD(GP_RIGHT);
  GP.BOLD("-", "state.limits");
  GP.TABLE_END();
  GP.BLOCK_END();
  GP.HR();
  GP.SEND(VERSION);
  GP.NAV_BLOCK_END();
  // WiFi settings tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/wifi");
  GP.BOX_BEGIN();
  GP.SWITCH("wifi.sta", Cfg.wifiSTA);
  GP.LABEL("Connect to existing network");
  GP.BOX_END();
  GP.LABEL("SSID");
  GP.TEXT("wifi.ssid", "", Cfg.wifiSSID, "", 128);
  GP.LABEL("Password");
  GP.PASS("wifi.pass", "", Cfg.wifiPass, "", 128);
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // Telegram settings tab
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/tg");
  GP.BOX_BEGIN();
  GP.SWITCH("tg.enabled", Cfg.tgEnabled);
  GP.LABEL("Enable Telegram notifications");
  GP.BOX_END();
  GP.LABEL("Bot token");
  GP.PASS("tg.bot_token", "", Cfg.tgBotToken, "", sizeof(Cfg.tgBotToken));
  GP.LABEL("Chat ID");
  GP.TEXT("tg.chat_id", "", Cfg.tgChatID, "", sizeof(Cfg.tgChatID));

  GP.LABEL("Current threshold in Amps for alerts");
  char tgCurrentbuf[6];
  itoa(Cfg.tgCurrentThreshold, tgCurrentbuf, 10);
  GP.TEXT("tg.current_threshold", "", tgCurrentbuf, "", sizeof(tgCurrentbuf));
  
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // MQTT
  GP.NAV_BLOCK_BEGIN();
  GP.FORM_BEGIN("/mqtt");
  GP.BOX_BEGIN();
  GP.SWITCH("mqtt.enabled", Cfg.mqttEnabled);
  GP.LABEL("Enable MQTT");
  GP.BOX_END();
  GP.LABEL("Broker IP address");
  GP.TEXT("mqtt.broker_ip", "", Cfg.mqttBrokerIp, "", sizeof(Cfg.mqttBrokerIp));
  GP.BREAK();
  GP.SUBMIT("Save and reboot");
  GP.FORM_END();
  GP.NAV_BLOCK_END();
  // System tab
  GP.NAV_BLOCK_BEGIN();
  GP.SYSTEM_INFO();
  GP.OTA_FIRMWARE();
  GP.NAV_BLOCK_END();
  GP.BUILD_END();
}

void onPortalUpdate() {
  if (portal.update()) {
    if (portal.update("state.charge")) {
      portal.answer(String(Ess.charge) + "%");
    }
    if (portal.update("state.health")) {
      portal.answer(String(Ess.health) + "%");
    }
    if (portal.update("state.current")) {
      portal.answer(String(Ess.current, 1) + "A");
    }
    if (portal.update("state.temperature")) {
      portal.answer(String(Ess.temperature, 1) + "°C");
    }
    if (portal.update("state.voltage")) {
      portal.answer(String(Ess.voltage, 2) + " / " +
                    String(Ess.ratedVoltage, 2) + " V");
    }
    if (portal.update("state.limits")) {
      portal.answer(String(Ess.ratedChargeCurrent, 1) + " / " +
                    String(Ess.ratedDischargeCurrent, 1) + " A");
    }
  }
  if (portal.form()) {
    if (portal.form("/wifi")) {
      portal.copyBool("wifi.sta", Cfg.wifiSTA);
      portal.copyStr("wifi.ssid", Cfg.wifiSSID, sizeof(Cfg.wifiSSID));
      portal.copyStr("wifi.pass", Cfg.wifiPass, sizeof(Cfg.wifiPass));

      Pref.putBool(CFG_WIFI_STA, Cfg.wifiSTA);
      Pref.putString(CFG_WIFI_SSID, Cfg.wifiSSID);
      Pref.putString(CFG_WIFI_PASS, Cfg.wifiPass);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/tg")) {
      portal.copyBool("tg.enabled", Cfg.tgEnabled);
      portal.copyStr("tg.bot_token", Cfg.tgBotToken, sizeof(Cfg.tgBotToken));
      portal.copyStr("tg.chat_id", Cfg.tgChatID, sizeof(Cfg.tgChatID));

      int thr = (int)Cfg.tgCurrentThreshold; 
      portal.copyInt("tg.current_threshold", thr);
      if (thr < 0)   thr = 0;
      if (thr > 100) thr = 100;
      Cfg.tgCurrentThreshold = (uint8_t)thr;

      Pref.putBool(CFG_TG_ENABLED, Cfg.tgEnabled);
      Pref.putString(CFG_TG_BOT_TOKEN, Cfg.tgBotToken);
      Pref.putString(CFG_TG_CHAT_ID, Cfg.tgChatID);
      Pref.putUChar (CFG_TG_CURRENT_THRESHOLD, Cfg.tgCurrentThreshold);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    } else if (portal.form("/mqtt")) {
      portal.copyBool("mqtt.enabled", Cfg.mqttEnabled);
      portal.copyStr("mqtt.broker_ip", Cfg.mqttBrokerIp,
                     sizeof(Cfg.mqttBrokerIp));

      Pref.putBool(CFG_MQQTT_ENABLED, Cfg.mqttEnabled);
      Pref.putString(CFG_MQQTT_BROKER_IP, Cfg.mqttBrokerIp);

      Pref.end();
      backToWebRoot();
      needRestart = true;
    }
  }
}

void backToWebRoot() {
  portal.answer(F("<meta http-equiv='refresh' content='0; url=/' />"));
}

} // namespace WEB
