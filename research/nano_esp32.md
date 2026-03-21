# Arduino Nano ESP32

## Board
- **FQBN:** `arduino:esp32:nano_nora`
- **MCU:** ESP32-S3
- **Operating voltage:** 3.3V (all GPIO)

## I2C
- **SDA:** A4 (default)
- **SCL:** A5 (default)
- `Wire.begin()` uses these by default
- All STEMMA QT sensors + OLED share this bus
- STEMMA QT breakouts have built-in 10K pull-ups — do NOT add external pull-ups

## ADC (for MiCS5524)
- 12-bit resolution (0–4095 → 0–3.3V)
- **ADC1 (A0–A7):** Always available, even with WiFi
- **ADC2:** Unavailable when WiFi is active — avoid for sensors
- `analogReadMilliVolts(pin)` for calibrated mV readings

## WiFi
- ESP32-S3 has built-in WiFi (802.11 b/g/n)
- Relevant for P2 dashboard feature
- Remember: ADC2 pins stop working when WiFi is on
