#pragma once
// =====================================================================================
// mavlink_mini.h - Receptor MAVLink v1/v2 minimalista y sin dependencias externas.
// Decodifica solo los mensajes que necesita este HUD:
//   HEARTBEAT(0), SYS_STATUS(1), GPS_RAW_INT(24), ATTITUDE(30), VFR_HUD(74)
// Los CRC_EXTRA y offsets de campos están tomados del dialecto "common" oficial
// (mavlink/c_library_v2), por lo que el checksum se valida igual que con la librería
// completa, pero sin necesidad de instalar ese paquete gigante.
// =====================================================================================
#include <Arduino.h>

struct MavData {
  // ATTITUDE
  float roll = 0, pitch = 0, yaw = 0;       // rad
  // VFR_HUD
  float airspeed = 0, groundspeed = 0;      // m/s
  float alt = 0;                            // m
  float climb = 0;                          // m/s
  int16_t heading = 0;                      // deg (0-360)
  uint16_t throttle = 0;                    // %
  // SYS_STATUS
  float batteryVoltage = 0;                 // V
  int8_t batteryRemaining = -1;             // %
  // GPS_RAW_INT
  uint8_t gpsFixType = 0;
  uint8_t gpsSatellites = 255;
  int32_t gpsLat = 0, gpsLon = 0;           // degE7
  // HEARTBEAT
  uint8_t baseMode = 0;
  uint32_t customMode = 0;
  bool armed = false;

  uint32_t lastHeartbeatMs = 0;
  uint32_t lastAnyMsgMs = 0;
};

extern MavData mav;

void mavlinkInit(Stream *port);
void mavlinkUpdate();          // llamar cada loop()
bool mavlinkLinkOk();          // true si llegó algo reciente
const char *planeModeName(uint32_t customMode);
