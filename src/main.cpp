#include "battery.h"
#include <Arduino.h>
#include <SPI.h>
#include <mcp_can.h>

#define DEBUG 0
#define HOSTNAME "ess_monitor"
#define CAN0_INT 15

void initCAN();
void readCAN();
void writeCAN();
void logBatteryState();

BatteryState Battery;
MCP_CAN CAN0(5);

void setup() {
  Serial.begin(115200);

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

    if (DEBUG) {
      logBatteryState();
    }
  }
}

void initCAN() {
  Serial.print(F("Initializing MCP2515..."));

  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println(F(" OK."));
  } else {
    Serial.println(F(" ERROR!"));
  }
  CAN0.setMode(MCP_NORMAL);
  attachInterrupt(CAN0_INT, readCAN, LOW);
  pinMode(CAN0_INT, INPUT);
}

void readCAN() {
  if (digitalRead(CAN0_INT)) {
    // CAN0_INT high state means there is nothing to read
    return;
  }

  DataFrame frame = {};
  CAN0.readMsgBuf((unsigned long *)&frame.id, &frame.dlc, frame.data);
  processDataFrame(&frame, &Battery);
}

void writeCAN() {
  DataFrame chargeFrame = getChargeDataFrame(&Battery);

  CAN0.sendMsgBuf(DF_35E.id, DF_35E.dlc, (uint8_t *)DF_35E.data);
  CAN0.sendMsgBuf(DF_305.id, DF_305.dlc, (uint8_t *)DF_305.data);
  CAN0.sendMsgBuf(chargeFrame.id, chargeFrame.dlc, chargeFrame.data);
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
