# Koffing — Air Quality Monitor

Arduino Nano ESP32-based air quality monitoring station.

## Hardware
| Component | Interface | I2C Addr | Library |
|-----------|-----------|----------|---------|
| PMSA003I (PM2.5) | I2C Wire1 (D2/D3) via STEMMA QT-to-headers | 0x12 | Adafruit PM25 AQI Sensor |
| SGP40 (VOC) | I2C Wire / STEMMA QT | 0x59 | Adafruit SGP40 Sensor |
| SCD4x (CO2/Temp/Humidity) | I2C Wire / STEMMA QT | 0x62 | Raw Wire I2C (no library) |
| MiCS5524 (CO/VOC) | Analog (A0) | N/A | None (raw analogRead) |
| VBUS voltage divider | Analog (A1) | N/A | None (2x 10kΩ divider) |
| OLED 128x64 SSD1306 | I2C | 0x3C | Adafruit SSD1306 |

## Board
Arduino Nano ESP32 (`arduino:esp32:nano_nora`). Two I2C buses:
- **Wire** (A4/A5) — OLED, SGP40, SCD4x (STEMMA QT daisy chain)
- **Wire1** (D2/D3) — PMSA003I (separate bus, STEMMA QT-to-male-headers cable)

PMSA003I must be on a separate I2C bus — it disrupts SCD4x measurements when sharing Wire.

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
- Payload: `{"uptime":N,"pm25":N,"voc":N,"co2":N,"temp_f":N,"humidity":N,"gas":N,"vbus":N}` (fields omitted if sensor has no data)
- Non-blocking reconnection — one attempt per loop, never stalls sensor reads
- PubSubClient library for MQTT

## Serial Debugging
```bash
# Monitor serial output with timestamps, logged to logs/serial_YYYYMMDD_HHMMSS.log
uv run --with pyserial monitor.py

# Reset board on connect (captures full boot sequence)
uv run --with pyserial monitor.py --reset

# Time-limited capture (e.g. 60 seconds)
uv run --with pyserial monitor.py --duration 60

# Stdout only, no log file
uv run --with pyserial monitor.py --no-log
```
Debug sketches live in `examples/` — compile and upload them instead of the main sketch:
```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora examples/scd40_debug
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora examples/scd40_debug
```
After debugging, re-upload the main sketch from the project root.

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
- MiCS5524 on A0 (ADC1, WiFi-safe) — raw analog, 3-min warmup before data flows
- VBUS voltage divider on A1 (ADC1, WiFi-safe) — 2x 10kΩ, warns below 4.75V
- 5-second main loop interval (governed by SCD4x periodic measurement)
- Data struct serialized as JSON and published over MQTT (WiFi)
- WiFi uses ADC1-safe pins only (ADC2 unavailable when WiFi active)
