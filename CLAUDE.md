# Koffing — Air Quality Monitor

Arduino Nano ESP32-based air quality monitoring station.

## Hardware
| Component | Interface | I2C Addr | Library |
|-----------|-----------|----------|---------|
| PMSA003I (PM2.5) | I2C / STEMMA QT | 0x12 | Adafruit PM25 AQI Sensor |
| SGP40 (VOC) | I2C / STEMMA QT | 0x59 | Adafruit SGP40 Sensor |
| SCD4x (CO2/Temp/Humidity) | I2C / STEMMA QT | 0x62 | Sensirion I2C SCD4x |
| MiCS5524 (CO/VOC) | Analog | N/A | None (raw analogRead) |
| OLED 128x64 SSD1306 | I2C | 0x3C | Adafruit SSD1306 |

## Board
Arduino Nano ESP32 (`arduino:esp32:nano_nora`). I2C on A4 (SDA) / A5 (SCL).

## Build
```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
arduino-cli monitor -p /dev/cu.usbmodem*
```

## Project Structure
- `koffing.ino` — main sketch
- `research/` — sensor documentation and API notes
- `examples/` — per-sensor minimal working examples
- `wiring/` — wiring diagrams
- `plans/` — build plans

## Key Design Decisions
- SCD4x provides temp/humidity for SGP40 VOC compensation (no separate SHT31)
- MiCS5524 deferred — analog sensor, not yet wired
- 5-second main loop interval (governed by SCD4x periodic measurement)
- Data stored in struct for future WiFi serialization (P2)
