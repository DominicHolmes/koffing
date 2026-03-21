# PMSA003I — PM2.5 Air Quality Sensor

## Basics
- **I2C Address:** 0x12 (fixed)
- **Library:** `Adafruit PM25 AQI Sensor`
- **Protocol:** Plantower, 32-byte packets with CRC8 checksum

## API
```cpp
#include "Adafruit_PM25AQI.h"
Adafruit_PM25AQI aqi;
PM25_AQI_Data data;

aqi.begin_I2C();        // init on default Wire
aqi.read(&data);        // returns true on success
```

## Data Fields (PM25_AQI_Data)
| Field | Unit | Description |
|-------|------|-------------|
| `pm10_env` | µg/m³ | PM 1.0 (environmental) |
| `pm25_env` | µg/m³ | PM 2.5 (environmental) |
| `pm100_env` | µg/m³ | PM 10.0 (environmental) |
| `pm10_standard` | µg/m³ | PM 1.0 (standard) |
| `pm25_standard` | µg/m³ | PM 2.5 (standard) |
| `pm100_standard` | µg/m³ | PM 10.0 (standard) |
| `particles_03um` .. `particles_100um` | count/0.1L | Particle counts by size |
| `aqi_pm25_us` | — | Calculated US AQI for PM2.5 |
| `aqi_pm100_us` | — | Calculated US AQI for PM10 |

`read()` automatically calls `ConvertAQIData()` to populate AQI fields.

## Quirks
- **3-second boot delay** required before `begin_I2C()`
- Endian-swapped raw data (library handles this)
- Checksum validated on every read; packet rejected if bad
- Typical read interval: 1 second
