#include "hass.h"

namespace CAN {
  extern volatile uint32_t timestamp_frame351;
  extern volatile uint32_t timestamp_frame355;
  extern volatile uint32_t timestamp_frame356;
  extern volatile uint32_t timestamp_frame359;
}

extern Config Cfg;
extern volatile EssStatus Ess;

namespace HASS {

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);
HASensorNumber chargeSensor("charge");
HASensorNumber healthSensor("health");
HASensorNumber voltageSensor("voltage", HASensorNumber::PrecisionP2);
HASensorNumber ratedVoltageSensor("rated_voltage", HASensorNumber::PrecisionP2);
HASensorNumber currentSensor("current", HASensorNumber::PrecisionP1);
HASensorNumber ratedChargeCurrentSensor("rated_charge_current",
                                        HASensorNumber::PrecisionP1);
HASensorNumber ratedDischargeCurrentSensor("rated_discharge_current",
                                           HASensorNumber::PrecisionP1);
HASensorNumber temperatureSensor("temperature", HASensorNumber::PrecisionP1);
HASensor bmsWarningSensor("bms_warning");
HASensor bmsErrorSensor("bms_error");

HASensorNumber uptimeSensor("uptime", HASensorNumber::PrecisionP0);

static const uint16_t EXPIRE_AFTER = BATTERY_TIMEOUT_SEC;

void begin(uint8_t core, uint8_t priority);
void task(void *pvParameters);
void loop();

void begin(uint8_t core, uint8_t priority) {
  xTaskCreatePinnedToCore(task, "hass_task", 20000, NULL, priority, NULL, core);
}

void task(void *pvParameters) {
  Serial.printf("[HASS] Task running in core %d.\n",
                (uint32_t)xPortGetCoreID());

  uint8_t mac[6];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));
  device.enableExtendedUniqueIds();
  device.setName("ESS Monitor");
  device.setModel("ess-monitor");
  device.setManufacturer("Bimba Perdoling.");
  device.setSoftwareVersion(VERSION);
  char url[24];
  String("http://" + WiFi.localIP().toString()).toCharArray(url, sizeof(url));
  device.setConfigurationUrl(url);
  device.enableSharedAvailability();
  device.enableLastWill();

  chargeSensor.setIcon("mdi:battery-high");
  chargeSensor.setName("Charge");
  chargeSensor.setDeviceClass("battery");
  chargeSensor.setUnitOfMeasurement("%");
  chargeSensor.setExpireAfter(EXPIRE_AFTER);

  healthSensor.setIcon("mdi:heart");
  healthSensor.setName("Health");
  healthSensor.setUnitOfMeasurement("%");
  healthSensor.setExpireAfter(EXPIRE_AFTER);

  voltageSensor.setIcon("mdi:flash-triangle-outline");
  voltageSensor.setName("Voltage");
  voltageSensor.setDeviceClass("voltage");
  voltageSensor.setUnitOfMeasurement("V");
  voltageSensor.setExpireAfter(EXPIRE_AFTER);

  ratedVoltageSensor.setIcon("mdi:flash-triangle");
  ratedVoltageSensor.setName("Rated voltage");
  ratedVoltageSensor.setDeviceClass("voltage");
  ratedVoltageSensor.setUnitOfMeasurement("V");
  ratedVoltageSensor.setExpireAfter(EXPIRE_AFTER);

  currentSensor.setIcon("mdi:current-dc");
  currentSensor.setName("Current");
  currentSensor.setDeviceClass("current");
  currentSensor.setUnitOfMeasurement("A");
  currentSensor.setExpireAfter(EXPIRE_AFTER);

  ratedChargeCurrentSensor.setIcon("mdi:current-dc");
  ratedChargeCurrentSensor.setName("Rated charge current");
  ratedChargeCurrentSensor.setDeviceClass("current");
  ratedChargeCurrentSensor.setUnitOfMeasurement("A");
  ratedChargeCurrentSensor.setExpireAfter(EXPIRE_AFTER);

  ratedDischargeCurrentSensor.setIcon("mdi:current-dc");
  ratedDischargeCurrentSensor.setName("Rated discharge current");
  ratedDischargeCurrentSensor.setDeviceClass("current");
  ratedDischargeCurrentSensor.setUnitOfMeasurement("A");
  ratedDischargeCurrentSensor.setExpireAfter(EXPIRE_AFTER);

  temperatureSensor.setIcon("mdi:thermometer");
  temperatureSensor.setName("Temperature");
  temperatureSensor.setDeviceClass("temperature");
  temperatureSensor.setUnitOfMeasurement("°C");
  temperatureSensor.setExpireAfter(EXPIRE_AFTER);

  bmsWarningSensor.setIcon("mdi:alert");
  bmsWarningSensor.setDeviceClass("enum");
  bmsWarningSensor.setName("BMS warning");
  bmsWarningSensor.setExpireAfter(EXPIRE_AFTER);

  bmsErrorSensor.setIcon("mdi:alert-octagon");
  bmsErrorSensor.setDeviceClass("enum");
  bmsErrorSensor.setName("BMS error");
  bmsErrorSensor.setExpireAfter(EXPIRE_AFTER);

  uptimeSensor.setIcon("mdi:timer-outline");
  uptimeSensor.setName("Controller uptime");
  uptimeSensor.setUnitOfMeasurement("s");
  uptimeSensor.setExpireAfter(EXPIRE_AFTER);

  IPAddress ip;
  ip.fromString(Cfg.mqttBrokerIp);
  mqtt.begin(ip);

  vTaskDelay(1000 * 30 / portTICK_PERIOD_MS);

  while (1) {
    loop();
    vTaskDelay(5);
  }

  Serial.println("[HASS] Task exited.");
  vTaskDelete(NULL);
};

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // буфер останніх опублікованих таймстампів по кожному типу кадру
  static uint32_t lastPublished351 = 0;
  static uint32_t lastPublished355 = 0;
  static uint32_t lastPublished356 = 0;
  static uint32_t lastPublished359 = 0;

  // Every 5 seconds
  if (currentMillis - previousMillis >= 1000 * 5) {
    previousMillis = currentMillis;

#ifdef DEBUG
    Serial.println("[HASS] Publishing sensor updates.");
#endif

    char buf[4];

    // 0x351 — Battery Limits (ratedVoltage, ratedChargeCurrent, ratedDischargeCurrent)
    if (CAN::timestamp_frame351 != 0 &&
        CAN::timestamp_frame351 != lastPublished351) {

#ifdef DEBUG
      Serial.println("[HASS] New 0x351 frame — updating rated voltage/current sensors.");
#endif

      ratedVoltageSensor.setValue(Ess.ratedVoltage, true);
      ratedChargeCurrentSensor.setValue(Ess.ratedChargeCurrent, true);
      ratedDischargeCurrentSensor.setValue(Ess.ratedDischargeCurrent, true);

      lastPublished351 = CAN::timestamp_frame351;
    }

    // 0x355 — Battery Health (charge, health)
    if (CAN::timestamp_frame355 != 0 &&
        CAN::timestamp_frame355 != lastPublished355) {

#ifdef DEBUG
      Serial.println("[HASS] New 0x355 frame — updating charge/health sensors.");
#endif

      chargeSensor.setValue(Ess.charge, true);
      healthSensor.setValue(Ess.health, true);

      lastPublished355 = CAN::timestamp_frame355;
    }

    // 0x356 — Voltage, Current, Temp
    if (CAN::timestamp_frame356 != 0 &&
        CAN::timestamp_frame356 != lastPublished356) {

#ifdef DEBUG
      Serial.println("[HASS] New 0x356 frame — updating voltage/current/temperature sensors.");
#endif

      voltageSensor.setValue(Ess.voltage, true);
      currentSensor.setValue(Ess.current, true);
      temperatureSensor.setValue(Ess.temperature, true);

      lastPublished356 = CAN::timestamp_frame356;
    }

    // 0x359 — BMS Error/Warning
    if (CAN::timestamp_frame359 != 0 &&
        CAN::timestamp_frame359 != lastPublished359) {

#ifdef DEBUG
      Serial.println("[HASS] New 0x359 frame — updating BMS warning/error sensors.");
#endif

      sprintf(buf, "%d", Ess.bmsWarning);
      bmsWarningSensor.setValue(buf);
      sprintf(buf, "%d", Ess.bmsError);
      bmsErrorSensor.setValue(buf);

      lastPublished359 = CAN::timestamp_frame359;
    }

    // esp32 uptime
    uint64_t uptimeSec64 = getSystemUptimeSeconds();
    uptimeSensor.setValue((uint32_t)uptimeSec64, true);

  }

  mqtt.loop();
}

} // namespace HASS
