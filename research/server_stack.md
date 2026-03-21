# Server Stack — WiFi Reporting Architecture

## Problem

Koffing reads air quality every 5 seconds but only displays it on a tiny OLED. No historical data, no remote access, no trends.

## Options Evaluated

### 1. Direct HTTP server on ESP32
Run a web server on the ESP32 itself. Any device on LAN hits `http://<esp-ip>/` to see current readings.

- No dependencies, works immediately
- No historical data — only shows the current moment
- No dashboards or graphs
- LAN-only, nothing persisted

Verdict: too limited for anything beyond a quick demo.

### 2. Cloud services (Adafruit IO, ThingSpeak)
Push readings to a hosted platform that provides storage + dashboards.

**Adafruit IO free tier (as of 2026-03):**
- 30 data points per minute (Koffing produces ~7 fields at 12 reads/min = 84/min — already over budget)
- 30 days of data storage
- 10 feeds max
- 5 dashboards

**ThingSpeak** has similar constraints on the free tier.

Verdict: free tiers are too restrictive for our data volume. Paid tiers exist but self-hosting is free and unlimited.

### 3. Home Assistant
Smart home hub that ingests sensor data via MQTT or native API. Excellent for automation (e.g. "turn on air purifier when PM2.5 > 15").

- Great if you already run Home Assistant
- Overkill if you just want charts and historical data
- More infrastructure than the alternatives

Verdict: not needed now. Could layer on later since it speaks MQTT.

### 4. MQTT → self-hosted stack (Mosquitto + Telegraf + InfluxDB + Grafana)
ESP32 publishes JSON over MQTT. Four lightweight services on a home server handle the rest.

- Unlimited data points, unlimited retention
- Industry-standard IoT stack with huge community
- Runs on any machine (including an old MacBook Air alongside Jellyfin)
- All free, open source
- Extensible — add Home Assistant, alerts, or additional sensors later

Verdict: **chosen approach.**

## Architecture

```
ESP32 (koffing.ino)
  │
  │  WiFi → publishes JSON every 5s
  │
  ▼
Mosquitto (MQTT broker, port 1883)
  │
  │  Telegraf subscribes to koffing/sensors topic
  │
  ▼
Telegraf (data bridge)
  │
  │  Parses JSON, writes time-series data
  │
  ▼
InfluxDB v2 (time-series database, port 8086)
  │
  │  Flux queries
  │
  ▼
Grafana (dashboards, port 3000)
  │
  └──→ Any browser on the network
```

## Component Details

### Mosquitto (MQTT broker)
- Written in C, extremely lightweight
- Receives messages from ESP32, forwards to subscribers (Telegraf)
- No persistence needed — InfluxDB handles storage
- Config: `listener 1883`, `allow_anonymous true`

### Telegraf (data bridge)
- Written in Go, by the InfluxDB team
- Subscribes to MQTT topic `koffing/sensors`
- Parses JSON payload into individual fields
- Writes to InfluxDB as measurement `air_quality`
- Handles buffering, retries, batching automatically

### InfluxDB v2 (time-series database)
- Written in Go, optimized for timestamped data
- Stores readings with automatic compression
- Months of 5-second sensor data = megabytes of disk
- Query language: Flux

### Grafana (visualization)
- Written in Go + TypeScript/React
- Serves dashboards as a web UI on port 3000
- Accessible from any device on the network (phone, laptop, tablet)
- Pre-provisioned with 5 panels: PM2.5, CO2, VOC, Temperature, Humidity

## Why Homebrew services over Docker

- User already runs Jellyfin natively on macOS
- Single-machine setup, no orchestration needed
- `brew services` manages start/stop/autostart
- Simpler mental model — just four background services
- Docker would work too, but adds a layer for no benefit here

## MQTT Payload Format

Topic: `koffing/sensors`

```json
{"uptime":12345,"pm25":12,"voc":95,"co2":650,"temp_f":72.3,"humidity":48.1}
```

Fields are omitted if the corresponding sensor hasn't produced data yet. `uptime` (seconds since boot) is always present.
