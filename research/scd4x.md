# SCD4x — CO2, Temperature & Humidity Sensor

## Basics
- **I2C Address:** 0x62
- **Library:** `Sensirion I2C SCD4x`
- **Measures:** CO2 (ppm), Temperature (°C), Humidity (%RH)

## API
```cpp
#include <SensirionI2cScd4x.h>
#include <Wire.h>
SensirionI2cScd4x scd;

scd.begin(Wire, SCD41_I2C_ADDR_62);
scd.wakeUp();
scd.stopPeriodicMeasurement();
scd.reinit();
scd.startPeriodicMeasurement();  // 5-second interval

// In loop:
bool ready = false;
scd.getDataReadyStatus(ready);
if (ready) {
    uint16_t co2;
    float temp, humidity;
    scd.readMeasurement(co2, temp, humidity);
}
```

## Measurement Modes
| Mode | Interval | Notes |
|------|----------|-------|
| Periodic | 5 sec | Default, most accurate |
| Low-power periodic | 30 sec | Power saving |
| Single shot | 5 sec min | SCD41 only |

## Calibration
- **ASC (Auto Self-Calibration):** Enabled by default. Assumes sensor sees 400 ppm (outdoor air) for 4+ hours at least once per week.
- **Forced recalibration:** Requires 3+ min operation in known CO2 concentration first.
- **Temperature offset:** Compensate for self-heating (0–20°C range). Use `setTemperatureOffset()`.
- **Altitude/Pressure:** `setSensorAltitude()` or `setAmbientPressure()` for compensation.

## Quirks
- Must call `stopPeriodicMeasurement()` before changing any config
- `persistSettings()` saves to EEPROM — **only 2000 write cycles**, don't call frequently
- `readMeasurement()` returns error if called before data ready
- All methods return `int16_t` error code (0 = success)
- Startup sequence: `wakeUp()` → `stopPeriodicMeasurement()` → `reinit()` → `startPeriodicMeasurement()`
