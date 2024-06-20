#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

typedef struct BatteryState {
  int16_t charge;
  int16_t health;
  int16_t voltage;
  int16_t current;
  int16_t ratedVoltage;
  int16_t ratedChargeCurrent;
  int16_t ratedDischargeCurrent;
  int16_t temperature;
  uint8_t bmsWarning, bmsError;
} BatteryState;

typedef struct DataFrame {
  uint32_t id;
  uint8_t dlc;
  uint8_t data[8];
} DataFrame;

int16_t bytesToInt16(uint8_t low, uint8_t high);
void processDataFrame(DataFrame *f, BatteryState *b);
uint8_t getChargeControlByte(BatteryState *b);
DataFrame getChargeDataFrame(BatteryState *b);

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

#endif
