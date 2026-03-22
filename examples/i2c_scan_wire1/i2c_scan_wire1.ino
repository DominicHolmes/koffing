// Scan both I2C buses — Wire (A4/A5) and Wire1 (D2/D3)

#include <Wire.h>

void scan_bus(TwoWire &bus, const char* name) {
  Serial.print("\n--- ");
  Serial.print(name);
  Serial.println(" scan ---");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      if (addr == 0x3C) Serial.print(" (SSD1306)");
      if (addr == 0x12) Serial.print(" (PMSA003I)");
      if (addr == 0x59) Serial.print(" (SGP40)");
      if (addr == 0x62) Serial.print(" (SCD4x)");
      Serial.println();
      found++;
    }
  }
  if (found == 0) Serial.println("  NO DEVICES FOUND");
  Serial.print("  Total: ");
  Serial.println(found);
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== Dual I2C Bus Scanner ===");

  Wire.begin();
  pinMode(D2, INPUT_PULLUP);
  pinMode(D3, INPUT_PULLUP);
  Wire1.begin(D2, D3);

  scan_bus(Wire, "Wire (A4/A5)");
  scan_bus(Wire1, "Wire1 (D2/D3)");

  Serial.println("\nDone. Rescanning every 5s...");
}

void loop() {
  delay(5000);
  scan_bus(Wire, "Wire (A4/A5)");
  scan_bus(Wire1, "Wire1 (D2/D3)");
}
