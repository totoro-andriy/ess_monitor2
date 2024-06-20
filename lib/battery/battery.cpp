#include "battery.h"

int16_t bytesToInt16(uint8_t low, uint8_t high) { return (high << 8) | low; }

void processDataFrame(DataFrame *f, BatteryState *b) {
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
