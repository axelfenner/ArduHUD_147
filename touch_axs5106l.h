#pragma once
// =====================================================================================
// touch_axs5106l.h - Driver mínimo por I2C para el controlador táctil AXS5106L
// (usado por Waveshare ESP32-S3-Touch-LCD-1.47). No depende de ESP-IDF, solo Wire.
// =====================================================================================
#include <Arduino.h>
#include <Wire.h>

#define AXS5106L_ADDR          0x63
#define AXS5106L_ID_REG        0x08
#define AXS5106L_TOUCH_DATA_REG 0x01

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  bool touched;
};

// rotation: 1 = landscape (320x172), coincide con la orientación usada en este proyecto.
void touchInit(TwoWire *wire, int pinRst, int pinInt, uint8_t rotation);

// Debe llamarse cada loop(); actualiza el estado interno leyendo por interrupción.
void touchPoll();

// Devuelve el punto de contacto más reciente (válido solo si touched == true).
TouchPoint touchGetPoint();
