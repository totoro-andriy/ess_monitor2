#include <Arduino.h>
#include <EEManager.h>
#include <GyverPortal.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <mcp_can.h>

#define HOSTNAME "ess_monitor2"
#define DEBUG 0
#define PROTECTED 0

void build() ;
void web_action();
void countDownHour();
void mcp_read();
byte genChargeFrame ();
void mcp_send();
void ProcessData(uint32_t cid, int d0, int d1, int d2, int d3, int d4, int d5,   int d6, int d7);
void PrintData();
void draw();
void checkmac();
void mcp_can_init();
void setWifiSta();
void setWifiAp();
void initWifi();
void initEEPROM();

String status_msg;
String voltage;
String temp_str;
String health_str;
String charge_str;
String current_str;
String limmits_str;
String esponline_str;

// Clone protection
char licmac[] = "00:00:00:00:00:00";

// Init EEPROM
struct Data {
  byte initDone;
  String client_ssid;
  String client_pswd;
  int target_chrg;
  bool wifi_mode;
  int target_dischrg;
  int full_charge_time;
  byte state_of_charge;
};
Data data;
EEManager memory(data);

// init Oled Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Declare Vars
int batt_charge, batt_health, rated_voltage, rated_current1, rated_current2,
    batt_voltage, batt_temp, bms1, bms2;
int16_t batt_current;
double drated_voltage, dbatt_voltage, dbatt_temp, dbatt_current;
int drated_current1, drated_current2;
unsigned long online, previousMillis;
const long interval = 1000;

unsigned long previousMillis2;
const long interval2 = 60000;

bool needsReset, needsInit;

bool charge_allowed, discharge_allowed, gofull;
int count_seconds = 36;
// Web Interface
#define GP_NO_OTA
#define GP_NO_UPLOAD
#define GP_NO_DOWNLOAD
GyverPortal portal;

void build() {
  GP.ONLINE_CHECK(10000);
  GP.setTimeout(3000);
  GP.BUILD_BEGIN(GP_DARK);
  GP.UPDATE("charge_allowed_led,discharge_allowed_led,gofull_led,batt_voltage_"
            "state,batt_soc_state,batt_soh_state,batt_temp_state,batt_curr_"
            "state,batt_limmits_state,esponline_time");
  GP.UPDATE_CLICK("rst_info", "rst");
  GP.ALERT("rst_info");
  GP.NAV_TABS("Home,Inverter,Config");
  // Home Tab
  GP.NAV_BLOCK_BEGIN();
  GP.PAGE_TITLE(HOSTNAME);
  M_BLOCK(GP_DIV, GP.LABEL("Статус Системи"););
  M_BLOCK(GP_TAB, "", "Напруга", GP.TITLE(voltage, "batt_voltage_state"););
  M_BLOCK(GP_THIN, "", "Заряд", GP.TITLE(charge_str, "batt_soc_state"););
  M_BLOCK(GP_THIN, "", "Здоров'я", GP.TITLE(health_str, "batt_soh_state"););
  M_BLOCK(GP_THIN, "", "Температура", GP.TITLE(temp_str, "batt_temp_state"););
  M_BLOCK(GP_TAB, "", "Поточне Навантаження",
          GP.TITLE(current_str, "batt_curr_state"););
  M_BLOCK(GP_TAB, "", "Ліміти батареї",
          GP.TITLE(limmits_str, "batt_limmits_state"););
  GP.BREAK();
  GP.HR();
  M_BLOCK(GP_THIN, "", "Час Онлайн Контролера",
          GP.LABEL(esponline_str, "esponline_time"););
  GP.NAV_BLOCK_END();
  GP.NAV_BLOCK_BEGIN();
  // Inverter Tab
  M_BLOCK(GP_DIV, GP.LABEL("Дозволи для інвертера"););

  GP.GRID_BEGIN();

  GP.BLOCK_BEGIN(GP_DIV_RAW);
  GP.LED("charge_allowed_led", charge_allowed, GP_GREEN);
  GP.BREAK();
  GP.LABEL("charge");
  GP.BLOCK_END();

  GP.BLOCK_BEGIN(GP_DIV_RAW);
  GP.LED("discharge_allowed_led", discharge_allowed, GP_GREEN);
  GP.BREAK();
  GP.LABEL("discharge");
  GP.BLOCK_END();

  GP.BLOCK_BEGIN(GP_DIV_RAW);
  GP.LED("gofull_led", gofull, GP_BLUE);
  GP.BREAK();
  GP.LABEL("balance");
  GP.BLOCK_END();

  GP.GRID_END();
  GP.BREAK();

  GP.BREAK();
  GP.BREAK();
  GP.LABEL("Максимальний % заряду");
  GP.SLIDER("target_charge", data.target_chrg, 0, 100);
  GP.SPAN("Після досягнення заданого проценту заряду або вище. Запит на заряд "
          "до інвертера передаватися перестане");
  GP.BREAK();
  GP.BREAK();
  GP.LABEL("Дозволений % розряду");
  GP.SLIDER("target_discharge", data.target_dischrg, 0, 100);
  GP.SPAN("Після досягнення заданого проценту заряду або нижче. Дозвіл на "
          "подальший рорзряд батареї передаватися перестане");
  GP.BREAK();
  GP.BREAK();
  GP.HR();
  GP.PLAIN("Ці параметри можуть інгноруватися вашим пристроєм, все залнежить "
           "від моделі, прошивки та виробника",
           "disclaimer", GP_YELLOW);
  GP.HR();
  GP.BREAK();
  GP.LABEL("Запит повного заряду батареї");
  GP.SPINNER("full_charge_time", data.full_charge_time, 0, 72);
  GP.SPAN("годин");
  GP.BREAK();
  GP.HR();
  GP.PLAIN("Повний заряд рекомендується проводити тіьки від мережі 220V", "",
           GP_YELLOW);
  GP.PLAIN("Цей режим потрібно виключно для балансування батарей.", "",
           GP_YELLOW);
  GP.HR();

  GP.NAV_BLOCK_END();
  // Config Tab
  GP.NAV_BLOCK_BEGIN();

  M_BLOCK(GP_DIV, GP.LABEL("Налаштування Wifi"););
  GP.FORM_BEGIN("/wifi");
  GP.SWITCH("wifi_mode", data.wifi_mode);
  GP.LABEL("Під'єднуватись до wifi");
  GP.BREAK();
  GP.TEXT("form_ssid", "SSID", data.client_ssid);
  GP.TEXT("form_pswd", "Пароль", data.client_pswd);
  GP.BREAK();
  GP.SUBMIT("Застосувати");
  GP.FORM_END();
  GP.BREAK();
  GP.HR();
  M_BLOCK(GP_DIV, GP.LABEL("Скинути Налаштування"););
  GP.BUTTON("rst", "Factory Reset", "", GP_RED);

  GP.NAV_BLOCK_END();

  GP.BUILD_END();
}

void web_action() {
  if (portal.form()) {
    Serial.print("Submit form: ");
    if (portal.form("/wifi")) {
      portal.copyString("form_ssid", data.client_ssid);
      portal.copyString("form_pswd", data.client_pswd);
      portal.copyBool("wifi_mode", data.wifi_mode);
      if (data.wifi_mode) {
        Serial.print("Station Mode ");
      } else {
        Serial.print("Access Point Mode ");
      }
      Serial.print("ssid: ");
      Serial.print(data.client_ssid);
      Serial.print(" password: ");
      Serial.println(data.client_pswd);
      memory.update();
      needsReset = true;
    }
  }
  if (portal.update()) {
    if (DEBUG) {
      Serial.print("Got Update Signal ");
      Serial.println(portal.updateName());
    }
    if (portal.update("rst_info")) {
      portal.answer("Налаштування було скинуто!");
      needsInit = true;
    }
    if (portal.update("charge_allowed_led"))
      portal.answer(charge_allowed);
    if (portal.update("discharge_allowed_led"))
      portal.answer(discharge_allowed);
    if (portal.update("gofull_led"))
      portal.answer(gofull);
    if (portal.update("batt_voltage_state"))
      portal.answer(voltage);
    if (portal.update("batt_soc_state"))
      portal.answer(charge_str);
    if (portal.update("batt_soh_state"))
      portal.answer(health_str);
    if (portal.update("batt_temp_state"))
      portal.answer(temp_str);
    if (portal.update("batt_curr_state"))
      portal.answer(current_str);
    if (portal.update("batt_limmits_state"))
      portal.answer(limmits_str);
    if (portal.update("esponline_time"))
      portal.answer(esponline_str);
  }
  if (portal.click()) {
    Serial.print("Got Click Signal ");
    Serial.println(portal.clickName());
    if (portal.click("target_charge")) {
      data.target_chrg = portal.getInt();
      memory.update();
    }
    if (portal.click("target_discharge")) {
      data.target_dischrg = portal.getInt();
      memory.update();
    }
    if (portal.click("full_charge_time")) {
      data.full_charge_time = portal.getInt();
    }
  }
}

// Init CAN

MCP_CAN CAN0(5);
#define CAN0_INT 15
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  if (PROTECTED) {
    checkmac();
  }
  mcp_can_init();
  initEEPROM();
  u8g2.clear();
  initWifi();

  mcp_read();

  portal.attachBuild(build);
  portal.attach(web_action);
  portal.start();
}

void countDownHour() {
  if (data.full_charge_time) {
    if (count_seconds) {
      count_seconds--;
    } else {
      count_seconds = 36;
      data.full_charge_time--;
      Serial.println("One hour has passed since the request for a Full Charge");
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    mcp_send();
    PrintData();
    draw();
    countDownHour();

    if (data.full_charge_time) {
      gofull = true;
    } else {
      gofull = false;
    };

    health_str = String(batt_health) + "%";

    charge_str = String(batt_charge) + "%";

    drated_voltage = rated_voltage;
    drated_voltage = drated_voltage / 10;
    dbatt_voltage = batt_voltage;
    dbatt_voltage = dbatt_voltage / 100;
    voltage = String(dbatt_voltage) + " - " + String(drated_voltage);

    dbatt_temp = batt_temp;
    dbatt_temp = dbatt_temp / 10;
    temp_str = String(dbatt_temp) + "°C";

    dbatt_current = batt_current;
    dbatt_current = dbatt_current / 10;
    current_str = String(dbatt_current) + "A";

    drated_current1 = rated_current1;
    drated_current1 = drated_current1 / 10;
    drated_current2 = rated_current2;
    drated_current2 = drated_current2 / 10;
    limmits_str =
        String(drated_current1) + "A / " + String(drated_current2) + "A";

    online = esp_timer_get_time() / 1000000;
    esponline_str = String(online) + " секунд";
  }

  mcp_read();
  portal.tick();
  if (needsInit) {
    Serial.println("Got EEPROM Erase Signal");
    needsInit = false;
    memory.reset();
    ESP.restart();
  } // Reset
  if (memory.tick()) {
    if (needsReset) {
      ESP.restart();
    }
  } // Save & Restart
}

void mcp_read() {
  if (!digitalRead(CAN0_INT)) // If CAN0_INT pin is low, read receive buffer
  {
    CAN0.readMsgBuf(&rxId, &len,
                    rxBuf); // Read data: len = data length, buf = data byte(s)
    ProcessData(rxId, rxBuf[0], rxBuf[1], rxBuf[2], rxBuf[3], rxBuf[4],
                rxBuf[5], rxBuf[6], rxBuf[7]);
  }
}

byte f35e[8] = {'P', 'Y', 'L', 'O', 'N', ' ', ' ', ' '}; // deya protocol id
byte f370[8] = {'P', 'Y', 'L', 'O',
                'N', 'D', 'I', 'Y'}; // sunny island optional id
byte f305[8] = {0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // keep alive
byte f35a[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // no idea
byte f35c[8]; // Charge Restrictions for Deye Inverter

byte f379[8] = {0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // One Battery

byte genChargeFrame() {
  if (rated_current1 != 0) {               // battery allows charging
    if (batt_charge <= data.target_chrg) { // charge policy ok
      charge_allowed = 1;
    } else {
      charge_allowed = 0;
    };
  }
  if (rated_current2 != 0) {                  // battery allows discharging
    if (batt_charge >= data.target_dischrg) { // discharge policy ok
      discharge_allowed = 1;
    } else {
      discharge_allowed = 0;
    };
  }
  byte res = 0;
  if (data.full_charge_time) {
    res |= (1 << 3);
  }
  if (discharge_allowed) {
    res |= (1 << 6);
  }
  if (charge_allowed) {
    res |= (1 << 7);
  }
  return res;
}

void mcp_send() {
  CAN0.sendMsgBuf(0x35e, 0, 8, f35e);
  // CAN0.sendMsgBuf(0x370, 0, 8, f370);
  CAN0.sendMsgBuf(0x305, 0, 1, f305);
  // CAN0.sendMsgBuf(0x35a, 0, 8, f35a);
  f35c[1] = genChargeFrame();
  if (DEBUG) {
    Serial.print("Sending Frame 35C: ");
    Serial.print(f35c[1], BIN);
    Serial.print(" HEX: ");
    Serial.print(f35c[1], HEX);
    Serial.print(" ");
  }
  CAN0.sendMsgBuf(0x35c, 0, 1, f35c);
  // CAN0.sendMsgBuf(0x379, 0, 1, f379);
}

void ProcessData(uint32_t cid, int d0, int d1, int d2, int d3, int d4, int d5,
                 int d6, int d7) {
  // Try to parse Data
  switch (cid) {
  case 849: // 0x351 Battery Limmits
    rated_voltage = (d1 << 8) | d0;
    rated_current1 = (d3 << 8) | d2;
    rated_current2 = (d5 << 8) | d4;
    break;
  case 853: // 0x355 Battery Health
    batt_charge = (d1 << 8) | d0;
    batt_health = (d3 << 8) | d2;
    break;
  case 854: // 0x356 System Voltage,Current,Health
    batt_voltage = (d1 << 8) | d0;
    batt_current = (uint16_t)d3 << 8 | d2;
    batt_temp = (uint16_t)d5 << 8 | d4;
    break;
  case 857: // 0x359 BMS Error
    bms1 = d1;
    bms2 = d3;
    break;

  default:
    if (DEBUG) {
      Serial.print(F("Unknown Frame with Adress:"));
      Serial.print(cid, HEX);
      Serial.print(" ");
      Serial.print("\t");
      Serial.print(d0, HEX);
      Serial.print(" ");
      Serial.print(d1, HEX);
      Serial.print(" ");
      Serial.print(d2, HEX);
      Serial.print(" ");
      Serial.print(d3, HEX);
      Serial.print(" ");
      Serial.print(d4, HEX);
      Serial.print(" ");
      Serial.print(d5, HEX);
      Serial.print(" ");
      Serial.print(d6, HEX);
      Serial.print(" ");
      Serial.print(d7, HEX);
      Serial.print(" ");
      Serial.println();
    }
    break;
  }
}

void PrintData() {
  Serial.print("Load:");
  Serial.print(dbatt_current);
  Serial.print("amps\t");
  Serial.print("RC:");
  Serial.print(drated_current1);
  Serial.print("/");
  Serial.print(drated_current2);
  Serial.print("\t");
  Serial.print("SOC:");
  Serial.print(batt_charge);
  Serial.print("%");
  Serial.print(" ");
  Serial.print("SOH:");
  Serial.print(batt_health);
  Serial.print("%");
  Serial.print(" ");
  Serial.print("T:");
  Serial.print(dbatt_temp);
  Serial.print("\t");
  Serial.print("V:");
  Serial.print(dbatt_voltage);
  Serial.print(" of ");
  Serial.print(drated_voltage);
  Serial.println();
}

void draw() {
  // graphic commands to redraw the complete screen should be placed here
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_pressstart2p_8r);

  u8g2.drawStr(0, 10, "V");
  u8g2.setCursor(12, 10);

  u8g2.print(dbatt_voltage);
  u8g2.setCursor(70, 10);

  u8g2.print(drated_voltage);

  u8g2.drawLine(0, 11, 128, 11);

  u8g2.drawStr(0, 24, "C:");
  u8g2.setCursor(16, 24);
  u8g2.print(batt_charge);
  u8g2.drawStr(36, 24, "%");

  u8g2.drawStr(70, 24, "H:");
  u8g2.setCursor(86, 24);
  u8g2.print(batt_health);
  u8g2.drawStr(110, 24, "%");

  u8g2.drawStr(12, 36, "Temp:");
  u8g2.setCursor(52, 36);
  u8g2.print(dbatt_temp);
  u8g2.drawStr(102, 36, "C");

  if (bms1 || bms2) { // Show BMS Errors if any
    u8g2.drawStr(0, 50, "ERROR:");
    u8g2.setCursor(50, 50);
    u8g2.print(bms1);
    u8g2.setCursor(90, 50);
    u8g2.print(bms2);
  } else { // Show Status
    if (data.wifi_mode) {
      u8g2.setCursor(0, 50);
      u8g2.print(WiFi.localIP());
    } else {
      u8g2.drawStr(0, 50, "ap:");
      u8g2.drawStr(30, 50, HOSTNAME);
    }
  }

  u8g2.drawStr(0, 64, "A");
  u8g2.setCursor(12, 64);
  u8g2.print(dbatt_current);
  u8g2.setCursor(70, 64);
  u8g2.print(drated_current1);
  u8g2.setCursor(100, 64);

  u8g2.print(drated_current2);
  u8g2.drawLine(0, 52, 128, 52);

  u8g2.sendBuffer();
}

void checkmac() {
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  String licmacstr(licmac);
  String curmacstr(WiFi.macAddress());
  if (curmacstr != licmacstr) {
    while (1) {
      Serial.println("ESP Warmup!");
      delay(1000);
    };
  }
}

void mcp_can_init() {
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
  } else
    Serial.println("Error Initializing MCP2515...");
  CAN0.setMode(MCP_NORMAL);
  attachInterrupt(CAN0_INT, mcp_read, LOW);
  pinMode(CAN0_INT, INPUT);
}

void setWifiSta() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(data.client_ssid, data.client_pswd);
  int wifi_connect_wait_time;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (wifi_connect_wait_time < 20) {
      wifi_connect_wait_time++;
    } else {
      break;
    }
  }
}
void setWifiAp() {
  IPAddress AP_IP(192, 168, 4, 1);
  IPAddress AP_PFX(255, 255, 255, 0);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_PFX);
  WiFi.softAP(HOSTNAME, "12345678");
}

void initWifi() {
  if (data.wifi_mode) {
    setWifiSta();
    if (WiFi.status() != WL_CONNECTED) {
      setWifiAp();
    }
  } else {
    setWifiAp();
  }
  Serial.print("Local IP is: ");
  Serial.println(WiFi.localIP());
}

void initEEPROM() {
  EEPROM.begin(memory.blockSize());
  byte eepromStat = memory.begin(0, 'b');

  Serial.print("EEPROM: ");
  switch (eepromStat) {
  case 0:
    Serial.println("OK");
    break;
  case 1:
    Serial.println("Init Done");
    data.target_chrg = 30;
    data.target_dischrg = 5;
    data.full_charge_time = 0;
    break;
  case 2:
    Serial.println("Error No Space");
    break;
  }
}
