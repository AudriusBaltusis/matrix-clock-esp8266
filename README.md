# Sunset Clock

ESP8266 + MAX7219 4-in-1 LED matrix WiFi clock.

- Shows time as **HH:MM ss** (large HH:MM, small seconds, blinking colon)
- Time from **NTP**, no RTC needed
- **Automatic DST** (EU rules, Vilnius timezone) or an optional hardware DST switch
- **Auto brightness by sunrise/sunset**, calculated on the device from date and coordinates
- **WiFi setup portal**, so no credentials in the source code

## Hardware

- ESP8266 board (NodeMCU or Wemos D1 mini)
- MAX7219 4-in-1 (32x8) LED matrix module, FC16 type

### Wiring

| MAX7219 | ESP8266 pin | GPIO |
|---------|-------------|------|
| VCC     | 3V3 (or VIN/5V) | n/a |
| GND     | G           | n/a  |
| DIN     | D7          | 13   |
| CLK     | D5          | 14   |
| CS      | D4          | 2    |

DIN and CLK use the hardware SPI pins. CS can be any GPIO (`CS_PIN` in the code).

Optional DST switch: D2 (GPIO4) to GND = summer time (set `USE_DST_SWITCH 1`).

Notes:
- The MAX7219 is rated for 4.0 V minimum. 3.3 V usually works, but if the display flickers or shows random pixels, power VCC from VIN/5V. The 3.3 V data lines normally still work.
- GPIO2 (D4) also drives the onboard LED on most boards, so it may blink with display updates. This is harmless.

## Software setup

### Arduino IDE

1. Add the ESP8266 board URL in File > Preferences > Additional boards manager URLs:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. Boards Manager: install **esp8266 by ESP8266 Community** (3.x).
3. Select board: **NodeMCU 1.0 (ESP-12E Module)** or **LOLIN(WEMOS) D1 R2 & mini**.
4. Library Manager: install
   - **MD_MAX72XX** (MajicDesigns)
   - **WiFiManager** (tzapu)
5. Open the sketch, compile, upload.

## First boot

1. The display shows **W** while waiting for WiFi.
2. Connect your phone or laptop to the **`MatrixClock-Setup`** hotspot (no password).
3. Open `192.168.4.1` if the setup page does not appear automatically.
4. Choose your 2.4 GHz network, enter the password, save.
5. The display shows **N** briefly while NTP syncs, then the clock appears.

Boot status letters: `W` = connecting to WiFi / setup portal, `N` = waiting for NTP.

To forget the saved network: add `wm.resetSettings();` after `WiFiManager wm;`, upload once, configure again, then remove the line and upload again.

## Configuration

All settings are constants at the top of the sketch.

| Constant | Purpose |
|----------|---------|
| `HARDWARE_TYPE` | Module type: `FC16_HW`, `GENERIC_HW`, `PAROLA_HW`, `ICSTATION_HW` |
| `CS_PIN` | Chip select pin |
| `USE_DST_SWITCH` | `0` = automatic EU DST, `1` = manual switch on `DST_PIN` |
| `BRIGHT_DAY` / `BRIGHT_NIGHT` | Brightness 0 to 15 (the scale is not linear; 0 to 3 are dim) |
| `LAT` / `LON` | Coordinates for sunrise/sunset (default: Vilnius) |
| `DIM_EARLY_MIN` | Minutes before sunset to dim, and minutes after sunrise to stay dim |

Timezone is set in `applyTimezone()`. Default is `EET-2EEST,M3.5.0/3,M10.5.0/4` (Vilnius, UTC+2 winter, UTC+3 summer).

## Troubleshooting

- **Garbled, mirrored, or reversed module order:** change `HARDWARE_TYPE`.
- **Time off by an hour:** make sure you are on ESP8266 core 3.x. Add a serial print of `t.tm_isdst` to check.
- **Stuck on W:** use a 2.4 GHz network, check the password, or reset saved settings (see above).
- **Display too bright:** lower `BRIGHT_DAY`. If level 0 is still too bright, replace the module's `RSET` resistor with a higher value (for example 20 to 47 kOhm).
- **Compile errors about missing headers:** install the missing library, select an ESP8266 board, and restart the IDE.

## How the brightness works

Once a minute the sketch computes sunrise and sunset for the current date at `LAT`/`LON` (standard NOAA solar equations, in UTC) and switches between `BRIGHT_DAY` and `BRIGHT_NIGHT`. In Vilnius this means dimming around 15:20 in December and around 22:00 in June, with no fixed night hours to maintain.
