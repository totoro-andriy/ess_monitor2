#include "battery.h"
#include <gtest/gtest.h>

TEST(GetChargeControlByte, ChargeBit) {
  const uint8_t mask = 1 << 7;
  BatteryState b;

  b = {.ratedChargeCurrent = 0, .charge = 0};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 0, .charge = 98};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 0, .charge = 99};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 0, .charge = 100};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 10, .charge = 0};
  EXPECT_TRUE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 10, .charge = 98};
  EXPECT_TRUE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 10, .charge = 99};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedChargeCurrent = 10, .charge = 100};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);
}

TEST(GetChargeControlByte, DischargeBit) {
  const uint8_t mask = 1 << 6;
  BatteryState b;

  b = {.ratedDischargeCurrent = 0, .charge = 0};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 0, .charge = 29};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 0, .charge = 30};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 0, .charge = 100};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 10, .charge = 0};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 10, .charge = 29};
  EXPECT_FALSE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 10, .charge = 30};
  EXPECT_TRUE(getChargeControlByte(&b) & mask);

  b = {.ratedDischargeCurrent = 10, .charge = 100};
  EXPECT_TRUE(getChargeControlByte(&b) & mask);
}

TEST(GetChargeControlByte, FullChargeBit) {
  const uint8_t mask = 1 << 3;
  BatteryState b = {};

  // TODO: update the test after this is properly implemented
  EXPECT_FALSE(getChargeControlByte(&b) & mask);
}

TEST(BytesToInt16, All) {
  EXPECT_EQ(bytesToInt16(0b00, 0b00), 0);
  EXPECT_EQ(bytesToInt16(0b00, 0b01), 256);
  EXPECT_EQ(bytesToInt16(0b01, 0b00), 1);
  EXPECT_EQ(bytesToInt16(0b00, 0b10000000), -32768);
  EXPECT_EQ(bytesToInt16(0b10000000, 0b00), 128);
}

TEST(ProcessDataFrame, Frame0x351) {
  DataFrame f = {
      .id = 0x351, .dlc = 8, .data = {0x46, 0x02, 0x54, 0x01, 0xEC, 0x04}};
  BatteryState s = {};
  processDataFrame(&f, &s);
  EXPECT_EQ(s.ratedVoltage, 582);
  EXPECT_EQ(s.ratedChargeCurrent, 340);
  EXPECT_EQ(s.ratedDischargeCurrent, 1260);
}

TEST(ProcessDataFrame, Frame0x355) {
  DataFrame f = {.id = 0x355, .dlc = 8, .data = {0x62, 0x00, 0x5E, 0x00}};
  BatteryState s = {};
  processDataFrame(&f, &s);
  EXPECT_EQ(s.charge, 98);
  EXPECT_EQ(s.health, 94);
}

TEST(ProcessDataFrame, Frame0x356) {
  DataFrame f = {
      .id = 0x356, .dlc = 8, .data = {0x8A, 0x16, 0x05, 0x00, 0xFD, 0x00}};
  BatteryState s = {};
  processDataFrame(&f, &s);
  EXPECT_EQ(s.voltage, 5770);
  EXPECT_EQ(s.current, 5);
  EXPECT_EQ(s.temperature, 253);
}

TEST(ProcessDataFrame, Frame0x359) {
  DataFrame f = {.id = 0x359, .dlc = 8, .data = {0x00, 0x01, 0x00, 0x02}};
  BatteryState s = {};
  processDataFrame(&f, &s);
  EXPECT_EQ(s.bmsWarning, 1);
  EXPECT_EQ(s.bmsError, 2);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  if (RUN_ALL_TESTS())
    ;

  return 0;
}
