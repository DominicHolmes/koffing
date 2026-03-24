# SCD4x Debugging Log

## Datasheet Reference

[Sensirion SCD4x Datasheet v1.1 (April 2021)](https://cdn-learn.adafruit.com/assets/assets/000/104/015/original/Sensirion_CO2_Sensors_SCD4x_Datasheet.pdf?1629489682)

### I2C Protocol Summary
- Address: **0x62**
- All commands/data are 16-bit. Each data word followed by 8-bit CRC.
- Power-up: needs 1000 ms after VDD reaches 2.25V before accepting commands.
- Four sequence types: write, send command, read, send command and fetch result.
- For read sequences, must wait the command execution time before issuing the read header.

### Key Command Reference
| Command | Hex | Exec Time | During Periodic? |
|---------|------|-----------|-----------------|
| start_periodic_measurement | 0x21b1 | — | no |
| read_measurement | 0xec05 | 1 ms | yes |
| stop_periodic_measurement | 0x3f86 | 500 ms | yes |
| get_data_ready_status | 0xe4b8 | 1 ms | yes |
| perform_self_test | 0x3639 | 10000 ms | no |
| perform_factory_reset | 0x3632 | 1200 ms | no |
| reinit | 0x3646 | 20 ms | no |
| get_serial_number | 0x3682 | 1 ms | no |
| set_temperature_offset | 0x241d | 1 ms | no |
| set_ambient_pressure | 0xe000 | 1 ms | yes |
| persist_settings | 0x3615 | 800 ms | no |

### read_measurement Response Format
Returns 9 bytes: 3 words × (2 data bytes + 1 CRC byte)
- word[0]: CO2 in ppm (raw)
- word[1]: T = -45 + 175 × word[1] / 2^16 (°C)
- word[2]: RH = 100 × word[2] / 2^16 (%)

### Critical Constraints (from datasheet)
- While periodic measurement is running, **only** these commands are allowed: read_measurement, get_data_ready_status, stop_periodic_measurement, set_ambient_pressure.
- After stop_periodic_measurement, must wait **500 ms** before sending other commands.
- read_measurement buffer empties on read — only one read per 5s cycle. Returns NACK if no data available.
- Temperature offset default is 4°C. Must persist_settings to save to EEPROM.

---

## Session 5 — 2026-03-24: Periodic failure after 1-2 hours

### Observations
- Sensor worked for ~1.5 hours then stopped reporting.
- All SCD4x fields (CO2, Temp, Humidity) disappeared from MQTT/Grafana simultaneously at 16:08.
- PM2.5 (Wire1) and Gas (Analog) continued reporting, but Gas (MiCS5524) showed massive noise/spikes immediately following the SCD4x failure.
- VOC (SGP40) flatlined at 100 after a brief drop to 0, suggesting the ESP32 rebooted and the SGP40 lost its SCD4x-based compensation.

### Diagnosis: The 16:08:23 "Electrical Event"
The Grafana telemetry shows a cascading failure triggered by a specific event at **16:08:23**:
1.  **Sudden Silence (16:08:23 – 16:11:43):** Most sensors stop reporting simultaneously. This indicates the ESP32 likely entered a brown-out loop or hung due to power instability.
2.  **SCD4x Hard Lockout:** The SCD40 never recovers. Its final successful reading is at 16:08:23. Post-reboot, it fails to initialize, confirming an internal hardware fault that occurred at that timestamp.
3.  **MiCS5524 "Analog Death" (Gas):** The Gas sensor (Analog A0) shows the most dramatic evidence. Before the event, it was steady at ~700. After recovery (16:11:43), it begins oscillating wildly between **297 and 2469**. This is characteristic of a floating analog input or a collapsed reference voltage. If the SCD40 is internally shorting the 5V rail, the ESP32's internal ADC reference will be completely unstable.
4.  **PM2.5 Spike (16:09:21):** A single outlier of 29 µg/m³ appears during the recovery phase, likely a corrupted I2C frame or an artifact of the PMSA003I rebooting.
5.  **VOC Index Reset:** The SGP40 "dives" to 1 at 16:12:59 and then flatlines at 100. This is the behavior of a freshly rebooted SGP40 that has lost its history and is waiting for new compensation data (which it isn't getting because the SCD40 is dead).

**Conclusion:** The SCD40 experienced a catastrophic internal failure at 16:08:23 that significantly loaded the 5V power rail, causing an ESP32 brown-out and permanently damaging the analog reference/rail stability. The sensor is not just "not reading"; it is actively compromising the rest of the system's electrical integrity.

### Resolution/Next Steps
- The sensor is exhibiting hardware instability that persists across MCU resets.
- **Action:** Add a retry mechanism to `read_scd()` to attempt `scd_init()` periodically if `scd_ok` is false.
- **Action:** Request RMA from Adafruit. The self-test failure (`0x20F`) combined with these lockouts is conclusive evidence of a defective unit.

---

### Isolated debug sketch results
Ran `examples/scd40_debug` with SCD4x alone on Wire bus (PMSA003I on Wire1, no other traffic):
- Self-test: **FAILED** with `0x20F` (NotEnoughData) — degraded from `0x200` in session 2
- `startPeriodicMeasurement()`: returns 0 (OK)
- `getDataReadyStatus()`: never returns true, even after 90+ seconds
- No I2C errors — sensor ACKs commands but never produces data

### Resolution
Sensor is NOT dead — the Sensirion I2C library was the problem. Switching to raw Wire commands (mirroring bb_scd41's approach) fixed it. Key differences from the Sensirion library (discovered via [bb_scd41](https://github.com/bitbank2/bb_scd41)):
- No CRC on parameter-less commands (Sensirion wraps all frames with CRC)
- Wakeup → reinit as a single atomic sequence (20ms + 30ms delays)
- No `stopPeriodicMeasurement` before reinit
- Self-test failure (`0x200`/`0x20F`) is a red herring — sensor produces valid data despite failing self-test

### Main sketch behavior before diagnosis
- SCD4x init succeeded (`scd_ok = true`)
- `getDataReadyStatus` polled successfully but never returned ready
- Timed out after 5 minutes as expected (`SCD4X_TIMEOUT_MS`)
- All other sensors (PMSA003I, SGP40, MiCS5524) working correctly

---

## Session 3 — 2026-03-22: PMSA003I moved to Wire1

### Root cause confirmed
The PMSA003I was disrupting the SCD4x on the shared I2C bus. Systematic elimination:
1. SCD4x alone → works
2. SCD4x + SGP40 → works
3. SCD4x + PMSA003I → fails (data ready never true)
4. Main sketch with everything disabled except SCD4x → still fails if PMSA003I is **physically on the bus** (even without calling `begin()`)

The PMSA003I is a continuous I2C talker that disrupts the SCD4x's photoacoustic measurement cycle.

### Fix
Moved PMSA003I to Wire1 (second I2C bus) on D2 (SDA) / D3 (SCL). Required a STEMMA QT-to-male-headers cable because the bare header pins lack pull-up resistors (ESP32 internal pull-ups too weak at ~45K).

### SCD4x hardware concern
The sensor's self-test never passes (codes `0x200`, `0x20F`) and it intermittently stops producing data even in isolation. It worked for two readings at 2:40 PM on 3/21 then stopped. Factory reset did not help. Likely needs replacement.

---

## Session 2 — 2026-03-21: PARTIALLY RESOLVED

### What changed
Built a standalone debug sketch (`examples/scd40_debug/`) to isolate the SCD4x from all other sensor/WiFi code. The sensor **produced valid data** on first try.

### Debug sketch results
```
Serial: 0x3D073BB2
Self-test: FAILED (malfunction code 0x200)
startPeriodicMeasurement(): OK
20s  YES  CO2=249  TEMP=22.0C/71.6F  HUMID=53.9%  OK
25s  YES  CO2=306  TEMP=21.8C/71.2F  HUMID=54.5%  OK
```

### Self-test failure (0x200)
The sensor reports a self-test malfunction but still produces plausible readings. Research shows:
- Code `0x4345` → dead sensor, no readings ([SparkFun](https://community.sparkfun.com/t/scd40-fails-self-test-and-does-not-give-readings/66100))
- Code `196` → defective unit, needed replacement ([Sensirion GitHub #15](https://github.com/Sensirion/arduino-i2c-scd4x/issues/15))
- Code `0x200` (ours) → sensor works despite failure. Monitor for degradation.

### Root cause analysis
The previous session's debug loop had an I2C scan running every iteration which may have been interfering with the bus during measurement windows. The standalone sketch only scans once at boot. The sensor needs uninterrupted I2C access during its 5-second measurement cycle.

### Fix applied to main sketch
Added `scd.reinit()` + delay between `stopPeriodicMeasurement()` and `startPeriodicMeasurement()` in `koffing.ino`. This clears any stale sensor state from prior runs (the SCD4x retains measurement state across ESP32 reboots since it has its own power rail).

### Key SCD4x gotchas
1. **Stop before config** — The SCD4x ignores almost all I2C commands while periodic measurement is active. Must call `stopPeriodicMeasurement()` before any config/init commands.
2. **Survives MCU reboot** — If the ESP32 resets but the SCD4x stays powered, it's still in periodic mode on next boot. The stop→reinit→start sequence handles this.
3. **205 mA peak draw** — Use 5V via STEMMA QT (Adafruit breakout has onboard regulator), not the Nano's 3.3V pin.
4. **5-second measurement cycle** — `getDataReadyStatus()` returns true once per cycle. Don't poll too aggressively or interrupt with heavy I2C traffic mid-cycle.
5. **Self-test takes ~10 seconds** — Don't run in normal operation, only for diagnostics.

---

## Session 1 — 2026-03-20: No data

### Summary
The SCD4x communicated over I2C at 0x62 but never produced measurement data. All other sensors (PMSA003I, SGP40, OLED) worked correctly.

### What worked
- I2C scan found device at 0x62
- `wakeUp()`, `stopPeriodicMeasurement()`, `reinit()`, `startPeriodicMeasurement()` all returned error 0
- `getSerialNumber()` returned valid serial
- `getDataReadyStatus()` returned error 0

### What didn't work
- `getDataReadyStatusRaw()` always returned `0x0000` (or briefly `0x8000` after factory reset)
- Data never became ready, even after 10+ minutes
- `performSelfTest()` failed with error 527 = `ReadError | NotEnoughDataError`

### What was tried
1. Standard init sequence: wakeUp → stop → reinit → startPeriodic — no data
2. Removed I2C scan from loop (was interfering with bus) — no change
3. Polling loop with retries — no change
4. Wire timeout 200ms, clock 50kHz — no change
5. Isolated SCD4x alone on I2C bus — no change
6. Factory reset — returned 0 but no change
7. Without reinit — no change
8. Ran 10+ minutes continuously — never produced data

### Likely explanation (in retrospect)
The debug loop from session 1 included an I2C bus scan on every iteration, which likely disrupted the SCD4x measurement cycle. The sensor needs clean I2C bus time to complete its photoacoustic measurement. Session 2's standalone sketch only scanned once at boot, giving the sensor uninterrupted bus access.
