# Koffing

Air quality monitor built around an Arduino Nano ESP32. Reads PM2.5, CO2, VOC, temperature, and humidity, then shows everything on a little OLED with an animated Koffing sprite whose mood tracks how bad the air is.

![Koffing states](art/preview/preview.png)

## Hardware

| Component | What it measures | Interface | I2C Addr |
|-----------|-----------------|-----------|----------|
| [PMSA003I](research/pmsa003i.md) | PM1.0 / PM2.5 / PM10 particulate matter | I2C / STEMMA QT | 0x12 |
| [SGP40](research/sgp40.md) | VOC index (1-500) | I2C / STEMMA QT | 0x59 |
| [SCD4x](research/scd4x.md) | CO2 (ppm), temperature, humidity | I2C / STEMMA QT | 0x62 |
| [MiCS5524](research/mics5524.md) | CO / combustible gases (analog) | Analog | N/A |
| [OLED SSD1306](research/oled_ssd1306.md) | 128x64 mono display | I2C | 0x3C |
| [Arduino Nano ESP32](research/nano_esp32.md) | Microcontroller (WiFi-capable) | -- | -- |

The SCD4x handles temp/humidity compensation for the SGP40, so no separate SHT31 needed. MiCS5524 isn't wired up yet. See [SCD4x debugging notes](research/scd4x_debugging.md) for a known data-ready quirk.

## Wiring

All I2C sensors daisy-chained via STEMMA QT. OLED on the same bus with jumper wires.

```
                          STEMMA QT daisy chain
Arduino Nano ESP32 ──QT──> PMSA003I ──QT──> SGP40 ──QT──> SCD4x
    A4 (SDA) ─────┐          0x12           0x59          0x62
    A5 (SCL) ───┐ │
                │ │
    GND ──────┐ │ │
    3V3 ────┐ │ │ │       OLED SSD1306 128x64
            │ │ │ │       ┌─────────────────┐
            │ │ │ └─ SDA ─┤ SDA             │
            │ │ └── SCL ──┤ SCL       0x3C  │
            │ └─── GND ──┤ GND             │
            └──── VCC ───┤ VCC             │
                          └─────────────────┘
```

MiCS5524 will eventually go on A0 (5V from VBUS, En! tied to GND).

Full wiring details: [wiring/i2c_chain.md](wiring/i2c_chain.md)

## Build

```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
arduino-cli monitor -p /dev/cu.usbmodem*
```

## Alert thresholds

These are tuned to flag *suboptimal* conditions, not just dangerous ones. The point is knowing when your air is hurting focus or sleep, not waiting until it's actually unhealthy. Values that cross a threshold show as inverted (black-on-white) on the OLED.

| Sensor | Threshold | What it means |
|--------|-----------|---------------|
| PM2.5 | > 12 µg/m³ | Above EPA "Good." Open a window or run a filter. |
| VOC | > 100 | Sensirion baseline is 100. Above that = air getting worse. |
| CO2 | > 800 ppm | Cognitive performance drops measurably around 800-1000 ppm. |

## Koffing sprite

The OLED shows an animated Koffing whose expression matches air quality. Worse air → bigger grin → more gas clouds on screen.

The sprite takes a single integer (0 = clean, 10 = hazardous):

```c
#include "art/include/koffing_gfx.h"

koffing_draw(display, level, 0, 0);
```

| Level | Face | Clouds |
|-------|------|--------|
| 0 | Happy | None |
| 1-4 | Grin | 1-4, small wisps |
| 5-6 | Thrilled | 5-6, medium puffs |
| 7-10 | Ecstatic | 7-10, big billows |

PM2.5 mapping:

| PM2.5 µg/m³ | Level |
|--------------|-------|
| 0-3 | 0 |
| 4-6 | 1 |
| 7-9 | 2 |
| 10-12 | 3 |
| 13-20 | 4-5 |
| 21-35 | 6-7 |
| 36-55 | 8 |
| 56-100 | 9 |
| 100+ | 10 |

### Regenerating art

Sprites are pixel grids defined in Python, exported as C headers (mono bitmaps + indexed color with RGB565 palettes).

```bash
cd art/generator
uv run python generate.py
```

Outputs `art/include/*.h` (what the sketch uses) and `art/preview/` (PNGs for review). Color sprites use a 4-bit indexed palette, so a shiny variant would just be a palette swap.

## Project structure

```
koffing.ino              Main sketch
research/                Sensor docs and API notes
wiring/                  Wiring diagrams
plans/                   Build plans
art/
  include/               Arduino C headers (sprite data + library)
    koffing_gfx.h        Public API — only file you need to include
  preview/               Human-reviewable images
  generator/             Python sprite generator
```

## Research

- [PMSA003I — PM2.5 particulate sensor](research/pmsa003i.md)
- [SGP40 — VOC index sensor](research/sgp40.md)
- [SCD4x — CO2 / temp / humidity sensor](research/scd4x.md)
- [SCD4x debugging log](research/scd4x_debugging.md)
- [MiCS5524 — CO / combustible gas sensor](research/mics5524.md)
- [OLED SSD1306 — 128x64 display](research/oled_ssd1306.md)
- [Arduino Nano ESP32 — board notes](research/nano_esp32.md)
- [Build plan](plans/build_plan.md)
