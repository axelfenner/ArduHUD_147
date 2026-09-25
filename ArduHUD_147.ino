// =====================================================================================
// ArduHUD_147.ino
// HUD con horizonte artificial para avion RC (ArduPilot/ArduPlane) via MAVLink,
// para la placa Waveshare ESP32-S3-Touch-LCD-1.47.
//
// - 3 modos de pantalla (A: minimal, B: + velocidad/altitud, C: + estado completo)
//   Se cambian deslizando el dedo horizontalmente (swipe) sobre la pantalla.
// - El modo elegido se guarda en NVS (Preferences) y se restaura al reiniciar.
// - Los datos de vuelo llegan por MAVLink desde el puerto TELEM del FC (UART directo).
//
// Librerias requeridas (Arduino Library Manager):
//   - "GFX Library for Arduino" (moononournation)
// Board: "ESP32S3 Dev Module" | USB CDC On Boot: Enabled | PSRAM: OPI PSRAM
// =====================================================================================
#include <Arduino_GFX_Library.h>
#include <Preferences.h>

#include "config.h"
#include "jd9853_init.h"
#include "touch_axs5106l.h"
#include "mavlink_mini.h"
#include "hud_draw.h"

// El panel real se inicializa y rota ANTES de envolverlo en un Canvas (framebuffer en RAM):
// esto evita "tearing"/artefactos, ya que dibujamos todo en RAM y lo volcamos a la pantalla
// de una sola vez con flush() en vez de escribir pixel a pixel en vivo sobre el LCD.
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_PIN_DC, LCD_PIN_CS, LCD_PIN_SCK, LCD_PIN_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *panel = new Arduino_ST7789(bus, LCD_PIN_RST, 0 /* rotation */, false /* IPS */,
                                        LCD_NATIVE_W, LCD_NATIVE_H,
                                        LCD_COL_OFFSET, LCD_ROW_OFFSET, LCD_COL_OFFSET, LCD_ROW_OFFSET);
Arduino_Canvas *gfx = new Arduino_Canvas(SCREEN_W, SCREEN_H, panel, 0, 0, 0 /* el panel ya rota, el canvas no */);

HardwareSerial MavSerial(1);
Preferences prefs;

HudMode currentMode = MODE_A;
static bool touchWasDown = false;
static int touchStartX = 0;

static void loadMode() {
  prefs.begin("hud", true);
  int m = prefs.getInt("mode", 0);
  prefs.end();
  if (m < 0 || m >= MODE_COUNT) m = 0;
  currentMode = (HudMode)m;
}

static void saveMode() {
  prefs.begin("hud", false);
  prefs.putInt("mode", (int)currentMode);
  prefs.end();
}

static void initDisplay() {
  panel->begin();                // reset por hardware (via LCD_PIN_RST) + init base ST7789
  jd9853_reg_init(bus);          // secuencia correctiva especifica del panel JD9853 (deja MADCTL en portrait)
  panel->setRotation(1);         // ahora si, rota el PANEL REAL a landscape 320x172

  gfx->begin(GFX_SKIP_OUTPUT_BEGIN); // solo reserva el framebuffer en RAM, no vuelve a tocar el panel

  pinMode(LCD_PIN_BL, OUTPUT);
  analogWrite(LCD_PIN_BL, UI_BACKLIGHT_DEFAULT);
}

static void handleTouchSwipe() {
  touchPoll();
  TouchPoint tp = touchGetPoint();

  if (tp.touched && !touchWasDown) {
    touchStartX = tp.x;
    touchWasDown = true;
  } else if (!tp.touched && touchWasDown) {
    touchWasDown = false;
    // usamos la ultima posicion conocida como aproximacion del punto de soltado
    int dx = (int)tp.x - touchStartX;
    if (dx == 0) return; // sin datos utiles, evitar falso positivo
    if (abs(dx) >= UI_SWIPE_THRESHOLD_PX) {
      int m = (int)currentMode;
      if (dx < 0) m = (m + 1) % MODE_COUNT;       // deslizar a la izquierda -> siguiente modo
      else        m = (m - 1 + MODE_COUNT) % MODE_COUNT; // deslizar a la derecha -> modo anterior
      currentMode = (HudMode)m;
      saveMode();
    }
  }
}

void setup() {
  Serial.begin(115200);

  loadMode();

  initDisplay();

  Wire.begin(TOUCH_PIN_SDA, TOUCH_PIN_SCL);
  touchInit(&Wire, TOUCH_PIN_RST, TOUCH_PIN_INT, 1);

  MavSerial.begin(MAV_BAUD, SERIAL_8N1, MAV_RX_PIN, MAV_TX_PIN);
  mavlinkInit(&MavSerial);
}

void loop() {
  handleTouchSwipe();
  mavlinkUpdate();

  static uint32_t lastFrame = 0;
  uint32_t frameInterval = 1000 / UI_FPS_TARGET;
  if (millis() - lastFrame >= frameInterval) {
    lastFrame = millis();
    hudDrawFrame(gfx, currentMode, mavlinkLinkOk());
    gfx->flush();
  }
}
