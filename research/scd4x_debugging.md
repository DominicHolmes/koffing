# SCD4x Debugging Log — 2026-03-20

## Summary
The SCD4x (SCD40 variant, serial D763D073BB2) communicates over I2C at 0x62 but never produces measurement data. All other sensors (PMSA003I, SGP40, OLED) work correctly.

## What works
- I2C scan finds device at 0x62
- `wakeUp()`, `stopPeriodicMeasurement()`, `reinit()`, `startPeriodicMeasurement()` all return error 0
- `getSerialNumber()` returns valid serial
- `getDataReadyStatus()` returns error 0

## What doesn't work
- `getDataReadyStatusRaw()` always returns `0x0000` (or briefly `0x8000` after factory reset, which is still "not ready" per the lower 11-bit mask)
- Data never becomes ready, even after 10+ minutes of periodic measurement
- `performSelfTest()` fails with error 527 = `ReadError | NotEnoughDataError`
- Tested with both regular and low-power periodic modes

## What was tried
1. Standard init sequence: wakeUp → stop → reinit → startPeriodic — **no data**
2. Removed I2C scan from loop (was interfering with bus) — **no change**
3. Added polling loop with retries for data ready — **no change**
4. Increased Wire timeout to 200ms, slowed clock to 50kHz — **no change**
5. Isolated SCD4x alone on I2C bus (no other sensors) — **no change**
6. Factory reset (`performFactoryReset()`) — returned 0 but **no change**
7. Tried without `reinit()` — **no change**
8. Ran for 10+ minutes continuously — **never produced data**

## Likely causes (untested)
- **Defective sensor** — I2C registers respond but measurement subsystem may be broken. Self-test failure (error 527) supports this.
- **Power issue** — SCD40 peaks at 205mA during measurement. USB power through Nano ESP32's 3.3V regulator may not supply enough. The LED was noticeably brighter when SCD4x was isolated (less bus load) but still didn't work.
- **ESP32 I2C clock stretching** — ESP32-S3 has known issues with long clock stretching. The SCD4x stretches clock during measurements. However, commands that require responses (serial number read) work fine.

## Next steps to try
- Test the SCD4x on a different board (standard Arduino Uno/Mega) to rule out ESP32-specific issues
- Try with external 3.3V power supply instead of Nano ESP32 regulator
- Try a different SCD4x unit if available
- Check if there's a newer version of the Sensirion I2C SCD4x library
