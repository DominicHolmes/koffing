# Koffing

Air quality monitor with personality. An Arduino Nano ESP32 reads PM2.5, CO2, VOC, temperature, and humidity, then displays the results on an OLED alongside an animated Koffing whose expression and gas clouds reflect how bad (or good) the air is.

## Hardware

| Component | What it measures | Interface | I2C Addr |
|-----------|-----------------|-----------|----------|
| [PMSA003I](research/pmsa003i.md) | PM1.0 / PM2.5 / PM10 particulate matter | I2C / STEMMA QT | 0x12 |
| [SGP40](research/sgp40.md) | VOC index (1-500) | I2C / STEMMA QT | 0x59 |
| [SCD4x](research/scd4x.md) | CO2 (ppm), temperature, humidity | I2C / STEMMA QT | 0x62 |
| [MiCS5524](research/mics5524.md) | CO / combustible gases (analog) | Analog | N/A |
| [OLED SSD1306](research/oled_ssd1306.md) | 128x64 mono display | I2C | 0x3C |
| [Arduino Nano ESP32](research/nano_esp32.md) | Microcontroller (WiFi-capable) | -- | -- |

SCD4x provides temp/humidity compensation for the SGP40 VOC sensor, eliminating the need for a separate SHT31. MiCS5524 is deferred (analog, not yet wired). See [SCD4x debugging notes](research/scd4x_debugging.md) for a known data-ready issue.

## Wiring

All I2C sensors are daisy-chained via STEMMA QT. The OLED connects to the same I2C bus with jumper wires.

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

Future: MiCS5524 analog sensor on A0 (5V from VBUS, En! tied to GND).

Full wiring details: [wiring/i2c_chain.md](wiring/i2c_chain.md)

## Build

```bash
arduino-cli compile --fqbn arduino:esp32:nano_nora .
arduino-cli upload -p /dev/cu.usbmodem* --fqbn arduino:esp32:nano_nora .
arduino-cli monitor -p /dev/cu.usbmodem*
```

## Koffing sprite

The OLED displays an animated Koffing whose mood tracks air quality. As pollution increases, he grins wider and gas clouds fill the screen — he's a poison gas Pokemon, after all.

![Koffing states](art/preview/preview.png)

**Levels 0-10**: The sprite system takes a single integer (0 = clean air, 10 = hazardous) and handles face selection and cloud placement. Clouds accumulate incrementally — small wisps first, then medium puffs, then large billows.

| Level | Face | Description |
|-------|------|-------------|
| 0 | Happy | Content smile, no clouds |
| 1-4 | Grin | Squinting toothy grin, 1-4 clouds |
| 5-6 | Thrilled | Raised eyebrows, bigger grin, 5-6 clouds |
| 7-10 | Ecstatic | Wide-eyed mania, huge grin, 7-10 clouds |

### Using the sprite library

The integration code only needs one include and one function call:

```c
#include "art/include/koffing_gfx.h"

// Map your AQI to 0-10, then:
koffing_draw(display, level, 0, 0);
```

### Regenerating art

Sprites are defined as pixel grids in Python and exported as C headers (mono bitmaps + indexed color with RGB565 palettes). To regenerate after editing:

```bash
cd art/generator
uv run python generate.py
```

This outputs:
- `art/include/*.h` — C headers for Arduino (the only files the sketch needs)
- `art/preview/` — PNGs, XBMs, and a preview sheet for human review

Color sprites use a 4-bit indexed palette, so a future "shiny" Koffing variant (blue/grey body, pink clouds) is just a palette swap with zero additional sprite data.

## Project structure

```
koffing.ino              Main sketch
research/                Sensor documentation and API notes
wiring/                  Wiring diagrams
plans/                   Build plans
art/
  include/               Arduino C headers (sprite data + library)
    koffing_gfx.h        Public API — the only file you need to include
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
