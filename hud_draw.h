#pragma once
// =====================================================================================
// hud_draw.h - Dibuja el horizonte artificial y los 3 modos de HUD (A/B/C)
// =====================================================================================
#include <Arduino_GFX_Library.h>
#include "mavlink_mini.h"

enum HudMode { MODE_A = 0, MODE_B = 1, MODE_C = 2, MODE_COUNT = 3 };

void hudDrawFrame(Arduino_GFX *gfx, HudMode mode, bool linkOk);
