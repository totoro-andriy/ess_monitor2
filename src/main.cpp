#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

#define DEBUG 1
#define HOSTNAME "ess_monitor"
#define CAN0_INT 15

typedef struct BatteryState {
  int16_t charge, health, voltage, current, ratedVoltage, ratedChargeCurrent,
      ratedDischargeCurrent, temperature;
  byte bmsWarning, bmsError;
} BatteryState;

typedef struct DataFrame {
  uint32_t id;
  byte dlc;
  byte data[8];
} DataFrame;

/*
 * Prototypes
 */
void initCAN();
void readCAN();
void writeCAN();
void processDataFrame(DataFrame *frame);
byte getChargeByte();
DataFrame getChargeDataFrame();
void logBatteryState();

/*
 * Known CAN data frames
 */

// Deye protocol id
const DataFrame DF_35E = {0x35e, 8, {'P', 'Y', 'L', 'O', 'N', ' ', ' ', ' '}};
// Sunny Island optional id
const DataFrame DF_370 = {0x370, 8, {'P', 'Y', 'L', 'O', 'N', 'D', 'I', 'Y'}};
// Keep alive
const DataFrame DF_305 = {0x305, 1, {0x21}};
// No idea
const DataFrame DF_35A = {0x35a, 8};
// One Battery (Luxpower)
const DataFrame DF_379 = {0x379, 1, {0x7e}};

BatteryState Battery;
MCP_CAN CAN0(5);

void setup() {
  Serial.begin(115200);

  initCAN();
}

void loop() {
  static unsigned long previousMillis;
  unsigned long currentMillis = millis();

  readCAN();

  // Every 1 second
  if (currentMillis - previousMillis >= 1000 * 1) {
    previousMillis = currentMillis;

    writeCAN();

    if (DEBUG) {
      logBatteryState();
    }
  }
}

void initCAN() {
  Serial.print("Initializing MCP2515...");

  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(" OK.");
  } else {
    Serial.println(" ERROR!");
  }
  CAN0.setMode(MCP_NORMAL);
  attachInterrupt(CAN0_INT, readCAN, LOW);
  pinMode(CAN0_INT, INPUT);
}

void readCAN() {
  if (!digitalRead(CAN0_INT)) { // If CAN0_INT pin is low, read receive buffer
    DataFrame frame = {};
    CAN0.readMsgBuf((unsigned long *)&frame.id, &frame.dlc, frame.data);
    processDataFrame(&frame);
  }
}

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame();

  CAN0.sendMsgBuf(DF_35E.id, DF_35E.dlc, (byte *)DF_35E.data);
  CAN0.sendMsgBuf(DF_305.id, DF_305.dlc, (byte *)DF_305.data);
  CAN0.sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
}

byte getChargeByte() {
  byte res = 0;
  if (false) { // TODO: if(data.full_charge_time)
    res |= (1 << 3);
  }
  if (Battery.ratedDischargeCurrent != 0 && Battery.charge >= 30) {
    // Battery allows discharging and discharge policy is ok.
    // TODO: read target discharge value from settings
    res |= (1 << 6);
  }
  if (Battery.ratedChargeCurrent != 0 && Battery.charge <= 98) {
    // Battery allows charging and charge policy is ok.
    // TODO: read target charge value from settings
    res |= (1 << 7);
  }
  return res;
}

DataFrame getChargeDataFrame() {
  return DataFrame{0x35c, 1, {0, getChargeByte()}};
}

void processDataFrame(DataFrame *frame) {
  switch (frame->id) {
  case 849: // 0x351 Battery Limits
    Battery.ratedVoltage = (frame->data[1] << 8) | frame->data[0];
    Battery.ratedChargeCurrent = (frame->data[3] << 8) | frame->data[2];
    Battery.ratedDischargeCurrent = (frame->data[5] << 8) | frame->data[4];
    break;
  case 853: // 0x355 Battery Health
    Battery.charge = (frame->data[1] << 8) | frame->data[0];
    Battery.health = (frame->data[3] << 8) | frame->data[2];
    break;
  case 854: // 0x356 System Voltage, Current, Temp
    Battery.voltage = (frame->data[1] << 8) | frame->data[0];
    Battery.current = (frame->data[3] << 8) | frame->data[2];
    Battery.temperature = (frame->data[5] << 8) | frame->data[4];
    break;
  case 857: // 0x359 BMS Error
    Battery.bmsWarning = frame->data[1];
    Battery.bmsError = frame->data[3];
    break;
  }
}

void logBatteryState() {
  Serial.printf("Load: %.1f amps\t", Battery.current / 10.0);
  Serial.printf("RC: %.1f/%.1f\t", Battery.ratedChargeCurrent / 10.0,
                Battery.ratedDischargeCurrent / 10.0);
  Serial.printf("SOC: %d%% SOH: %d%% T: %d\t", Battery.charge, Battery.health,
                Battery.temperature);
  Serial.printf("V: %.2f of %.2f\n", Battery.voltage / 100.0,
                Battery.ratedVoltage / 10.0);
}
