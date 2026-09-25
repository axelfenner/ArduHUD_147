#include "mavlink_mini.h"
#include "config.h"

MavData mav;

static Stream *s_port = nullptr;

// ---------------------------------------------------------------- CRC-16/MCRF4XX (X.25), igual que MAVLink oficial
static uint16_t crcAccumulate(uint8_t data, uint16_t crc) {
  uint8_t tmp = data ^ (uint8_t)(crc & 0xFF);
  tmp ^= (tmp << 4);
  return (crc >> 8) ^ ((uint16_t)tmp << 8) ^ ((uint16_t)tmp << 3) ^ ((uint16_t)tmp >> 4);
}

// CRC_EXTRA oficiales del dialecto "common" para los mensajes que decodificamos
static uint8_t crcExtraFor(uint32_t msgId) {
  switch (msgId) {
    case 0:  return 50;   // HEARTBEAT
    case 1:  return 124;  // SYS_STATUS
    case 24: return 24;   // GPS_RAW_INT
    case 30: return 39;   // ATTITUDE
    case 74: return 20;   // VFR_HUD
    default: return 0;
  }
}

// ---------------------------------------------------------------- Lectores little-endian seguros (con padding a 0)
static uint32_t rdU32(const uint8_t *p, int len, int off) { uint32_t v = 0; for (int i = 0; i < 4; i++) if (off + i < len) v |= (uint32_t)p[off + i] << (8 * i); return v; }
static uint16_t rdU16(const uint8_t *p, int len, int off) { uint16_t v = 0; for (int i = 0; i < 2; i++) if (off + i < len) v |= (uint16_t)p[off + i] << (8 * i); return v; }
static int8_t   rdI8 (const uint8_t *p, int len, int off) { return (off < len) ? (int8_t)p[off] : 0; }
static int16_t  rdI16(const uint8_t *p, int len, int off) { return (int16_t)rdU16(p, len, off); }
static int32_t  rdI32(const uint8_t *p, int len, int off) { return (int32_t)rdU32(p, len, off); }
static float    rdF32(const uint8_t *p, int len, int off) { uint32_t u = rdU32(p, len, off); float f; memcpy(&f, &u, 4); return f; }

// ---------------------------------------------------------------- Decodificadores por mensaje
static void decodeHeartbeat(const uint8_t *p, int len) {
  mav.customMode = rdU32(p, len, 0);
  mav.baseMode = (uint8_t)rdI8(p, len, 6);
  mav.armed = (mav.baseMode & 0x80) != 0;
  mav.lastHeartbeatMs = millis();
}

static void decodeAttitude(const uint8_t *p, int len) {
  mav.roll  = rdF32(p, len, 4);
  mav.pitch = rdF32(p, len, 8);
  mav.yaw   = rdF32(p, len, 12);
}

static void decodeVfrHud(const uint8_t *p, int len) {
  mav.airspeed    = rdF32(p, len, 0);
  mav.groundspeed = rdF32(p, len, 4);
  mav.alt         = rdF32(p, len, 8);
  mav.climb       = rdF32(p, len, 12);
  mav.heading     = rdI16(p, len, 16);
  mav.throttle    = rdU16(p, len, 18);
}

static void decodeSysStatus(const uint8_t *p, int len) {
  uint16_t voltMv = rdU16(p, len, 14);
  mav.batteryVoltage = (voltMv == 0xFFFF) ? -1.0f : voltMv / 1000.0f;
  mav.batteryRemaining = rdI8(p, len, 30);
}

static void decodeGpsRawInt(const uint8_t *p, int len) {
  mav.gpsLat = rdI32(p, len, 8);
  mav.gpsLon = rdI32(p, len, 12);
  mav.gpsFixType = (uint8_t)rdI8(p, len, 28);
  mav.gpsSatellites = (uint8_t)rdI8(p, len, 29);
}

static void dispatch(uint32_t msgId, const uint8_t *payload, int len) {
  mav.lastAnyMsgMs = millis();
  switch (msgId) {
    case 0:  decodeHeartbeat(payload, len); break;
    case 1:  decodeSysStatus(payload, len); break;
    case 24: decodeGpsRawInt(payload, len); break;
    case 30: decodeAttitude(payload, len); break;
    case 74: decodeVfrHud(payload, len); break;
    default: break;
  }
}

// ---------------------------------------------------------------- Máquina de estados del receptor
enum ParseState { WAIT_START, V1_HEADER, V2_HEADER, GOT_PAYLOAD, GOT_CRC };

static uint8_t s_buf[280];
static int s_idx = 0;
static int s_expectedLen = 0;
static bool s_isV2 = false;
static uint8_t s_incompatFlags = 0;
static uint32_t s_msgId = 0;

static void resetParser() {
  s_idx = 0;
}

static void tryParseByte(uint8_t c) {
  // s_buf acumula: [STX][...header...][payload][crc_lo][crc_hi]  (sin firma, la saltamos aparte)
  if (s_idx == 0) {
    if (c == 0xFE) { s_isV2 = false; s_buf[s_idx++] = c; }
    else if (c == 0xFD) { s_isV2 = true; s_buf[s_idx++] = c; }
    return;
  }

  s_buf[s_idx++] = c;

  int lenFieldIdx = 1;
  int headerLenV1 = 6;  // STX,len,seq,sysid,compid,msgid
  int headerLenV2 = 10; // STX,len,incompat,compat,seq,sysid,compid,msgid(3)

  int header = s_isV2 ? headerLenV2 : headerLenV1;

  if (s_idx == header) {
    s_expectedLen = s_buf[lenFieldIdx];
    if (s_isV2) {
      s_incompatFlags = s_buf[2];
      s_msgId = (uint32_t)s_buf[7] | ((uint32_t)s_buf[8] << 8) | ((uint32_t)s_buf[9] << 16);
    } else {
      s_msgId = s_buf[5];
    }
  }

  if (s_idx > header) {
    int totalNoSig = header + s_expectedLen + 2; // header + payload + crc16
    if (s_idx >= totalNoSig) {
      // frame v2 firmado: hay 13 bytes extra de firma que debemos descartar
      int sigBytes = (s_isV2 && (s_incompatFlags & 0x01)) ? 13 : 0;
      int total = totalNoSig + sigBytes;
      if (s_idx >= total) {
        // Validar CRC
        uint16_t crc = 0xFFFF;
        for (int i = 1; i < header + s_expectedLen; i++) crc = crcAccumulate(s_buf[i], crc);
        crc = crcAccumulate(crcExtraFor(s_msgId), crc);
        uint16_t crcRecv = s_buf[totalNoSig - 2] | (s_buf[totalNoSig - 1] << 8);
        if (crc == crcRecv) {
          dispatch(s_msgId, &s_buf[header], s_expectedLen);
        }
        resetParser();
      } else if (s_idx >= (int)sizeof(s_buf)) {
        resetParser();
      }
    } else if (s_idx >= (int)sizeof(s_buf)) {
      resetParser();
    }
  }
}

void mavlinkInit(Stream *port) {
  s_port = port;
  resetParser();
}

void mavlinkUpdate() {
  if (!s_port) return;
  while (s_port->available()) {
    tryParseByte((uint8_t)s_port->read());
  }

#if MAV_SEND_HEARTBEAT
  static uint32_t lastSend = 0;
  if (millis() - lastSend > 1000) {
    lastSend = millis();
    // HEARTBEAT MAVLink v1 mínimo: type=6(GCS), autopilot=8(INVALID), base_mode=0, custom_mode=0, status=0
    uint8_t payload[9] = {0, 0, 0, 0, 6, 8, 0, 0, 3};
    uint8_t frame[8 + 9 + 2];
    static uint8_t seq = 0;
    frame[0] = 0xFE;
    frame[1] = 9;
    frame[2] = seq++;
    frame[3] = 255; // sysid GCS
    frame[4] = 190; // compid GCS
    frame[5] = 0;   // HEARTBEAT
    memcpy(&frame[6], payload, 9);
    uint16_t crc = 0xFFFF;
    for (int i = 1; i < 6 + 9; i++) crc = crcAccumulate(frame[i], crc);
    crc = crcAccumulate(50, crc);
    frame[6 + 9] = crc & 0xFF;
    frame[6 + 9 + 1] = crc >> 8;
    s_port->write(frame, sizeof(frame));
  }
#endif
}

bool mavlinkLinkOk() {
  return (millis() - mav.lastAnyMsgMs) < MAV_LINK_TIMEOUT_MS && mav.lastAnyMsgMs != 0;
}

const char *planeModeName(uint32_t customMode) {
  switch (customMode) {
    case 0:  return "MANUAL";
    case 1:  return "CIRCLE";
    case 2:  return "STABILIZE";
    case 3:  return "TRAINING";
    case 4:  return "ACRO";
    case 5:  return "FBWA";
    case 6:  return "FBWB";
    case 7:  return "CRUISE";
    case 8:  return "AUTOTUNE";
    case 10: return "AUTO";
    case 11: return "RTL";
    case 12: return "LOITER";
    case 14: return "AVOID_ADSB";
    case 15: return "GUIDED";
    case 17: return "QSTABILIZE";
    case 18: return "QHOVER";
    case 19: return "QLOITER";
    case 20: return "QLAND";
    case 21: return "QRTL";
    case 22: return "QAUTOTUNE";
    case 23: return "QACRO";
    case 24: return "THERMAL";
    default: return "???";
  }
}
