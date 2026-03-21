# OLED Display — SSD1306 128x64

## Basics
- **I2C Address:** 0x3C (most common for these cheap displays; try 0x3D if 0x3C fails)
- **Resolution:** 128 × 64 pixels
- **Library:** `Adafruit SSD1306` + `Adafruit GFX Library`

## API
```cpp
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W 128
#define SCREEN_H 64
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1); // -1 = no reset pin

display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
display.clearDisplay();
display.setTextSize(1);              // 6x8 px per char at size 1
display.setTextColor(SSD1306_WHITE);
display.setCursor(0, 0);
display.println("Hello");
display.display();                   // push buffer to screen
```

## Key Methods
| Method | Description |
|--------|-------------|
| `display()` | **Required** — pushes RAM buffer to hardware |
| `clearDisplay()` | Clears buffer (not screen until `display()`) |
| `setCursor(x, y)` | Set text position |
| `setTextSize(n)` | Character scale (1 = 6×8 px) |
| `setTextColor(c)` / `setTextColor(c, bg)` | WHITE, BLACK, INVERSE |
| `print()` / `println()` | Print text (inherits from Print) |
| `drawBitmap(x, y, bmp, w, h, color)` | Draw bitmap array |
| `setContrast(level)` | Brightness 0–255 |
| `invertDisplay(bool)` | Invert all pixels |

## Layout at TextSize 1
- 21 chars × 8 lines on 128×64
- Useful for dense sensor readouts

## Wiring
```
OLED    →  Nano ESP32
GND     →  GND
VCC     →  3V3
SCL     →  A5
SDA     →  A4
```

## Quirks
- All drawing stays in RAM until `display()` is called
- Splash screen shown on init — suppress with `#define SSD1306_NO_SPLASH` before include
- Uses ~1KB RAM for frame buffer (128×64 / 8)
- Shares I2C bus with all STEMMA QT sensors — no address conflicts
