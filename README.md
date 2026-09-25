# ArduHUD_147 — HUD con horizonte artificial (ESP32-S3-Touch-LCD-1.47 + ArduPilot/ArduPlane)

Sketch para Arduino IDE que convierte una placa **Waveshare ESP32-S3-Touch-LCD-1.47**
en un HUD de cabina con horizonte artificial, leyendo telemetría **MAVLink** directamente
por UART desde un puerto **TELEM** del flight controller (ArduPilot / ArduPlane).

## Requisitos

- Arduino IDE 2.x
- Paquete de placas **esp32 by Espressif Systems** (probado con 3.3.11)
- Librería (Library Manager): **"GFX Library for Arduino"** (moononournation) — es la única
  dependencia externa. El resto (parser MAVLink, driver táctil, init de panel) está incluido
  en el propio sketch para no depender de librerías pesadas.

### Configuración de la placa en el IDE

| Opción            | Valor              |
|--------------------|--------------------|
| Board              | ESP32S3 Dev Module |
| USB CDC On Boot    | Enabled            |
| PSRAM              | OPI PSRAM          |
| Flash Size         | 16MB (o el de tu placa) |
| Partition Scheme   | Default (con OTA o similar, 16MB) |

## Conexionado (UART directo al TELEM del FC)

| ESP32-S3 (HUD)     | FC (puerto TELEM, ej. TELEM2) |
|---------------------|--------------------------------|
| GPIO44 (RX)        | TX del FC                     |
| GPIO43 (TX, opcional) | RX del FC                   |
| GND                | GND (¡común obligatorio!)      |

> ⚠️ **Niveles de 3.3V**: el ESP32-S3 es 3.3V. La mayoría de FCs (Pixhawk, etc.) ya usan
> TELEM a 3.3V TTL, pero verifica el datasheet de tu FC antes de conectar — 5V dañaría el ESP32.
>
> ⚠️ **Pines GPIO43/44**: son el UART0 por defecto del ESP32-S3, libres de conflicto con LCD,
> táctil, SD y batería en esta placa, pero **confirma que estén realmente accesibles en tu
> revisión física de la placa** (continuidad/serigrafía) antes de soldar.

El TX del HUD (GPIO43) es opcional: solo se usa para enviar un HEARTBEAT periódico
(buena práctica MAVLink), pero ArduPilot ya transmite telemetría en TELEM sin necesidad
de handshake previo.

## Configuración en ArduPilot (Mission Planner / parámetros)

En el puerto TELEM que uses (ejemplo TELEM2 = `SERIAL2`):

```
SERIAL2_PROTOCOL = 2      ; MAVLink2
SERIAL2_BAUD     = 57     ; 57600 baudios (o el que uses, debe coincidir con MAV_BAUD en config.h)
```

Las tasas de stream (`SRx_*` / `SR2_*`) por defecto ya incluyen ATTITUDE, VFR_HUD,
GPS_RAW_INT y SYS_STATUS, que es todo lo que este HUD necesita. Si no ves datos,
sube `SR2_EXTRA1` (ATTITUDE), `SR2_EXTRA2` (VFR_HUD) y `SR2_POSITION`/`SR2_EXT_STAT` (GPS/estado).

## Modos de pantalla (deslizar el dedo para cambiar)

- **A (mínimo)**: solo horizonte artificial, rumbo y estado del enlace.
- **B**: A + velocidad indicada/tierra, altitud y variómetro.
- **C**: B + batería, GPS, modo de vuelo y estado ARMED/DISARMED.

Desliza horizontalmente sobre la pantalla:
- Izquierda → siguiente modo (A→B→C→A)
- Derecha → modo anterior

El modo elegido se guarda en NVS (`Preferences`) y se restaura automáticamente al reiniciar.

## Estructura del sketch

- `config.h` — pines y parámetros (LCD, táctil, UART MAVLink, UI).
- `jd9853_init.h` — secuencia de registros específica del panel JD9853 (colores/gamma correctos).
- `touch_axs5106l.h/.cpp` — driver mínimo por I2C para el controlador táctil AXS5106L.
- `mavlink_mini.h/.cpp` — parser MAVLink v1/v2 propio y ligero (HEARTBEAT, SYS_STATUS,
  GPS_RAW_INT, ATTITUDE, VFR_HUD), con CRC-16 verificado contra la especificación oficial.
- `hud_draw.h/.cpp` — dibujo del horizonte artificial, escala de cabeceo, indicador de
  alabeo y los 3 layouts de datos.
- `ArduHUD_147.ino` — inicialización de pantalla/táctil/UART y bucle principal.

## Notas / solución de problemas

- Si la imagen sale rotada o espejada, prueba cambiar el parámetro `rotation` (0–3) del
  constructor `Arduino_Canvas` en el `.ino`.
- Si no llegan datos ("NO LINK" en la barra superior), revisa baudios, cableado TX/RX
  cruzado y los parámetros `SERIALx_PROTOCOL`/`SERIALx_BAUD`/`SRx_*` en ArduPilot.
- Compilado y verificado con `arduino-cli` (esp32 core 3.3.11) sin errores ni warnings.
