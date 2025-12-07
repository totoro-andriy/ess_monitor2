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

volatile uint32_t timestamp_frame351=0;
volatile uint32_t timestamp_frame355=0;
volatile uint32_t timestamp_frame356=0;
volatile uint32_t timestamp_frame359=0;

static uint32_t last305OkMs = 0;
static const uint32_t WD_TIMEOUT_MS = 5UL * 60UL * 1000UL; // 5 хвилин

MCP_CAN can(CS_PIN);
portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

TaskHandle_t canTaskHandle = nullptr;
void IRAM_ATTR onCanInt();

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();
bool initCAN();
void readCAN();
void writeCAN();
void logReadDataFrame(DataFrame *f);
// void logWriteDataFrame(DataFrame *f);
int16_t bytesToInt16(uint8_t low, uint8_t high);
void processDataFrame(DataFrame *f);
uint8_t getChargeControlByte();
DataFrame getChargeDataFrame();

bool sendFrameChecked(const DataFrame &f) {
  const uint8_t maxRetries = 3;

  for (uint8_t i = 0; i < maxRetries; i++) {
    byte rc = can.sendMsgBuf(f.id, f.dlc, (uint8_t *)f.data);
    if (rc == CAN_OK) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

#ifdef DEBUG
  Serial.printf("[CAN] ERROR: sendMsgBuf(0x%03X) failed after %u retries\n",
                f.id, maxRetries);
#endif
  return false;
}

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "can_task", 20000,
                          NULL, priority, &canTaskHandle, core);
}

bool initCAN() {
  if (can.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    can.setMode(MCP_NORMAL);
    pinMode(INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(INT_PIN), CAN::onCanInt, FALLING);
    Serial.println(F("[CAN] Initializing MCP2515... OK."));
    return true;
  }
  Serial.println(F("[CAN] Initializing MCP2515... ERROR!"));
  return false;
}

void IRAM_ATTR onCanInt() {
  if (canTaskHandle == nullptr) {
    return;
  }

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(canTaskHandle, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void task(void *pvParameters) {
  Serial.printf("[CAN] Task running in core %d.\n", (uint32_t)xPortGetCoreID());

  if (!initCAN()) {
    Serial.println("[CAN] Task exited due to init error.");
    vTaskDelete(NULL);
    return;
  }

  uint32_t previousWriteMillis = millis();

  while (1) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
    readCAN();
    uint32_t now = millis();
    if (now - previousWriteMillis >= 1000) {
      previousWriteMillis = now;
      writeCAN();
    }
  }

  Serial.println("[CAN] Task exited.");
  vTaskDelete(NULL);
}

void loop() {
  
}

void readCAN() {
  DataFrame f = {};

  while (can.checkReceive() == CAN_MSGAVAIL) {
    if (can.readMsgBuf((unsigned long *)&f.id, &f.dlc, f.data) == CAN_OK) {
      processDataFrame(&f);
    } else {
      break;
    }
  }
}

// void writeCAN() {
// DataFrame chargeFrame = getChargeDataFrame();

// // logWriteDataFrame((DataFrame *)&DF_35E);
// can.sendMsgBuf(DF_35E.id, DF_35E.dlc, (uint8_t *)DF_35E.data);
// logWriteDataFrame((DataFrame *)&DF_305);
// can.sendMsgBuf(DF_305.id, DF_305.dlc, (uint8_t *)DF_305.data);
// // logWriteDataFrame(&chargeFrame);
// can.sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
// }

// void logReadDataFrame(DataFrame *f) {
// #ifdef DEBUG
// Serial.printf("[CAN] Frame received:\t <- ID: %04x DLC: %d Data: "
//               "%02x %02x %02x %02x %02x %02x %02x %02x\n",
//               f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
//               f->data[4], f->data[5], f->data[6], f->data[7]);
// #endif
// }

// void logWriteDataFrame(DataFrame *f) {
// #ifdef DEBUG
//   Serial.printf("[CAN] Frame sent:\t -> ID: %04x DLC: %d Data: "
//                 "%02x %02x %02x %02x %02x %02x %02x %02x\n",
//                 f->id, f->dlc, f->data[0], f->data[1], f->data[2], f->data[3],
//                 f->data[4], f->data[5], f->data[6], f->data[7]);
// #endif
// }

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame();
  (void)sendFrameChecked(DF_35E);
  if (sendFrameChecked(DF_305)) {
    last305OkMs = millis();
  } else {
  #ifdef DEBUG
    Serial.println("[CAN] WARNING: 0x305 send failed");
  #endif
  }
  if (last305OkMs != 0 && (millis() - last305OkMs > WD_TIMEOUT_MS)) {
    Serial.println("[CAN] ERROR: No successful 0x305 for 5 minutes — restarting ESP!");
    ESP.restart();
  }
  (void)sendFrameChecked(chargeFrame);
}

int16_t bytesToInt16(uint8_t low, uint8_t high) { return (high << 8) | low; }

void processDataFrame(DataFrame *f) {
  portENTER_CRITICAL(&stateMux);
  switch (f->id) {
  case 849: // 0x351 Battery Limits
    Ess.ratedVoltage = bytesToInt16(f->data[0], f->data[1]) / 10.0;
    Ess.ratedChargeCurrent = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.ratedDischargeCurrent = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    timestamp_frame351 = millis();
    break;
  case 853: // 0x355 Battery Health
    Ess.charge = bytesToInt16(f->data[0], f->data[1]);
    Ess.health = bytesToInt16(f->data[2], f->data[3]);
    timestamp_frame355 = millis();
    break;
  case 854: // 0x356 System Voltage, Current, Temp
    Ess.voltage = bytesToInt16(f->data[0], f->data[1]) / 100.0;
    Ess.current = bytesToInt16(f->data[2], f->data[3]) / 10.0;
    Ess.temperature = bytesToInt16(f->data[4], f->data[5]) / 10.0;
    timestamp_frame356 = millis();
    break;
  case 857: // 0x359 BMS Error
    Ess.bmsWarning = f->data[1];
    Ess.bmsError = f->data[3];
    timestamp_frame359 = millis();
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
