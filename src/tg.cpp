#include "tg.h"
#include "types.h"
#include <FastBot.h>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern Config Cfg;
extern volatile EssStatus Ess;

namespace TG {

typedef enum State : int {
  Undef = 0,
  Charging = 1,
  Discharging = 2,
  Balance = 3
} State;

FastBot bot;
State state = State::Undef;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void onMessage(FB_msg &msg);
String getStatusMsg();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "tg_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[TG] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  bot.setToken(Cfg.tgBotToken);
  bot.setChatID(Cfg.tgChatID);
  bot.setTextMode(FB_MARKDOWN);
  bot.attach(onMessage);

  vTaskDelay(1000 * 30 / portTICK_PERIOD_MS);

  while (1) {
    loop();
  }

  Serial.println("[TG] Task exited.");
  vTaskDelete(NULL);
}

void loop() {
  static int16_t previousCurrent = 0;
  static State previousState = State::Undef;
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Every 5 seconds
  if (currentMillis - previousMillis >= 1000 * 5) {
    previousMillis = currentMillis;

    if (Ess.current > (int)Cfg.tgCurrentThreshold) {
      state = State::Charging;
    } else if (Ess.current < -(int)Cfg.tgCurrentThreshold) {
      state = State::Discharging;
    } else {
      // Let's count current deviations in a tgCurrentThreshold range as a balanced state
      state = State::Balance;
    }

    if (state != previousState && previousState != State::Undef) {
      // The state has changed from one known value to another
      switch (state) {
      case State::Discharging:
        bot.sendMessage("🕯️ *Переключено на живлення від батарейки.* Грилі не "
                        "смажимо.\n\n||" +
                        getStatusMsg() + "||");
        break;
      case State::Charging:
        bot.sendMessage("💡 *Електрохарчування відновлено.*\n\n||" +
                        getStatusMsg() + "||");
        break;
      default:
        break;
      }
    }

    previousState = state;
  }

  bot.tick();
}

void onMessage(FB_msg &msg) {
#ifdef DEBUG
  Serial.println("[TG] Message received: " + msg.toString());
#endif

  if (msg.text == "/status" || msg.text.startsWith("/status@")) {
    bot.sendMessage(getStatusMsg(), msg.chatID);
  }
}

String getStatusMsg() {
  String s;
  switch (state) {
  case State::Balance:
    s = "⚪️ Статус: *простій*.\n\n";
    break;
  case State::Charging:
    s = "🟢 Статус: *заряджання*.\n\n";
    break;
  case State::Discharging:
    s = "🔴 Статус: *розряджання*.\n\n";
    break;
  default:
    s = "🟡 Статус: *невизначений*.\n\n";
    break;
  }
  if (Ess.charge > 75) {
    s += "🟩🟩🟩🟩";
  } else if (Ess.charge > 50) {
    s += "🟩🟩🟩🟦";
  } else if (Ess.charge > 25) {
    s += "🟩🟩🟦🟦";
  } else {
    s += "🟩🟦🟦🟦";
  }
  s += " Заряд: *" + String(Ess.charge) + "%*\n";
  s += "🔌 Навантаження: *" + String(Ess.current, 1) + "A*\n";
  s += "⚡️ Напруга: *" + String(Ess.voltage, 2) + "V*, номінальна: *" +
       String(Ess.ratedVoltage, 2) + "V*\n";
  s += "🌡️ Температура батареї: *" + String(Ess.temperature, 1) + "°C*\n";
  s += "🍀 Здоров'я батареї: *" + String(Ess.health) + "%*\n";

#ifdef DEBUG
  Serial.println(s);
#endif

  return s;
}

} // namespace TG
