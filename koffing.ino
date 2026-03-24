// Koffing — Air Quality Monitor
// Arduino Nano ESP32 + PMSA003I + SGP40 + SCD4x + MiCS5524 + SSD1306 OLED

#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PM25AQI.h>
#include <Adafruit_SGP40.h>
#include "art/include/koffing_gfx.h"
#include "secrets.h"

#define SCREEN_W 128
#define SCREEN_H 64
#define LOOP_MS 5000
#define SCD4X_TIMEOUT_MS 300000  // 5 minutes — boots once, runs for days
#define MICS_PIN A0
#define MICS_WARMUP_MS 180000    // 3 minutes before readings are meaningful
#define VBUS_PIN A1              // USB 5V rail via voltage divider (2x 10k)
#define VBUS_LOW_THRESHOLD 4.75  // warn below this voltage

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Adafruit_PM25AQI pms;
Adafruit_SGP40 sgp;

#define SCD4X_ADDR ((uint8_t)0x62)

WiFiClient espClient;
PubSubClient mqtt(espClient);

bool pms_ok = false;
bool sgp_ok = false;
bool scd_ok = false;
bool mics_ok = false;
bool wifi_ok = false;
bool pms_got_data = false;
bool sgp_got_data = false;
bool scd_got_data = false;
bool mics_got_data = false;
unsigned long scd_start = 0;
unsigned long scd_last_read = 0;
unsigned long scd_last_retry = 0;
unsigned long wifi_lost_at = 0;

struct Readings {
  uint16_t pm25;
  int32_t voc;
  uint16_t co2;
  uint16_t gas;
  float temp;
  float humidity;
} data = {0, 0, 0, 0, 25.0, 50.0};

float vbus_voltage = 0.0;

// --- WiFi + MQTT ---

void wifi_connect() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("WiFi connecting...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi: connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print(".");
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);
  Serial.println(wifi_ok ? " OK" : " FAIL");
  if (wifi_ok) {
    wifi_lost_at = 0;
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifi_lost_at = millis();
  }
}

void mqtt_publish() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifi_ok) wifi_lost_at = millis();
    wifi_ok = false;
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    return;
  }
  if (!wifi_ok) wifi_lost_at = 0;
  wifi_ok = true;

  if (!mqtt.connected()) {
    if (!mqtt.connect("koffing")) return;
  }

  char buf[200];
  int n = snprintf(buf, sizeof(buf), "{\"uptime\":%lu", millis() / 1000);
  if (pms_got_data)
    n += snprintf(buf + n, sizeof(buf) - n, ",\"pm25\":%u", data.pm25);
  if (sgp_got_data)
    n += snprintf(buf + n, sizeof(buf) - n, ",\"voc\":%ld", (long)data.voc);
  if (scd_got_data) {
    n += snprintf(buf + n, sizeof(buf) - n, ",\"co2\":%u", data.co2);
    n += snprintf(buf + n, sizeof(buf) - n, ",\"temp_f\":%.1f", c_to_f(data.temp));
    n += snprintf(buf + n, sizeof(buf) - n, ",\"humidity\":%.1f", data.humidity);
  }
  if (mics_got_data)
    n += snprintf(buf + n, sizeof(buf) - n, ",\"gas\":%u", data.gas);
  n += snprintf(buf + n, sizeof(buf) - n, ",\"vbus\":%.2f", vbus_voltage);
  snprintf(buf + n, sizeof(buf) - n, "}");
  mqtt.publish("koffing/sensors", buf);
}

// --- SCD4x raw I2C (mirrors bb_scd41 approach) ---

void scd_cmd(uint16_t cmd) {
  Wire.beginTransmission(SCD4X_ADDR);
  Wire.write(cmd >> 8);
  Wire.write(cmd & 0xFF);
  Wire.endTransmission();
}

bool scd_init() {
  Wire.beginTransmission(SCD4X_ADDR);
  if (Wire.endTransmission() != 0) return false;

  scd_cmd(0x36f6);  // wakeup
  delay(20);
  scd_cmd(0x3646);  // reinit
  delay(30);
  scd_cmd(0x21b1);  // start_periodic_measurement
  delay(1);
  return true;
}

bool scd_data_ready() {
  scd_cmd(0xe4b8);  // get_data_ready_status
  delay(5);
  uint8_t buf[3];
  Wire.requestFrom(SCD4X_ADDR, (uint8_t)3);
  for (int i = 0; i < 3; i++) buf[i] = Wire.read();
  uint16_t status = ((uint16_t)buf[0] << 8) | buf[1];
  return (status & 0x07FF) != 0;
}

bool scd_read_measurement(uint16_t &co2, float &temp, float &humidity) {
  scd_cmd(0xec05);  // read_measurement
  delay(5);
  uint8_t buf[9];
  Wire.requestFrom(SCD4X_ADDR, (uint8_t)9);
  for (int i = 0; i < 9; i++) buf[i] = Wire.read();
  co2 = ((uint16_t)buf[0] << 8) | buf[1];
  uint16_t t_raw = ((uint16_t)buf[3] << 8) | buf[4];
  uint16_t h_raw = ((uint16_t)buf[6] << 8) | buf[7];
  temp = -45.0 + 175.0 * t_raw / 65536.0;
  humidity = 100.0 * h_raw / 65536.0;
  return co2 > 0;
}

// --- Sensor reads ---

void read_pms() {
  if (!pms_ok) return;
  PM25_AQI_Data d;
  if (pms.read(&d)) {
    data.pm25 = d.pm25_env;
    pms_got_data = true;
  }
}

void read_sgp() {
  if (!sgp_ok) return;
  int32_t voc = sgp.measureVocIndex(data.temp, data.humidity);
  if (voc > 0) {
    data.voc = voc;
    sgp_got_data = true;
  }
}

void read_scd() {
  if (!scd_ok) {
    if (millis() - scd_last_retry > SCD4X_TIMEOUT_MS) {
      scd_last_retry = millis();
      Serial.println("SCD4x: attempting re-init...");
      scd_ok = scd_init();
      if (scd_ok) {
        scd_start = millis();
        scd_got_data = false;
      }
    }
    return;
  }

  if (scd_data_ready()) {
    uint16_t co2;
    float t, h;
    if (scd_read_measurement(co2, t, h)) {
      data.co2 = co2;
      data.temp = t;
      data.humidity = h;
      scd_got_data = true;
      scd_last_read = millis();
    }
  }

  // Disable if it never starts or stops producing data
  unsigned long since_last = scd_got_data ? (millis() - scd_last_read) : (millis() - scd_start);
  if (since_last > SCD4X_TIMEOUT_MS) {
    scd_ok = false;
    scd_last_retry = millis();
    Serial.println("SCD4x: timed out, disabling");
  }
}

void read_mics() {
  if (!mics_ok) return;
  if (millis() < MICS_WARMUP_MS) return;
  data.gas = analogRead(MICS_PIN);
  mics_got_data = true;
}

void read_vbus() {
  vbus_voltage = (analogRead(VBUS_PIN) / 4095.0) * 3.3 * 2.0;
}

// Map PM2.5 µg/m³ to level 0-10 (suboptimal-first scale)
uint8_t pm25_to_level(uint16_t pm25) {
  if (pm25 <= 3)   return 0;
  if (pm25 <= 6)   return 1;
  if (pm25 <= 9)   return 2;
  if (pm25 <= 12)  return 3;
  if (pm25 <= 20)  return constrain(map(pm25, 13, 20, 4, 5), 4, 5);
  if (pm25 <= 35)  return constrain(map(pm25, 21, 35, 6, 7), 6, 7);
  if (pm25 <= 55)  return 8;
  if (pm25 <= 100) return 9;
  return 10;
}

// Map CO2 ppm to level 0-10 (outdoor ~420, stuffy room ~1000+)
uint8_t co2_to_level(uint16_t co2) {
  if (co2 <= 500)  return 0;
  if (co2 <= 600)  return 1;
  if (co2 <= 700)  return 2;
  if (co2 <= 800)  return 3;
  if (co2 <= 1000) return constrain(map(co2, 801, 1000, 4, 5), 4, 5);
  if (co2 <= 1500) return constrain(map(co2, 1001, 1500, 6, 7), 6, 7);
  if (co2 <= 2000) return 8;
  if (co2 <= 3000) return 9;
  return 10;
}

// Map SGP40 VOC index to level 0-10 (100 = baseline average)
uint8_t voc_to_level(int32_t voc) {
  if (voc <= 100)  return 0;
  if (voc <= 130)  return 1;
  if (voc <= 160)  return 2;
  if (voc <= 200)  return 3;
  if (voc <= 250)  return constrain(map(voc, 201, 250, 4, 5), 4, 5);
  if (voc <= 350)  return constrain(map(voc, 251, 350, 6, 7), 6, 7);
  if (voc <= 400)  return 8;
  if (voc <= 450)  return 9;
  return 10;
}

// Map MiCS5524 raw ADC to level 0-10 (lower = cleaner)
uint8_t gas_to_level(uint16_t gas) {
  if (gas <= 80)   return 0;
  if (gas <= 150)  return 1;
  if (gas <= 200)  return 2;
  if (gas <= 300)  return 3;
  if (gas <= 450)  return constrain(map(gas, 301, 450, 4, 5), 4, 5);
  if (gas <= 700)  return constrain(map(gas, 451, 700, 6, 7), 6, 7);
  if (gas <= 900)  return 8;
  if (gas <= 1200) return 9;
  return 10;
}

// Worst-of-all-sensors — any single bad reading drives the visual
uint8_t air_quality_level() {
  uint8_t level = 0;
  if (pms_got_data)  level = max(level, pm25_to_level(data.pm25));
  if (scd_got_data)  level = max(level, co2_to_level(data.co2));
  if (sgp_got_data)  level = max(level, voc_to_level(data.voc));
  if (mics_got_data) level = max(level, gas_to_level(data.gas));
  return level;
}

float c_to_f(float c) { return c * 9.0 / 5.0 + 32.0; }

// Print text, inverse (black-on-white) when `alert` is true
void print_val(const char* label, int32_t val, const char* unit, bool alert) {
  display.print(label);
  if (alert) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.print(val);
  display.print(unit);
  if (alert) display.setTextColor(SSD1306_WHITE);
}

// Draw an X at (x, y) — 5x5 pixels
void draw_x(int16_t x, int16_t y) {
  display.drawLine(x, y, x + 4, y + 4, SSD1306_WHITE);
  display.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
}

// Show WiFi error overlay in top-left when disconnected
void draw_wifi_status() {
  if (wifi_ok) return;
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(0, 0);
  unsigned long mins = (millis() - wifi_lost_at) / 60000;
  char buf[16];
  snprintf(buf, sizeof(buf), "WiFi ERR %lum", mins);
  display.print(buf);
  display.setTextColor(SSD1306_WHITE);
}

// --- Display ---

void draw_display() {
  display.clearDisplay();

  uint8_t level = air_quality_level();
  koffing_draw(display, level, 0, 0);

  int16_t x = 68;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // PM2.5 — label
  display.setCursor(x, 0);
  display.print("PM2.5 ug");
  if (!pms_ok) draw_x(123, 1);

  // PM2.5 — big value, inverse if >12 (above EPA "Good")
  display.setCursor(x, 10);
  display.setTextSize(2);
  if (!pms_ok) {
    display.print("---");
  } else if (pms_got_data) {
    if (data.pm25 > 12) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(data.pm25);
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print("---");
  }
  display.setTextSize(1);

  // VOC — inverse if >100 (above Sensirion baseline average)
  display.setCursor(x, 26);
  if (!sgp_ok) {
    display.print("VOC ---");
    draw_x(123, 27);
  } else if (!sgp_got_data) {
    display.print("VOC ---");
  } else {
    print_val("VOC ", data.voc, "", data.voc > 100);
  }

  // CO2 — inverse if >800 ppm (poor indoor air)
  display.setCursor(x, 36);
  if (!scd_ok) {
    display.print("CO2 ---");
    draw_x(123, 37);
  } else if (!scd_got_data) {
    display.print("CO2 ---");
  } else {
    print_val("CO2 ", data.co2, "p", data.co2 > 800);
  }

  // GAS — raw analog, inverse if elevated (>300 ADC)
  display.setCursor(x, 46);
  if (!mics_ok) {
    display.print("GAS ---");
    draw_x(123, 47);
  } else if (!mics_got_data) {
    display.print("GAS ---");
  } else {
    print_val("GAS ", data.gas, "", data.gas > 300);
  }

  // Temp (F) + Humidity
  display.setCursor(x, 56);
  if (scd_got_data) {
    display.print((int)c_to_f(data.temp));
    display.print("F ");
    display.print((int)data.humidity);
    display.print("%");
  } else {
    display.print("--F --%");
  }

  display.setTextSize(1);
  display.setCursor(0, 56);
  if (vbus_voltage < VBUS_LOW_THRESHOLD) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  char vbuf[10];
  snprintf(vbuf, sizeof(vbuf), "%4.2fV", vbus_voltage);
  display.print(vbuf);
  display.setTextColor(SSD1306_WHITE);

  draw_wifi_status();
  display.display();
}

// --- Serial logging ---

void serial_log() {
  Serial.print("[");
  Serial.print(pms_ok ? "P" : "p");
  Serial.print(sgp_ok ? "S" : "s");
  Serial.print(scd_ok ? "C" : "c");
  Serial.print(mics_ok ? "G" : "g");
  Serial.print("] ");
  Serial.print("PM2.5=");  Serial.print(pms_got_data ? (int)data.pm25 : -1);
  Serial.print(" VOC=");   Serial.print(sgp_got_data ? (int)data.voc : -1);
  Serial.print(" CO2=");   Serial.print(scd_got_data ? (int)data.co2 : -1);
  Serial.print(" GAS=");   Serial.print(mics_got_data ? (int)data.gas : -1);
  if (scd_got_data) {
    Serial.print(" T=");   Serial.print(c_to_f(data.temp), 1);
    Serial.print("F H=");  Serial.print(data.humidity, 1);
    Serial.print("%");
  }
  Serial.print(" VBUS=");  Serial.print(vbus_voltage, 2);  Serial.print("V");
  Serial.print(" LVL=");   Serial.println(air_quality_level());
}

// --- Main ---

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== Koffing ===");

  Wire.begin();
  Wire1.begin(D2, D3);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED FAIL — halting");
    while (1) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Koffing init...");
  display.display();

  scd_ok = scd_init();
  scd_start = millis();
  Serial.print("SCD4x: "); Serial.println(scd_ok ? "started" : "FAIL");

  pms_ok = pms.begin_I2C(&Wire1);
  Serial.print("PMS:   "); Serial.println(pms_ok ? "OK (Wire1)" : "FAIL");

  sgp_ok = sgp.begin();
  Serial.print("SGP40: "); Serial.println(sgp_ok ? "OK" : "FAIL");

  pinMode(MICS_PIN, INPUT);
  mics_ok = true;
  Serial.println("MiCS:  OK (warmup 3 min)");

  pinMode(VBUS_PIN, INPUT);

  wifi_connect();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("Init complete.\n");
}

void loop() {
  read_scd();
  read_pms();
  read_sgp();
  read_mics();
  read_vbus();
  draw_display();
  serial_log();
  mqtt.loop();
  mqtt_publish();
  delay(LOOP_MS);
}
