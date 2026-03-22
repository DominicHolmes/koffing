# Wiring Diagram

## I2C Bus (STEMMA QT chain + OLED)

```
                          STEMMA QT daisy chain
Arduino Nano ESP32 ──QT──→ PMSA003I ──QT──→ SGP40 ──QT──→ SCD4x
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

STEMMA QT order doesn't matter — I2C is a shared bus. The OLED connects via jumper wires to the same SDA/SCL/GND/3V3 pins.

## I2C Address Map
| Address | Device |
|---------|--------|
| 0x12 | PMSA003I |
| 0x3C | OLED SSD1306 |
| 0x59 | SGP40 |
| 0x62 | SCD4x |

No conflicts.

## MiCS5524 (analog)
```
MiCS5524    →  Nano ESP32
Ao          →  A0
En!         →  GND (always on)
GND         →  GND
5V          →  VUSB (5V USB power)
```

## VBUS Voltage Divider (A1)
```
VUSB ─── 10kΩ ──┬── 10kΩ ─── GND
                 │
                 A1
```
Two 10kΩ resistors divide the ~5V USB rail to ~2.5V for the ESP32's 3.3V-max ADC. Code scales the reading back up by 2x. Displayed on OLED bottom-left; goes inverse below 4.75V to flag a weak adapter or cable. Also published as `vbus` over MQTT.
