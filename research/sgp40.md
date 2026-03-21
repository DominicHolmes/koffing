# SGP40 — MOX VOC Gas Sensor

## Basics
- **I2C Address:** 0x59 (fixed)
- **Library:** `Adafruit SGP40 Sensor`
- **Measures:** Raw gas signal + computed VOC Index (0–500)

## API
```cpp
#include "Adafruit_SGP40.h"
Adafruit_SGP40 sgp;

sgp.begin();                                    // init + self-test (500ms)
uint16_t raw = sgp.measureRaw(tempC, humidity); // raw signal
int32_t voc = sgp.measureVocIndex(tempC, humidity); // VOC Index 0-500
```

## Temperature/Humidity Compensation
SGP40 requires ambient temp and humidity for accurate VOC calculation. Defaults to 25°C / 50% RH if not provided. **We use SCD4x readings for this.**

## VOC Index Algorithm (Sensirion)
- Proprietary algorithm with internal state tracking
- **45-second blackout** — returns 0 until algorithm has enough data
- Continuous learning: adapts baseline over ~12 hours
- Index 100 = average conditions; >100 = worsening; <100 = improving
- Gating: freezes estimator during high VOC events (up to 180 min)
- State can be saved/restored with `VocAlgorithm_get_states()` / `set_states()`

## Tunable Parameters
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `voc_index_offset` | 100 | 1–250 | Baseline offset |
| `learning_time_hours` | 12 | 1–72 | Time to adapt |
| `gating_max_duration_min` | 180 | 0–720 | Max freeze during high VOC |
| `std_initial` | 50 | 10–500 | Initial standard deviation |

## Quirks
- 250ms delay built into each measurement command
- Self-test runs during `begin()` — takes 500ms
- CRC8 (poly 0x31) on all I2C responses
- `softReset()` affects ALL I2C devices on the bus
- Read interval: 1 second recommended
