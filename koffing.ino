// Koffing — Air Quality Monitor
// Arduino Nano ESP32 + PMSA003I + SGP40 + SCD4x + SSD1306 OLED

#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PM25AQI.h>
#include <Adafruit_SGP40.h>
#include <SensirionI2cScd4x.h>
#include "art/include/koffing_gfx.h"
#include "secrets.h"

#define SCREEN_W 128
#define SCREEN_H 64
#define LOOP_MS 5000
#define SCD4X_TIMEOUT_MS 300000  // 5 minutes — boots once, runs for days

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Adafruit_PM25AQI pms;
Adafruit_SGP40 sgp;
SensirionI2cScd4x scd;

WiFiClient espClient;
PubSubClient mqtt(espClient);

bool pms_ok = false;
bool sgp_ok = false;
bool scd_ok = false;
bool wifi_ok = false;
bool pms_got_data = false;
bool sgp_got_data = false;
bool scd_got_data = false;
unsigned long scd_start = 0;

struct Readings {
  uint16_t pm25;
  int32_t voc;
  uint16_t co2;
  float temp;
  float humidity;
} data = {0, 0, 0, 25.0, 50.0};

// --- WiFi + MQTT ---

void wifi_connect() {
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

void mqtt_publish() {
  if (WiFi.status() != WL_CONNECTED) {
    wifi_ok = false;
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    return;
  }
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
  snprintf(buf + n, sizeof(buf) - n, "}");
  mqtt.publish("koffing/sensors", buf);
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
  if (!scd_ok) return;
  bool ready = false;
  scd.getDataReadyStatus(ready);
  if (ready) {
    uint16_t co2;
    float t, h;
    if (scd.readMeasurement(co2, t, h) == 0 && co2 > 0) {
      data.co2 = co2;
      data.temp = t;
      data.humidity = h;
      scd_got_data = true;
    }
  }
  if (!scd_got_data && millis() - scd_start > SCD4X_TIMEOUT_MS) {
    scd_ok = false;
    Serial.println("SCD4x: timed out, disabling");
  }
}

// Map PM2.5 µg/m³ to Koffing level 0-10 (suboptimal-first scale)
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

float c_to_f(float c) { return c * 9.0 / 5.0 + 32.0; }

// --- Status dots: 4 squares at bottom-right (PMS, SGP, SCD, WiFi) ---

void draw_status(int16_t base_x, int16_t y) {
  const bool status[] = {pms_ok, sgp_ok, scd_ok, wifi_ok};
  for (uint8_t i = 0; i < 4; i++) {
    int16_t x = base_x + i * 5;
    if (status[i])
      display.fillRect(x, y, 3, 3, SSD1306_WHITE);
    else
      display.drawRect(x, y, 3, 3, SSD1306_WHITE);
  }
}

// Print text, inverse (black-on-white) when `alert` is true
void print_val(const char* label, int32_t val, const char* unit, bool alert) {
  display.print(label);
  if (alert) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.print(val);
  display.print(unit);
  if (alert) display.setTextColor(SSD1306_WHITE);
}

// --- Display ---

void draw_display() {
  display.clearDisplay();

  uint8_t level = pm25_to_level(data.pm25);
  koffing_draw(display, level, 0, 0);

  int16_t x = 66;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // PM2.5 — label
  display.setCursor(x, 0);
  display.print("PM2.5 ug");

  // PM2.5 — big value, inverse if >12 (above EPA "Good")
  display.setCursor(x, 10);
  display.setTextSize(2);
  if (pms_got_data) {
    if (data.pm25 > 12) display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(data.pm25);
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print("---");
  }
  display.setTextSize(1);

  // VOC — inverse if >100 (above Sensirion baseline average)
  display.setCursor(x, 28);
  if (!sgp_ok || !sgp_got_data) {
    display.print("VOC ---");
  } else {
    print_val("VOC ", data.voc, "", data.voc > 100);
  }

  // CO2 — inverse if >1500 ppm (poor indoor air)
  display.setCursor(x, 38);
  if (scd_ok && scd_got_data) {
    print_val("CO2 ", data.co2, "p", data.co2 > 800);
  } else {
    display.print("CO2 ---");
  }

  // Temp (F) + Humidity — only show if SCD4x is providing real data
  display.setCursor(x, 50);
  if (scd_got_data) {
    display.print((int)c_to_f(data.temp));
    display.print("F ");
    display.print((int)data.humidity);
    display.print("%");
  } else {
    display.print("--F --%");
  }

  // Version label bottom-left
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print("koffing v0");

  draw_status(108, 61);
  display.display();
}

// --- Serial logging ---

void serial_log() {
  Serial.print("PM2.5=");  Serial.print(pms_got_data ? (int)data.pm25 : -1);
  Serial.print(" VOC=");   Serial.print(sgp_got_data ? (int)data.voc : -1);
  Serial.print(" CO2=");   Serial.print(scd_got_data ? (int)data.co2 : -1);
  if (scd_got_data) {
    Serial.print(" T=");   Serial.print(c_to_f(data.temp), 1);
    Serial.print("F H=");  Serial.print(data.humidity, 1);
    Serial.print("%");
  }
  Serial.print(" LVL=");   Serial.println(pms_got_data ? pm25_to_level(data.pm25) : -1);
}

// --- Main ---

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== Koffing ===");

  Wire.begin();

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

  pms_ok = pms.begin_I2C();
  Serial.print("PMS:   "); Serial.println(pms_ok ? "OK" : "FAIL");

  sgp_ok = sgp.begin();
  Serial.print("SGP40: "); Serial.println(sgp_ok ? "OK" : "FAIL");

  scd.begin(Wire, 0x62);
  scd.wakeUp();
  delay(50);
  scd.stopPeriodicMeasurement();
  delay(500);
  scd_ok = (scd.startPeriodicMeasurement() == 0);
  scd_start = millis();
  Serial.print("SCD4x: "); Serial.println(scd_ok ? "started" : "FAIL");

  wifi_connect();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.println("Init complete.\n");
}

void loop() {
  read_pms();
  read_sgp();
  read_scd();
  draw_display();
  serial_log();
  mqtt.loop();
  mqtt_publish();
  delay(LOOP_MS);
}
