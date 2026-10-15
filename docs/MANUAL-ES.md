# STICKY WEATHER - Documentación Técnica

[Logo](logo.png)

**Versión:** V57 - 60 Minutos + Batería Optimizada  
**Hardware:** Seeed Studio reTerminal Sticky (ESP32-S3 16MB)  
**Autor:** Angel Esteban - La Rioja  
**Web Flasher:** https://estebangel-crypto.github.io/sticky-weather/

---

## 1. Descripción General

Sticky Weather es un firmware para el reTerminal Sticky que convierte el dispositivo en una estación meteorológica minimalista de tinta electrónica.

Muestra:
- Temperatura actual, sensación térmica, humedad y viento
- Previsión de 4 días con iconos (Sol, Nublado, Lluvia, Nieve, Tormenta, Niebla)
- Temperatura interior y humedad del sensor SHT40 interno
- Hora, fecha y porcentaje de batería
- Icono de luna cuando entra en reposo

La clave del proyecto es la **ultra-baja autonomía**: duerme en deep-sleep y solo despierta cada 60 minutos para actualizar datos.

---

## 2. Funcionamiento (V57)

### 2.1 Ciclo de Vida

1. **Arranque / Wake-up:** El ESP32 despierta por 2 motivos:
   - **Timer:** Cada 60 minutos (INTERVALO_MIN)
   - **Botón:** Pulsación física (EXT1 wakeup)

2. **Decisión de datos:** 
   - Si han pasado <60 min desde último update web Y no es pulsación larga de UP, **reutiliza datos de RTC** (ahorra batería, no enciende WiFi)
   - Si han pasado >60 min o es pulsación larga, conecta WiFi.

3. **Conexión WiFi + NTP:**
   - Conecta a SSID guardado en Preferences ("tiempo")
   - Sincroniza hora vía pool.ntp.org con zona horaria CET-1CEST
   - Obtiene meteorología vía Open-Meteo / API configurada (obtenerTiempo())

4. **Lectura de sensores:**
   - **Batería:** I2C addr 0x55 registro 0x2C (SOC) y 0x08 (voltaje). Solo se lee cuando hay update WiFi o pulsación de botón. NO cada 5 segundos (optimización V57 que redujo consumo del 18% en 15h)
   - **SHT40:** Temp/humedad interior por Wire1 SDA=1 SCL=0

5. **Dibujado:**
   - GFX: Seeed_GFX en e-paper 296x128 aprox.
   - Función dibujar(): icono grande central (iconoDe/wcode), temperaturas, barra inferior con 4 días, engranaje si no hay config, batería dibujada.

6. **Loop despierto 3 minutos:**
   - TIEMPO_DESPTERTA_MIN = 3. Se queda despierto 3 min para interacción
   - UP corto: refresca pantalla + batería + SHT40
   - UP largo (>1500ms): Fuerza actualización WiFi + tiempo + batería
   - DOWN largo: Entra en modo configuración (portal)
   - AI: Refresca batería y dibuja
   - NO chequea batería cada 5s (eliminado en V57)

7. **Dormir:**
   - Muestra icono luna (iconoLunaVisible=true)
   - display.powerOff(), EPD_PWR LOW
   - esp_sleep_enable_ext1_wakeup(UP/DOWN/AI) + timer 60 min
   - esp_deep_sleep_start()

### 2.2 Gestión de Batería

- Umbral bajo: 10% (UMBRAL_BATERIA_BAJA)
- Si <10% y no se ha mostrado aviso, muestra pantallaBateriaBaja() y duerme 60 min
- Umbral recuperación: 15% para resetear aviso
- Aviso guardado en RTC_DATA_ATTR rtc_avisoBateriaMostrado para persistir en deep-sleep

### 2.3 Portal de Configuración

Si no hay configuración válida o DOWN al arrancar:
- Crea AP "Sticky-Tiempo"
- WebServer 80 + DNSServer portal cautivo
- Páginas: / (root), /redes (scan), /wifi (conectar), /ciudades (geocoding), /guardar
- Guarda en Preferences: ssid, pass, ciudad, lat, lon, lang, uni, fmt
- Time portal: 15 minutos, luego duerme

### 2.4 Hardware y Pines

- PIN_PWR_HOLD 45, PIN_PWR_LOCK 46, PIN_EPD_PWR 47, PIN_TOUCH_PWR 42 -> stickyPowerLatch()
- BTN_AI 4, BTN_UP 5, BTN_DOWN 6 (INPUT_PULLUP)
- SDA 1, SCL 0 (Wire1 para sensor y fuel gauge)

### 2.5 Optimizaciones V57 vs anteriores

- INTERVALO_DATOS_MIN 30->60 min (mitad de conexiones WiFi)
- Batería solo en update, no en cada loop de 5s (ahorro 18% en 15h medido)
- Uso RTC_DATA_ATTR para ultimoUpdateWeb y datosValidosRTC para no perder datos en deep-sleep

---

## 3. Flasheo Web

- Web flasher en GitHub Pages /docs con esp-web-tools 10
- manifest.json apunta a firmware.bin
- Compatible Chrome/Edge con WebSerial

## 4. Instalación

1. Ir a https://estebangel-crypto.github.io/sticky-weather/
2. Conectar Sticky por USB-C (mantener BOOT si no entra en download)
3. INSTALL -> elegir puerto COM -> Esperar 100%

---

## 5. Uso

- **UP corto:** Refresco
- **UP largo:** Actualizar tiempo ahora
- **DOWN largo:** Reconfigurar WiFi/ciudad

Hecho en La Rioja con ♥
