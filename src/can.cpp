#include "can.h"
#include "types.h"
#include <HardwareSerial.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mcp_can.h>
#include <stdint.h>

extern volatile EssStatus Ess;

namespace CAN {

MCP_CAN can(CS_PIN);
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
bool initCAN();
void readCAN();
void writeCAN();
void logReadDataFrame(DataFrame *f);
void logWriteDataFrame(DataFrame *f);
int16_t bytesToInt16(uint8_t low, uint8_t high);
void processDataFrame(DataFrame *f);
uint8_t getChargeControlByte();
DataFrame getChargeDataFrame();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "can_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[CAN] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  if (initCAN()) {
    while (1) {
      loop();
    }
  }

  Serial.println("[CAN] Task exited.");
  vTaskDelete(NULL);
};

bool initCAN() {
  if (can.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    can.setMode(MCP_NORMAL);
    attachInterrupt(INT_PIN, readCAN, LOW);
    pinMode(INT_PIN, INPUT);
    Serial.println(F("[CAN] Initializing MCP2515... OK."));
    return true;
  }
  Serial.println(F("[CAN] Initializing MCP2515... ERROR!"));
  return false;
}

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  readCAN();

  // Every 1 second
  if (currentMillis - previousMillis >= 1000 * 1) {
    previousMillis = currentMillis;
    writeCAN();
  }
}

void readCAN() {
  if (digitalRead(INT_PIN)) {
    // INT_PIN high state means there is nothing to read
    return;
  }

  DataFrame f = {};
  can.readMsgBuf((unsigned long *)&f.id, &f.dlc, f.data);
  // logReadDataFrame(&f);
  processDataFrame(&f);

  // vTaskDelay(1000 / portTICK_PERIOD_MS);

  // // Charge/health
  // DataFrame f = {.id = 0x355, .dlc = 8, .data = {98, 0, 94, 0}};
  // logReadDataFrame(&f);
  // processDataFrame(&f);

  // // Battery limits
  // DataFrame f2 = {.id = 0x351,
  //                 .dlc = 8,
  //                 .data = {0x46, 0x02, 0x54, 0x01, 0xEC, 0x04, 0, 0}};

  // logReadDataFrame(&f2);
  // processDataFrame(&f2);

  // // Voltage, Current, Temp
  // uint32_t m = millis();
  // if (m < 30000) {
  //   DataFrame f3 = {
  //       .id = 0x356, .dlc = 8, .data = {0x8A, 0x16, 0x05, 0x00, 0xFD}};
  //   logReadDataFrame(&f3);
  //   processDataFrame(&f3);
  // } else if (m < 60000) {
  //   DataFrame f3 = {
  //       .id = 0x356, .dlc = 8, .data = {0x8A, 0x16, 0x32, 0x00, 0xFD}};
  //   logReadDataFrame(&f3);
  //   processDataFrame(&f3);
  // } else {
  //   DataFrame f3 = {
  //       .id = 0x356, .dlc = 8, .data = {0x8A, 0x16, 0xA4, 0xFF, 0xFD}};
  //   logReadDataFrame(&f3);
  //   processDataFrame(&f3);
  // }
}

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame();

  // logWriteDataFrame((DataFrame *)&DF_35E);
  can.sendMsgBuf(DF_35E.id, DF_35E.dlc, (uint8_t *)DF_35E.data);
  logWriteDataFrame((DataFrame *)&DF_305);
  can.sendMsgBuf(DF_305.id, DF_305.dlc, (uint8_t *)DF_305.data);
  // logWriteDataFrame(&chargeFrame);
  can.sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
}

void logReadDataFrame(DataFrame *f) {
#ifdef DEBUG
  Serial.printf("[CAN] Frame received:\t <- ID: %04x DLC: %d Data: "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
                f->data[4], f->data[5], f->data[6], f->data[7]);
#endif
}

void logWriteDataFrame(DataFrame *f) {
#ifdef DEBUG
  Serial.printf("[CAN] Frame sent:\t -> ID: %04x DLC: %d Data: "
                "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
                f->data[4], f->data[5], f->data[6], f->data[7]);
#endif
}

int16_t bytesToInt16(uint8_t low, uint8_t high) { return (high << 8) | low; }

void processDataFrame(DataFrame *f) {
  portENTER_CRITICAL(&stateMux);
  switch (f->id) {
  case 849: // 0x351 Battery Limits
    Ess.ratedVoltage = bytesToInt16(f->data[0], f->data[1]) / 10.0;
    Ess.ratedChargeCurrent = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.ratedDischargeCurrent = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    break;
  case 853: // 0x355 Battery Health
    Ess.charge = bytesToInt16(f->data[0], f->data[1]);
    Ess.health = bytesToInt16(f->data[2], f->data[3]);
    break;
  case 854: // 0x356 System Voltage, Current, Temp
    Ess.voltage = bytesToInt16(f->data[0], f->data[1]) / 100.0;
    Ess.current = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.temperature = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    break;
  case 857: // 0x359 BMS Error
    Ess.bmsWarning = f->data[1];
    Ess.bmsError = f->data[3];
    break;
  }
  portEXIT_CRITICAL(&stateMux);
}

uint8_t getChargeControlByte() {
  uint8_t res = 0;
  if (false) { // TODO: if(data.full_charge_time)
    res |= (1 << 3);
  }
  if (Ess.ratedDischargeCurrent != 0 && Ess.charge > 30) {
    // Battery allows discharging and discharge policy is ok.
    // TODO: read target discharge value from settings
    res |= (1 << 6);
  }
  if (Ess.ratedChargeCurrent != 0 && Ess.charge < 98) {
    // Battery allows charging and charge policy is ok.
    // TODO: read target charge value from settings
    res |= (1 << 7);
  }
  return res;
}

DataFrame getChargeDataFrame() {
  DataFrame f = {0x35c, 2, {getChargeControlByte(), 0x00}};
  return f;
}

} // namespace CAN
