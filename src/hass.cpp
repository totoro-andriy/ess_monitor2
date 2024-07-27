#include "hass.h"

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

  healthSensor.setIcon("mdi:heart");
  healthSensor.setName("Health");
  healthSensor.setUnitOfMeasurement("%");

  voltageSensor.setIcon("mdi:flash-triangle-outline");
  voltageSensor.setName("Voltage");
  voltageSensor.setDeviceClass("voltage");
  voltageSensor.setUnitOfMeasurement("V");

  ratedVoltageSensor.setIcon("mdi:flash-triangle");
  ratedVoltageSensor.setName("Rated voltage");
  ratedVoltageSensor.setDeviceClass("voltage");
  ratedVoltageSensor.setUnitOfMeasurement("V");

  currentSensor.setIcon("mdi:current-dc");
  currentSensor.setName("Current");
  currentSensor.setDeviceClass("current");
  currentSensor.setUnitOfMeasurement("A");

  ratedChargeCurrentSensor.setIcon("mdi:current-dc");
  ratedChargeCurrentSensor.setName("Rated charge current");
  ratedChargeCurrentSensor.setDeviceClass("current");
  ratedChargeCurrentSensor.setUnitOfMeasurement("A");

  ratedDischargeCurrentSensor.setIcon("mdi:current-dc");
  ratedDischargeCurrentSensor.setName("Rated discharge current");
  ratedDischargeCurrentSensor.setDeviceClass("current");
  ratedDischargeCurrentSensor.setUnitOfMeasurement("A");

  temperatureSensor.setIcon("mdi:thermometer");
  temperatureSensor.setName("Temperature");
  temperatureSensor.setDeviceClass("temperature");
  temperatureSensor.setUnitOfMeasurement("°C");

  bmsWarningSensor.setIcon("mdi:alert");
  bmsWarningSensor.setDeviceClass("enum");
  bmsWarningSensor.setName("BMS warning");

  bmsErrorSensor.setIcon("mdi:alert-octagon");
  bmsErrorSensor.setDeviceClass("enum");
  bmsErrorSensor.setName("BMS error");

  IPAddress ip;
  ip.fromString(Cfg.mqttBrokerIp);
  mqtt.begin(ip);

  vTaskDelay(1000 * 30 / portTICK_PERIOD_MS);

  while (1) {
    loop();
  }

  Serial.println("[HASS] Task exited.");
  vTaskDelete(NULL);
};

void loop() {
  static uint32_t previousMillis;
  uint32_t currentMillis = millis();

  // Every 5 seconds
  if (currentMillis - previousMillis >= 1000 * 5) {
    previousMillis = currentMillis;

#ifdef DEBUG
    Serial.println("[HASS] Publishing sensor updates.");
#endif

    char buf[4];

    chargeSensor.setValue(Ess.charge);
    healthSensor.setValue(Ess.health);
    voltageSensor.setValue(Ess.voltage);
    ratedVoltageSensor.setValue(Ess.ratedVoltage);
    currentSensor.setValue(Ess.current);
    ratedChargeCurrentSensor.setValue(Ess.ratedChargeCurrent);
    ratedDischargeCurrentSensor.setValue(Ess.ratedDischargeCurrent);
    temperatureSensor.setValue(Ess.temperature);
    sprintf(buf, "%d", Ess.bmsWarning);
    bmsWarningSensor.setValue(buf);
    sprintf(buf, "%d", Ess.bmsError);
    bmsErrorSensor.setValue(buf);
  }

  mqtt.loop();
}

} // namespace HASS
