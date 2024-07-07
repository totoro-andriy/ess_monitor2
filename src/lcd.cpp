#include "types.h"
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>

extern Config Cfg;
extern volatile EssStatus Ess;

namespace LCD {

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
void draw();

U8G2_SSD1306_128X64_NONAME_F_HW_I2C *lcd = nullptr;

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "lcd_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[LCD] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  lcd = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0,
                                                /* reset=*/U8X8_PIN_NONE);
  lcd->begin();
  lcd->clear();

  while (1) {
    loop();
  }

  Serial.println("[LCD] Task exited.");
  vTaskDelete(NULL);
}

void loop() {
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  draw();
#ifdef DEBUG
  Serial.println("[LCD] Draw.");
#endif
}

void draw() {
  lcd->clearBuffer();
  lcd->setFont(u8g2_font_pressstart2p_8r);

  // Voltage
  lcd->drawStr(0, 10, "V");

  lcd->setCursor(12, 10);
  lcd->print(Ess.voltage);

  lcd->setCursor(70, 10);
  lcd->print(Ess.ratedVoltage);

  lcd->drawLine(0, 11, 128, 11);

  // Charge / health
  lcd->drawStr(0, 24, "C:");

  lcd->setCursor(16, 24);
  lcd->print(Ess.charge);
  lcd->drawStr(36, 24, "%");

  lcd->drawStr(70, 24, "H:");
  lcd->setCursor(86, 24);
  lcd->print(Ess.health);
  lcd->drawStr(110, 24, "%");

  // Temperature
  lcd->drawStr(12, 36, "Temp:");
  lcd->setCursor(52, 36);
  lcd->print(Ess.temperature);
  lcd->drawStr(102, 36, "C");

  // BMS errors or Wifi status
  if (Ess.bmsError || Ess.bmsWarning) {
    lcd->drawStr(0, 50, "ER/WR:");
    lcd->setCursor(50, 50);
    lcd->print(Ess.bmsError);
    lcd->setCursor(90, 50);
    lcd->print(Ess.bmsWarning);
  } else {
    if (Cfg.wifiSTA) {
      lcd->setCursor(0, 50);
      lcd->print(WiFi.localIP());
    } else {
      lcd->drawStr(0, 50, "ap:");
      lcd->drawStr(30, 50, Cfg.hostname);
    }
  }

  // Current & limits
  lcd->drawStr(0, 64, "A");
  lcd->setCursor(12, 64);
  lcd->print(Ess.current);
  lcd->setCursor(70, 64);
  lcd->print(Ess.ratedChargeCurrent, 0);
  lcd->setCursor(100, 64);
  lcd->print(Ess.ratedDischargeCurrent, 0);
  lcd->drawLine(0, 52, 128, 52);

  lcd->sendBuffer();
}

} // namespace LCD