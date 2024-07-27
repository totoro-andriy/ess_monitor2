#ifndef _CAN_H
#define _CAN_H

#include <stdint.h>

#define CS_PIN 5
#define INT_PIN 15

namespace CAN {

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

void begin(uint8_t core, uint8_t priority);

} // namespace CAN

#endif
