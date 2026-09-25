#pragma once
// =====================================================================================
// config.h - Pines de hardware y ajustes de usuario
// Placa: Waveshare ESP32-S3-Touch-LCD-1.47 (ESP32-S3R8, panel JD9853 172x320, touch AXS5106L)
// =====================================================================================

// ---------------------------------------------------------------- Placa: Arduino IDE
// Board: "ESP32S3 Dev Module"
// USB CDC On Boot: Enabled
// PSRAM: OPI PSRAM
// Flash Size: 16MB
// Partition Scheme: Default (o "16M Flash (3MB APP/9.9MB FATFS)")

// ---------------------------------------------------------------- LCD (SPI, JD9853 compatible con ST7789)
#define LCD_PIN_DC    45
#define LCD_PIN_CS    21
#define LCD_PIN_SCK   38
#define LCD_PIN_MOSI  39
#define LCD_PIN_RST   40
#define LCD_PIN_BL    46

#define LCD_NATIVE_W  172
#define LCD_NATIVE_H  320
#define LCD_COL_OFFSET 34
#define LCD_ROW_OFFSET 0

#define SCREEN_W      320   // salida en landscape
#define SCREEN_H      172

// ---------------------------------------------------------------- Touch (I2C, AXS5106L)
#define TOUCH_PIN_SDA 42
#define TOUCH_PIN_SCL 41
#define TOUCH_PIN_RST 47
#define TOUCH_PIN_INT 48

// ---------------------------------------------------------------- Enlace MAVLink (UART directo al TELEM del FC)
// GPIO43/44 son el UART0 "libre" de este módulo (independiente del USB-CDC nativo en GPIO19/20).
// !! Verifica en tu unidad que estos pines no estén ocupados por otra cosa antes de soldar cables.
// Cablea SOLO: FC-TELEM-TX -> ESP32 GPIO44 (RX). GND <-> GND obligatorio.
// FC-TELEM-RX -> ESP32 GPIO43 (TX) es opcional (solo si quieres enviar HEARTBEAT/comandos).
// IMPORTANTE: la mayoría de puertos TELEM de ArduPilot son 3.3V. Si tu placa usa 5V en el TX,
// usa un divisor de tensión o un conversor de nivel antes de conectarlo al ESP32.
#define MAV_RX_PIN    44
#define MAV_TX_PIN    43
#define MAV_BAUD      57600   // debe coincidir con SERIALx_BAUD del FC (57600 -> valor de parametro "57")
#define MAV_SEND_HEARTBEAT 1  // 1 = el ESP32 también envía su propio HEARTBEAT cada 1s (opcional, recomendado)

// Timeout de enlace: si no llega ningún mensaje en este tiempo, se marca "LINK LOST"
#define MAV_LINK_TIMEOUT_MS 3000

// ---------------------------------------------------------------- Preferencias de UI
#define UI_BACKLIGHT_DEFAULT 200   // 0-255
#define UI_FPS_TARGET         20   // refresco del HUD
#define UI_SWIPE_THRESHOLD_PX 40   // distancia mínima para considerar "swipe"
