# Koffing — Air Quality Monitor Build Plan

## Context

Building an air quality monitoring station ("Koffing") using an Arduino Nano ESP32 with four sensors and an OLED display. The ESP32's WiFi capability will eventually serve a dashboard, and a Koffing animation is a future stretch goal.

## Hardware Summary

| Component | Interface | I2C Addr | Status |
|-----------|-----------|----------|--------|
| PMSA003I (PM2.5) | I2C / STEMMA QT | 0x12 | **Working** |
| SGP40 (VOC) | I2C / STEMMA QT | 0x59 | **Working** |
| SCD4x (CO2/Temp/Humidity) | I2C / STEMMA QT | 0x62 | **Working** (was blocked — needed real power supply, not USB from computer) |
| MiCS5524 (CO/VOC) | Analog (Ao pin) | N/A | Deferred — not wired yet |
| OLED 128x64 SSD1306 | I2C (GND/VCC/SCL/SDA) | 0x3C | **Working** |
| Arduino Nano ESP32 | — | — | Board core installed |

No I2C address conflicts.

## Completed

- [x] Step 1: Project documentation (CLAUDE.md, research/, wiring/, plans/)
- [x] Step 2: Install SSD1306 library
- [x] Step 3: Hardware validation — OLED, PMSA003I, SGP40 all confirmed working
- [x] OLED wired by user (GND→GND, VCC→3V3, SCL→A5, SDA→A4)
- [x] Step 4: Main koffing.ino — reads PMSA003I + SGP40 + SCD4x (graceful degradation), OLED with Koffing sprite + sensor data + status dots
- [x] Koffing pixel art sprite library (art/include/koffing_gfx.h)

## Resolved blocker

**SCD4x** was not producing data when powered via USB from a computer. Works fine on real power supply. Root cause was likely insufficient current — SCD4x peaks at 205mA during measurement. Debugging log preserved in `research/scd4x_debugging.md`.

## Remaining work

### Step 4: Write koffing.ino (main sketch)
Can proceed with working sensors (PMSA003I, SGP40, OLED). SCD4x support should be included but gracefully handle the sensor being unavailable.

Architecture:
- `setup()`: Init Serial, Wire, each sensor, OLED
- `loop()`: Read all sensors (respecting timing), update OLED display
- Sensor reads wrapped in individual functions
- SGP40 uses default 25°C/50%RH compensation until SCD4x is fixed (then use SCD4x temp/humidity)
- OLED shows 2-3 pages of data, auto-cycling every few seconds
- Data struct holds all readings (ready for future WiFi serialization)

Key timing:
- SGP40: 1-second reads, 45-second warmup before valid VOC index
- PMSA003I: 3-second boot delay, then ~1 read/sec
- SCD4x: 5-second interval (when working)
- Main loop: 5 seconds

### Step 5: Compile, upload, verify
```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
```

### Phase 2: WiFi + MQTT reporting — IN PROGRESS
ESP32 publishes sensor data over MQTT to a self-hosted server stack.

- [x] Add WiFi + MQTT to koffing.ino (PubSubClient, non-blocking reconnect)
- [x] secrets.h template for WiFi/MQTT credentials
- [x] Server configs (Mosquitto, Telegraf, InfluxDB, Grafana) in server/
- [x] Brewfile + setup.sh for one-command server install
- [x] Grafana dashboard with 5 panels (PM2.5, CO2, VOC, Temp, Humidity)
- [ ] Test end-to-end: ESP32 → MQTT → InfluxDB → Grafana
- [ ] Deploy to server MacBook Air

Architecture decision: [research/server_stack.md](../research/server_stack.md)

### Future phases
- **Phase 3:** MiCS5524 analog sensor integration (Ao→A0, ADC1 pin — safe with WiFi)
- **Phase 4:** Koffing animation on OLED

## Build commands
```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
```

## Monitor helper
```bash
python3 monitor.py [wait_seconds] [duration_seconds]
```
Note: `arduino-cli monitor` sometimes fails to produce output. May need `pyserial` for reliable monitoring, or use Arduino IDE serial monitor.

## Key files
- `koffing.ino` — main sketch (currently contains SCD4x isolation test v5)
- `monitor.py` — serial monitor helper script
- `research/` — sensor docs and debugging logs
- `wiring/i2c_chain.md` — wiring diagram
