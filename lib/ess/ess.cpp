#include "ess.h"

namespace Ess {

namespace {

TaskHandle_t *taskHandle;
MCP_CAN *can;
BatteryState state;
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

void task(void *pvParameters);
void setup();
void loop();
void initCAN();
void readCAN();
void writeCAN();
void processDataFrame(DataFrame *f, BatteryState *b);
DataFrame getChargeDataFrame(BatteryState *b);
uint8_t getChargeControlByte(BatteryState *b);

void task(void *pvParameters) {
  setup();

  while (1) {
    loop();
  }

  vTaskDelete(NULL);
};

void setup() {
  Serial.println(F("Executing ESS Setup routine..."));

  state = BatteryState{};
  initCAN();
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

void initCAN() {
  Serial.print(F("Initializing MCP2515..."));

  MCP_CAN c(5);
  can = &c;

  if (can->begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(F(" OK."));
  } else {
    Serial.println(F(" ERROR!"));
  }
  can->setMode(MCP_NORMAL);
  attachInterrupt(CAN0_INT, readCAN, LOW);
  pinMode(CAN0_INT, INPUT);
}

void readCAN() {
  // if (digitalRead(CAN0_INT)) {
  //   // CAN0_INT high state means there is nothing to read
  //   return;
  // }

  // DataFrame frame = {};
  // this->can->readMsgBuf((unsigned long *)&frame.id, &frame.dlc, frame.data);

  DataFrame frame = {.id = 0x355,
                     .dlc = 8,
                     .data = {(uint8_t)esp_random(), (uint8_t)esp_random(),
                              (uint8_t)esp_random(), (uint8_t)esp_random(),
                              (uint8_t)esp_random(), (uint8_t)esp_random(),
                              (uint8_t)esp_random(), (uint8_t)esp_random()}};
  processDataFrame(&frame, &state);
}

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame(&state);

  can->sendMsgBuf(DF_35E.id, DF_35E.dlc, (uint8_t *)DF_35E.data);
  can->sendMsgBuf(DF_305.id, DF_305.dlc, (uint8_t *)DF_305.data);
  can->sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
}

int16_t bytesToInt16(uint8_t low, uint8_t high) { return (high << 8) | low; }

void processDataFrame(DataFrame *f, BatteryState *b) {
  portENTER_CRITICAL(&stateMux);
  switch (f->id) {
  case 849: // 0x351 Battery Limits
    b->ratedVoltage = bytesToInt16(f->data[0], f->data[1]);
    b->ratedChargeCurrent = bytesToInt16(f->data[2], f->data[3]);
    b->ratedDischargeCurrent = bytesToInt16(f->data[4], f->data[5]);
    break;
  case 853: // 0x355 Battery Health
    b->charge = bytesToInt16(f->data[0], f->data[1]);
    b->health = bytesToInt16(f->data[2], f->data[3]);
    break;
  case 854: // 0x356 System Voltage, Current, Temp
    b->voltage = bytesToInt16(f->data[0], f->data[1]);
    b->current = bytesToInt16(f->data[2], f->data[3]);
    b->temperature = bytesToInt16(f->data[4], f->data[5]);
    break;
  case 857: // 0x359 BMS Error
    b->bmsWarning = f->data[1];
    b->bmsError = f->data[3];
    break;
  }
  portEXIT_CRITICAL(&stateMux);
}

uint8_t getChargeControlByte(BatteryState *b) {
  uint8_t res = 0;
  if (false) { // TODO: if(data.full_charge_time)
    res |= (1 << 3);
  }
  if (b->ratedDischargeCurrent != 0 && b->charge >= 30) {
    // Battery allows discharging and discharge policy is ok.
    // TODO: read target discharge value from settings
    res |= (1 << 6);
  }
  if (b->ratedChargeCurrent != 0 && b->charge <= 98) {
    // Battery allows charging and charge policy is ok.
    // TODO: read target charge value from settings
    res |= (1 << 7);
  }
  return res;
}

DataFrame getChargeDataFrame(BatteryState *b) {
  DataFrame f = {0x35c, 1, {0, getChargeControlByte(b)}};
  return f;
}

} // namespace

void begin() {
  xTaskCreatePinnedToCore(task, "ess_task", 4000, NULL, 1, taskHandle, 1);
}

BatteryState getState() { return state; }

} // namespace Ess
