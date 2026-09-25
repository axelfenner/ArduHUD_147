#include "hud_draw.h"
#include "config.h"
#include <math.h>

static const float PPD = 2.0f; // pixeles por grado de pitch (escala del horizonte)
static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

// ---------------------------------------------------------------- Horizonte artificial
static void drawHorizon(Arduino_GFX *gfx, float rollRad, float pitchRad) {
  float angle = -rollRad;
  float cosA = cosf(angle), sinA = sinf(angle);
  float dirX = cosA, dirY = sinA;     // a lo largo del horizonte
  float perpX = -sinA, perpY = cosA;  // hacia el "suelo" cuando roll=0

  float pitchDeg = pitchRad * 180.0f / PI;

  // Fondo cielo
  gfx->fillScreen(RGB565_SKYBLUE);

  // Relleno de "suelo" como rectangulo grande rotado, usando la marca 0 (horizonte)
  float v0 = pitchDeg * PPD;
  float hxr = CX + perpX * v0, hyr = CY + perpY * v0;
  float HL = 260, D = 300;
  float ax = hxr - dirX * HL, ay = hyr - dirY * HL;
  float bx = hxr + dirX * HL, by = hyr + dirY * HL;
  float cx2 = bx + perpX * D, cy2 = by + perpY * D;
  float dx2 = ax + perpX * D, dy2 = ay + perpY * D;
  gfx->fillTriangle((int16_t)ax, (int16_t)ay, (int16_t)bx, (int16_t)by, (int16_t)cx2, (int16_t)cy2, RGB565_SIENNA);
  gfx->fillTriangle((int16_t)ax, (int16_t)ay, (int16_t)cx2, (int16_t)cy2, (int16_t)dx2, (int16_t)dy2, RGB565_SIENNA);

  // Linea de horizonte (mas gruesa)
  float hl2 = 150;
  int16_t p1x = (int16_t)(hxr - dirX * hl2), p1y = (int16_t)(hyr - dirY * hl2);
  int16_t p2x = (int16_t)(hxr + dirX * hl2), p2y = (int16_t)(hyr + dirY * hl2);
  gfx->drawLine(p1x, p1y, p2x, p2y, RGB565_WHITE);
  gfx->drawLine((int16_t)(p1x + perpX), (int16_t)(p1y + perpY), (int16_t)(p2x + perpX), (int16_t)(p2y + perpY), RGB565_WHITE);

  // Escala de cabeceo (pitch ladder) cada 10 grados
  const int marks[] = {-30, -20, -10, 10, 20, 30};
  for (int i = 0; i < 6; i++) {
    int m = marks[i];
    float half = (abs(m) == 30) ? 34 : 20;
    float v = (pitchDeg - m) * PPD;
    float mx = CX + perpX * v, my = CY + perpY * v;
    int16_t l1x = (int16_t)(mx - dirX * half), l1y = (int16_t)(my - dirY * half);
    int16_t l2x = (int16_t)(mx + dirX * half), l2y = (int16_t)(my + dirY * half);
    gfx->drawLine(l1x, l1y, l2x, l2y, RGB565_WHITE);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);
    gfx->setCursor(l2x + 3, l2y - 4);
    gfx->print(abs(m));
  }

  // Indicador de alabeo (roll) - arco de marcas + puntero fijo arriba
  const int rollMarks[] = {-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60};
  int R = 78;
  for (int i = 0; i < 11; i++) {
    int rv = rollMarks[i];
    bool major = (rv == 0 || rv == 30 || rv == 60 || rv == -30 || rv == -60);
    float thetaDeg = -90.0f + rv - (rollRad * 180.0f / PI);
    float thetaRad = thetaDeg * PI / 180.0f;
    float ct = cosf(thetaRad), st = sinf(thetaRad);
    int innerR = major ? (R - 12) : (R - 7);
    int16_t ox = (int16_t)(CX + R * ct), oy = (int16_t)(CY + R * st);
    int16_t ix = (int16_t)(CX + innerR * ct), iy = (int16_t)(CY + innerR * st);
    gfx->drawLine(ox, oy, ix, iy, RGB565_YELLOW);
  }
  // Puntero fijo (triangulo apuntando hacia el pivote, en la parte superior)
  gfx->fillTriangle(CX, CY - R + 14, CX - 5, CY - R + 4, CX + 5, CY - R + 4, RGB565_YELLOW);

  // Simbolo de avion fijo (alas + punto central), no rota
  gfx->drawLine(CX - 32, CY, CX - 10, CY, RGB565_YELLOW);
  gfx->drawLine(CX - 10, CY, CX - 10, CY + 5, RGB565_YELLOW);
  gfx->drawLine(CX + 32, CY, CX + 10, CY, RGB565_YELLOW);
  gfx->drawLine(CX + 10, CY, CX + 10, CY + 5, RGB565_YELLOW);
  gfx->fillCircle(CX, CY, 2, RGB565_YELLOW);
}

// ---------------------------------------------------------------- Cajas de datos (velocidad / altitud)
static void drawDataBox(Arduino_GFX *gfx, int x, int y, int w, int h, const char *label, float value, int decimals, uint16_t color) {
  gfx->fillRect(x, y, w, h, RGB565_BLACK);
  gfx->drawRect(x, y, w, h, RGB565_DARKGREY);
  gfx->setTextColor(RGB565_LIGHTGREY);
  gfx->setTextSize(1);
  gfx->setCursor(x + 3, y + 2);
  gfx->print(label);
  gfx->setTextColor(color);
  gfx->setTextSize(2);
  gfx->setCursor(x + 3, y + 12);
  gfx->print(value, decimals);
}

static void drawTopBar(Arduino_GFX *gfx, bool linkOk) {
  gfx->fillRect(0, 0, SCREEN_W, 12, RGB565_BLACK);
  gfx->setTextColor(RGB565_WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(2, 2);
  gfx->print("HDG ");
  gfx->print(mav.heading);

  gfx->setCursor(SCREEN_W - 60, 2);
  gfx->setTextColor(linkOk ? RGB565_GREEN : RGB565_RED);
  gfx->print(linkOk ? "LINK OK" : "NO LINK");
}

static void drawBottomBar(Arduino_GFX *gfx) {
  gfx->fillRect(0, SCREEN_H - 11, SCREEN_W, 11, RGB565_BLACK);
  gfx->setTextColor(mav.armed ? RGB565_RED : RGB565_GREEN);
  gfx->setTextSize(1);
  gfx->setCursor(2, SCREEN_H - 9);
  gfx->print(mav.armed ? "ARMED" : "DISARMED");

  gfx->setTextColor(RGB565_CYAN);
  gfx->setCursor(70, SCREEN_H - 9);
  gfx->print(planeModeName(mav.customMode));

  gfx->setTextColor(mav.batteryVoltage < 10.5f && mav.batteryVoltage > 0 ? RGB565_RED : RGB565_WHITE);
  gfx->setCursor(160, SCREEN_H - 9);
  gfx->print(mav.batteryVoltage, 1);
  gfx->print("V ");
  if (mav.batteryRemaining >= 0) { gfx->print((int)mav.batteryRemaining); gfx->print("%"); }

  gfx->setTextColor(mav.gpsFixType >= 3 ? RGB565_GREEN : RGB565_ORANGE);
  gfx->setCursor(250, SCREEN_H - 9);
  gfx->print("GPS ");
  gfx->print(mav.gpsSatellites);
}

void hudDrawFrame(Arduino_GFX *gfx, HudMode mode, bool linkOk) {
  drawHorizon(gfx, mav.roll, mav.pitch);

  if (mode == MODE_A) {
    drawTopBar(gfx, linkOk);
    return;
  }

  // Modo B y C: velocidad (izq) y altitud (der)
  drawDataBox(gfx, 2, 20, 60, 26, "IAS m/s", mav.airspeed, 1, RGB565_WHITE);
  drawDataBox(gfx, SCREEN_W - 62, 20, 60, 26, "ALT m", mav.alt, 0, RGB565_WHITE);
  drawDataBox(gfx, 2, SCREEN_H - 36, 60, 24, "GND m/s", mav.groundspeed, 1, RGB565_WHITE);
  drawDataBox(gfx, SCREEN_W - 62, SCREEN_H - 36, 60, 24, "VS m/s", mav.climb, 1, mav.climb >= 0 ? RGB565_GREEN : RGB565_RED);

  drawTopBar(gfx, linkOk);

  if (mode == MODE_C) {
    drawBottomBar(gfx);
  }
}
