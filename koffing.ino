// Koffing — Air Quality Monitor
// Arduino Nano ESP32 + PMSA003I + SGP40 + SCD4x + SSD1306 OLED

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_PM25AQI.h>
#include <Adafruit_SGP40.h>
#include <SensirionI2cScd4x.h>
#include "art/include/koffing_gfx.h"

#define SCREEN_W 128
#define SCREEN_H 64
#define LOOP_MS 5000
#define SCD4X_TIMEOUT_MS 30000
#define PAGE_MS 4000
#define NUM_PAGES 2

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
Adafruit_PM25AQI pms;
Adafruit_SGP40 sgp;
SensirionI2cScd4x scd;

bool pms_ok = false;
bool sgp_ok = false;
bool scd_ok = false;
bool scd_got_data = false;
unsigned long scd_start = 0;

struct Readings {
  uint16_t pm25;
  uint16_t aqi;
  int32_t voc;
  uint16_t co2;
  float temp;
  float humidity;
} data = {0, 0, 0, 0, 25.0, 50.0};

uint8_t page = 0;
unsigned long last_page = 0;

// --- Sensor reads ---

void read_pms() {
  if (!pms_ok) return;
  PM25_AQI_Data d;
  if (pms.read(&d)) {
    data.pm25 = d.pm25_env;
    data.aqi = d.pm25_env; // raw µg/m³ as fallback; see aqi_to_level()
  }
}

void read_sgp() {
  if (!sgp_ok) return;
  data.voc = sgp.measureVocIndex(data.temp, data.humidity);
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

// Map PM2.5 µg/m³ to Koffing level 0-10
// Roughly follows EPA breakpoints: 0-12 good, 12-35 moderate, 35-55 USG, etc.
uint8_t pm25_to_level(uint16_t pm25) {
  if (pm25 <= 12)  return 0;
  if (pm25 <= 35)  return constrain(map(pm25, 13, 35, 1, 3), 1, 3);
  if (pm25 <= 55)  return constrain(map(pm25, 36, 55, 4, 5), 4, 5);
  if (pm25 <= 150) return constrain(map(pm25, 56, 150, 6, 8), 6, 8);
  if (pm25 <= 250) return 9;
  return 10;
}

// --- Status dots ---
// 3 dots at bottom-right: PMS, SGP, SCD
// Filled = ok, hollow = failed/missing

void draw_status(int16_t base_x, int16_t y) {
  const bool status[] = {pms_ok, sgp_ok, scd_ok};
  for (uint8_t i = 0; i < 3; i++) {
    int16_t x = base_x + i * 5;
    if (status[i]) {
      display.fillRect(x, y, 3, 3, SSD1306_WHITE);
    } else {
      display.drawRect(x, y, 3, 3, SSD1306_WHITE);
    }
  }
}

// --- Display pages ---

void draw_page_koffing() {
  uint8_t level = pm25_to_level(data.pm25);
  koffing_draw(display, level, 0, 0);

  int16_t x = 66;
  display.setTextSize(1);

  display.setCursor(x, 0);
  display.print("PM2.5");
  display.setCursor(x, 10);
  display.setTextSize(2);
  display.print(data.pm25);
  display.setTextSize(1);

  display.setCursor(x, 28);
  display.print("VOC ");
  display.print(data.voc);

  display.setCursor(x, 38);
  if (scd_ok && scd_got_data) {
    display.print("CO2 ");
    display.print(data.co2);
  } else {
    display.print("CO2 ---");
  }

  display.setCursor(x, 48);
  display.print(data.temp, 0);
  display.print("C ");
  display.print(data.humidity, 0);
  display.print("%");

  draw_status(113, 61);
}

void draw_page_detail() {
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("PM2.5: ");
  display.print(data.pm25);
  display.println(" ug/m3");

  display.print("VOC:   ");
  display.println(data.voc);

  display.print("CO2:   ");
  if (scd_ok && scd_got_data) {
    display.print(data.co2);
    display.println(" ppm");
  } else {
    display.println("---");
  }

  display.println();

  display.print("Temp:  ");
  display.print(data.temp, 1);
  display.println(" C");

  display.print("Hum:   ");
  display.print(data.humidity, 1);
  display.println(" %");

  display.println();
  display.print("Level: ");
  display.print(pm25_to_level(data.pm25));
  display.print("/");
  display.print(KOFFING_LEVEL_MAX);

  draw_status(113, 61);
}

void update_display() {
  if (millis() - last_page > PAGE_MS) {
    page = (page + 1) % NUM_PAGES;
    last_page = millis();
  }
  display.clearDisplay();
  if (page == 0) draw_page_koffing();
  else           draw_page_detail();
  display.display();
}

// --- Serial logging ---

void serial_log() {
  Serial.print("PM2.5=");  Serial.print(data.pm25);
  Serial.print(" VOC=");   Serial.print(data.voc);
  Serial.print(" CO2=");   Serial.print(scd_got_data ? data.co2 : 0);
  Serial.print(" T=");     Serial.print(data.temp, 1);
  Serial.print(" H=");     Serial.print(data.humidity, 1);
  Serial.print(" LVL=");   Serial.println(pm25_to_level(data.pm25));
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

  last_page = millis();
  Serial.println("Init complete.\n");
}

void loop() {
  read_pms();
  read_sgp();
  read_scd();
  update_display();
  serial_log();
  delay(LOOP_MS);
}
