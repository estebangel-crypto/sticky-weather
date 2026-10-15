# STICKY WEATHER - Technical Documentation

[Logo](logo.png)

**Version:** V57 - 60 Minutes + Optimized Battery  
**Hardware:** Seeed Studio reTerminal Sticky (ESP32-S3 16MB)  
**Author:** Angel Esteban - La Rioja, Spain  
**Web Flasher:** https://estebangel-crypto.github.io/sticky-weather/

---

## 1. Overview

Sticky Weather is a firmware for the reTerminal Sticky that turns it into a minimalist e-ink weather station.

It displays:
- Current temp, feels-like, humidity, wind
- 4-day forecast with icons (Sun, Partly Cloudy, Cloudy, Rain, Snow, Storm, Fog)
- Indoor temp/humidity from internal SHT40
- Time, date and battery percentage
- Moon icon when going to sleep

Key design: **ultra-low power** - deep-sleep and wake only every 60 minutes.

---

## 2. How it Works (V57)

### 2.1 Lifecycle

1. **Wake-up:** ESP32 wakes for:
   - **Timer:** Every 60 min (INTERVALO_MIN)
   - **Button:** Physical press (EXT1 wakeup)

2. **Data decision:**
   - If <60 min since last web update AND not long UP press, **reuse RTC data** (saves battery, no WiFi)
   - If >60 min or long press, connect WiFi.

3. **WiFi + NTP:**
   - Connects to SSID stored in Preferences ("tiempo")
   - Time sync via pool.ntp.org with CET-1CEST
   - Fetches weather via Open-Meteo / configured API (obtenerTiempo())

4. **Sensor reading:**
   - **Battery:** I2C addr 0x55 reg 0x2C (SOC) and 0x08 (voltage). Only read when there is WiFi update or button press. NOT every 5 seconds (V57 optimization that saved 18% in 15h)
   - **SHT40:** Indoor temp/humidity via Wire1 SDA=1 SCL=0

5. **Drawing:**
   - GFX: Seeed_GFX on e-paper ~296x128
   - dibujar(): large central icon, temps, bottom bar with 4 days, gear if not configured, battery widget.

6. **Awake loop 3 minutes:**
   - TIEMPO_DESPTERTA_MIN = 3. Stays awake 3 min for interaction
   - UP short: refresh screen + battery + SHT40
   - UP long (>1500ms): Force WiFi + weather + battery refresh
   - DOWN long: Enter config mode (portal)
   - AI: Refresh battery and draw
   - No battery check every 5s (removed in V57)

7. **Sleep:**
   - Shows moon icon
   - display.powerOff(), EPD_PWR LOW
   - esp_sleep_enable_ext1_wakeup + timer 60 min
   - esp_deep_sleep_start()

### 2.2 Battery Management

- Low threshold: 10%
- If <10% and warning not shown, shows pantallaBateriaBaja() and sleeps 60 min
- Recovery threshold: 15% to reset warning
- Warning saved in RTC_DATA_ATTR to persist across deep-sleep

### 2.3 Config Portal

If no valid config or DOWN at boot:
- Creates AP "Sticky-Tiempo"
- WebServer 80 + DNSServer captive portal
- Pages: / , /redes (scan), /wifi, /ciudades (geocoding), /guardar
- Saves to Preferences: ssid, pass, ciudad, lat, lon, lang, uni, fmt
- Portal timeout: 15 min then sleep

### 2.4 Hardware Pins

- PWR_HOLD 45, PWR_LOCK 46, EPD_PWR 47, TOUCH_PWR 42 -> stickyPowerLatch()
- BTN_AI 4, BTN_UP 5, BTN_DOWN 6 (INPUT_PULLUP)
- SDA 1, SCL 0

### 2.5 V57 vs Previous Optimizations

- INTERVAL 30->60 min (half WiFi connections)
- Battery only on update, not every 5s loop (18% saving measured)
- RTC_DATA_ATTR for ultimoUpdateWeb and datosValidosRTC

---

## 3. Web Flasher

- GitHub Pages /docs with esp-web-tools 10
- manifest.json -> firmware.bin
- Chrome/Edge with WebSerial support

## 4. Installation

1. Go to https://estebangel-crypto.github.io/sticky-weather/
2. Connect Sticky via USB-C (hold BOOT if not in download mode)
3. INSTALL -> choose COM port -> Wait 100%

## 5. Usage

- **UP short:** Refresh
- **UP long:** Update weather now
- **DOWN long:** Reconfigure WiFi/city

Made in La Rioja with ♥
