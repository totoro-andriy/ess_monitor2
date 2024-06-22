#ifndef ESS_H
#define ESS_H

#include <HardwareSerial.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mcp_can.h>
#include <stdint.h>

#define CAN0_INT 15

namespace Ess {

typedef struct BatteryState {
  int16_t charge;
  int16_t health;
  int16_t voltage;
  int16_t current;
  int16_t ratedVoltage;
  int16_t ratedChargeCurrent;
  int16_t ratedDischargeCurrent;
  int16_t temperature;
  uint8_t bmsWarning;
  uint8_t bmsError;
} BatteryState;

typedef struct DataFrame {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
} DataFrame;

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

void begin();
BatteryState getState();

} // namespace Ess

#endif
