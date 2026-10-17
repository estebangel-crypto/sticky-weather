# ☀️ Sticky Weather

[logo](docs/logo.png)

**reTerminal Sticky → Estación meteorológica e-paper minimalista**

Firmware para Seeed reTerminal Sticky (ESP32-S3) que lo convierte en una estación del tiempo con pantalla e-paper. Ultra-bajo consumo y flasheo 100% desde el navegador.

## ✨ Características

- 🌤️ Tiempo actual + 3 días (Open-Meteo API)
- 🌡️ Temperatura interior con sensor SHT40
- 🔋 60min deep-sleep, WiFi solo si hace falta (ahorro 18% batería)
- 🌙 Icono luna al entrar en sueño
- 💻 Flasheo directo desde Chrome/Edge sin instalar nada
- 🎨 Logo Sticky Weather optimizado para e-paper

## 🚀 Flashear ahora

**👉 https://estebangel-crypto.github.io/sticky-weather/**

1. Conecta el Sticky por USB-C (mantén BOOT si hace falta)
2. Pulsa INSTALL y elige el puerto COM
3. Espera al 100% ¡Listo!

## 📄 Documentación

- [📘 Manual ES - PDF](docs/STICKY-WEATHER-MANUAL-ES.pdf)
- [📝 Manual ES - MD](docs/MANUAL-ES.md)

## 🛠️ Hardware

- Seeed reTerminal Sticky (ESP32-S3)
- Pantalla e-paper 480x800
- Sensor SHT40 (temperatura y humedad interior)
- Batería LiPo

## 📦 Repo

- `docs/` → Web flasher + manuales + logos + firmware.bin
- `src/` → Código fuente Arduino

## 👨‍💻 Autor

Hecho en La Rioja con ♥ por **Angel Esteban**
- GitHub: @estebangel-crypto
- Web: https://estebangel-crypto.github.io/sticky-weather/
