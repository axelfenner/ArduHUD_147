#include "touch_axs5106l.h"

static TwoWire *s_wire = nullptr;
static uint8_t s_rotation = 1;
static volatile bool s_intFlag = false;
static TouchPoint s_last = {0, 0, false};

static void IRAM_ATTR touchIsr() {
  s_intFlag = true;
}

static bool i2cRead(uint8_t reg, uint8_t *data, uint8_t len) {
  s_wire->beginTransmission(AXS5106L_ADDR);
  s_wire->write(reg);
  if (s_wire->endTransmission() != 0) return false;
  s_wire->requestFrom((int)AXS5106L_ADDR, (int)len);
  if (s_wire->available() < len) return false;
  for (uint8_t i = 0; i < len; i++) data[i] = s_wire->read();
  return true;
}

void touchInit(TwoWire *wire, int pinRst, int pinInt, uint8_t rotation) {
  s_wire = wire;
  s_rotation = rotation;

  pinMode(pinRst, OUTPUT);
  digitalWrite(pinRst, LOW);
  delay(200);
  digitalWrite(pinRst, HIGH);
  delay(300);

  pinMode(pinInt, INPUT);
  attachInterrupt(digitalPinToInterrupt(pinInt), touchIsr, FALLING);

  uint8_t id[3] = {0};
  i2cRead(AXS5106L_ID_REG, id, 3); // solo para validar comunicación, no es critico
}

void touchPoll() {
  if (!s_intFlag) return;
  s_intFlag = false;

  uint8_t data[14] = {0};
  if (!i2cRead(AXS5106L_TOUCH_DATA_REG, data, 14)) return;

  uint8_t touchNum = data[1];
  if (touchNum == 0) {
    s_last.touched = false;
    return;
  }

  uint16_t rawX = ((uint16_t)(data[2] & 0x0F) << 8) | data[3];
  uint16_t rawY = ((uint16_t)(data[4] & 0x0F) << 8) | data[5];

  // Rotacion 1: intercambia ejes (coincide con el layout landscape 320x172)
  if (s_rotation == 1) {
    s_last.x = rawY;
    s_last.y = rawX;
  } else {
    s_last.x = rawX;
    s_last.y = rawY;
  }
  s_last.touched = true;
}

TouchPoint touchGetPoint() {
  return s_last;
}
