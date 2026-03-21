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
arduino-cli lib install "PubSubClient"  # first time only
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
arduino-cli monitor -p /dev/cu.usbmodem*
```

## Server Stack
```bash
cd server && ./setup.sh  # installs and starts Mosquitto, Telegraf, InfluxDB, Grafana
```
- Mosquitto (MQTT broker) — port 1883
- Telegraf (MQTT → InfluxDB bridge)
- InfluxDB 3 (time-series storage) — port 8181, data at `~/.influxdb3_data/`
- Grafana (dashboards) — port 3000, data at `$(brew --prefix)/var/lib/grafana/`

## WiFi / MQTT
- `secrets.h` (gitignored) — WiFi SSID/password and MQTT broker IP. Copy from `secrets.h.example`.
- ESP32 publishes JSON to MQTT topic `koffing/sensors` every 5s
- Payload: `{"uptime":N,"pm25":N,"voc":N,"co2":N,"temp_f":N,"humidity":N}` (fields omitted if sensor has no data)
- Non-blocking reconnection — one attempt per loop, never stalls sensor reads
- PubSubClient library for MQTT

## Project Structure
- `koffing.ino` — main sketch (sensors + WiFi/MQTT)
- `secrets.h.example` — WiFi/MQTT credential template
- `server/` — server stack configs and setup script
- `research/` — sensor documentation, API notes, architecture decisions
- `examples/` — per-sensor minimal working examples
- `wiring/` — wiring diagrams
- `plans/` — build plans

## Alert Philosophy
Thresholds flag **suboptimal** conditions, not just dangerous ones. The goal is environment optimization — know when air quality is degrading performance, focus, or sleep, well before it reaches "unhealthy" levels.

## Key Design Decisions
- SCD4x provides temp/humidity for SGP40 VOC compensation (no separate SHT31)
- MiCS5524 deferred — analog sensor, not yet wired
- 5-second main loop interval (governed by SCD4x periodic measurement)
- Data struct serialized as JSON and published over MQTT (WiFi)
- WiFi uses ADC1-safe pins only (ADC2 unavailable when WiFi active)
