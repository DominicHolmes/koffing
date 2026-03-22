// SCD40 Debug Sketch — isolated diagnostics for the SCD4x CO2 sensor
// Compile: arduino-cli compile --fqbn arduino:esp32:nano_nora examples/scd40_debug
// Upload:  arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora examples/scd40_debug

#include <Wire.h>
#include <SensirionI2cScd4x.h>

SensirionI2cScd4x scd;

void i2c_scan() {
  Serial.println("\n--- I2C Bus Scan ---");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      if (addr == 0x3C) Serial.print(" (SSD1306 OLED)");
      if (addr == 0x12) Serial.print(" (PMSA003I)");
      if (addr == 0x59) Serial.print(" (SGP40)");
      if (addr == 0x62) Serial.print(" (SCD4x)");
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("  NO DEVICES FOUND — check wiring!");
  Serial.print("Total devices: ");
  Serial.println(found);
}

void print_error(const char* label, uint16_t err) {
  if (err == 0) {
    Serial.print(label);
    Serial.println(": OK");
    return;
  }
  Serial.print(label);
  Serial.print(": ERROR 0x");
  Serial.print(err, HEX);
  char msg[64];
  errorToString(err, msg, sizeof(msg));
  Serial.print(" — ");
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n========================================");
  Serial.println("  SCD40 Debug Sketch");
  Serial.println("========================================");

  Wire.begin();
  i2c_scan();

  // Check raw I2C reachability before using the library
  Serial.println("\n--- Raw I2C probe at 0x62 ---");
  Wire.beginTransmission(0x62);
  uint8_t probe = Wire.endTransmission();
  if (probe == 0) {
    Serial.println("  ACK received — sensor is on the bus");
  } else {
    Serial.print("  NACK (error ");
    Serial.print(probe);
    Serial.println(") — sensor not responding!");
    Serial.println("  Check: STEMMA QT cable seated? Power LED on breakout?");
    Serial.println("  Halting. Fix wiring and reset.");
    while (1) delay(1000);
  }

  scd.begin(Wire, 0x62);

  // CRITICAL: stop any in-progress measurement first.
  // The SCD4x ignores most commands while measuring.
  // If the ESP32 rebooted but the SCD4x kept running, it's still in periodic mode.
  Serial.println("\n--- Stop any existing measurement ---");
  uint16_t err = scd.wakeUp();
  print_error("wakeUp()", err);
  delay(50);

  err = scd.stopPeriodicMeasurement();
  print_error("stopPeriodicMeasurement()", err);
  delay(500);

  // Read serial number (only works when NOT measuring)
  Serial.println("\n--- Read serial number ---");
  uint64_t serialNum;
  err = scd.getSerialNumber(serialNum);
  if (err == 0) {
    Serial.print("  Serial: 0x");
    Serial.println((unsigned long)serialNum, HEX);
  } else {
    print_error("getSerialNumber()", err);
  }

  // Self-test (takes ~10 seconds)
  Serial.println("\n--- Self test (takes ~10s) ---");
  uint16_t selfTestResult;
  err = scd.performSelfTest(selfTestResult);
  if (err == 0) {
    if (selfTestResult == 0) {
      Serial.println("  Self-test PASSED");
    } else {
      Serial.print("  Self-test FAILED, malfunction code: 0x");
      Serial.println(selfTestResult, HEX);
    }
  } else {
    print_error("performSelfTest()", err);
  }

  // Reinit to clear any stale state
  Serial.println("\n--- Reinit ---");
  err = scd.reinit();
  print_error("reinit()", err);
  delay(50);

  // Start periodic measurement
  Serial.println("\n--- Start periodic measurement ---");
  err = scd.startPeriodicMeasurement();
  print_error("startPeriodicMeasurement()", err);

  if (err != 0) {
    Serial.println("Cannot start measurement — halting.");
    while (1) delay(1000);
  }

  Serial.println("\nWaiting for first data (expect ~5s)...\n");
  Serial.println("TIME(s)  READY  CO2(ppm)  TEMP(C)  TEMP(F)  HUMID(%)  ERR");
  Serial.println("-------  -----  --------  -------  -------  --------  ---");
}

unsigned long loop_count = 0;

void loop() {
  delay(1000);
  loop_count++;

  unsigned long secs = millis() / 1000;
  bool ready = false;
  uint16_t err = scd.getDataReadyStatus(ready);

  if (err != 0) {
    Serial.print(secs);
    Serial.print("s      ");
    char msg[64];
    errorToString(err, msg, sizeof(msg));
    Serial.print("getDataReadyStatus ERROR: 0x");
    Serial.print(err, HEX);
    Serial.print(" — ");
    Serial.println(msg);
    return;
  }

  if (!ready) {
    if (loop_count <= 10 || loop_count % 10 == 0) {
      Serial.print(secs);
      Serial.println("s      no     (waiting...)");
    }
    return;
  }

  uint16_t co2;
  float temp, humidity;
  err = scd.readMeasurement(co2, temp, humidity);

  char buf[100];
  if (err == 0) {
    float temp_f = temp * 9.0 / 5.0 + 32.0;
    snprintf(buf, sizeof(buf), "%lus      YES    %-8u  %-7.1f  %-7.1f  %-8.1f  OK",
             secs, co2, temp, temp_f, humidity);
    if (co2 == 0) {
      strcat(buf, " (co2=0, invalid sample)");
    }
  } else {
    char msg[64];
    errorToString(err, msg, sizeof(msg));
    snprintf(buf, sizeof(buf), "%lus      YES    readMeasurement ERROR: 0x%X — %s",
             secs, err, msg);
  }
  Serial.println(buf);
}
