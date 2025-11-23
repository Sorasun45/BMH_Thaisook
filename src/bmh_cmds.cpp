#include <Arduino.h>
#include "config.h"
#include "bmh_state_struct.h"
#include "bmh_cmds.h"
#include "bmh_json.h"
#include "bmh_frame.h"
#include <string.h>
#include <stdlib.h>



static MultiState D0, D1, D2;

static void clearMulti(MultiState &s) {
  for (int i=0;i<MAX_PACKAGES;i++) {
    if (s.pkgs[i]) { free(s.pkgs[i]); s.pkgs[i]=NULL; s.pkglens[i]=0; }
  }
  s.active=false; s.total=0; s.received=0; s.last_ms=0;
}

static void initStates() { memset(&D0,0,sizeof(D0)); memset(&D1,0,sizeof(D1)); memset(&D2,0,sizeof(D2)); }

// helpers
static uint8_t checksum_ok(const uint8_t *buf, int len) {
  uint8_t c = compute_checksum(buf, len);
  return c == buf[len-1];
}

static void handle_A0(const uint8_t *buf, int len) {
  // return JSON via bmh_json
  bmh_json_emit_A0(buf, len);
}
static void handle_A1(const uint8_t *buf, int len) {
  bmh_json_emit_A1(buf, len);
}
static void handle_B0(const uint8_t *buf, int len) { bmh_json_emit_B0(buf,len); }
static void handle_B1(const uint8_t *buf, int len) { bmh_json_emit_B1(buf,len); }
static void handle_10(const uint8_t *buf, int len) { bmh_json_emit_10(buf,len); }
static void handle_E0(const uint8_t *buf, int len) { bmh_json_emit_E0(buf,len); }
static void handle_E1(const uint8_t *buf, int len) { bmh_json_emit_E1(buf,len); }
static void handle_F0(const uint8_t *buf, int len) { bmh_json_emit_F0(buf,len); }
static void handle_FF(const uint8_t *buf, int len) { bmh_json_emit_FF(buf,len); }

static void store_multi(MultiState &s, const uint8_t *buf, int len) {
  if (len < 5) return;
  uint8_t pkgInfo = buf[3];
  uint8_t total = (pkgInfo >> 4) & 0x0F;
  uint8_t current = pkgInfo & 0x0F;
  if (total == 0) total = 1;
  if (!s.active) {
    s.active = true; s.total = total; s.received = 0;
    for (int i=0;i<MAX_PACKAGES;i++){ s.pkgs[i]=NULL; s.pkglens[i]=0; }
  }
  int idx = (int)current - 1;
  if (idx < 0) idx = 0;
  if (idx >= MAX_PACKAGES) return;
  if (s.pkgs[idx] == NULL) {
    s.pkgs[idx] = (uint8_t*) malloc(len);
    if (s.pkgs[idx]) {
      memcpy(s.pkgs[idx], buf, len);
      s.pkglens[idx] = len;
      s.received++;
      s.last_ms = millis();
    }
  } else {
    s.last_ms = millis();
  }
}

static void try_finalize_D0() {
  if (!D0.active) return;
  if (D0.received < D0.total) return;
  bmh_json_emit_D0(D0.pkgs, D0.pkglens, D0.total);
  clearMulti(D0);
}

static void try_finalize_D1() {
  if (!D1.active) return;
  if (D1.received < D1.total) return;
  bmh_json_emit_D1(D1.pkgs, D1.pkglens, D1.total);
  clearMulti(D1);
}
static void try_finalize_D2() {
  if (!D2.active) return;
  if (D2.received < D2.total) return;
  bmh_json_emit_D2(D2.pkgs, D2.pkglens, D2.total);
  clearMulti(D2);
}

void process_bmh_frame(const uint8_t *buf, int len) {
  if (len < 4) return;
  if (!checksum_ok(buf,len)) {
    bmh_json_emit_error_checksum(buf,len);
    return;
  }
  uint8_t cmd = buf[2];
  switch (cmd) {
    case 0xA0: handle_A0(buf,len); break;
    case 0xA1: handle_A1(buf,len); break;
    case 0xB0: handle_B0(buf,len); break;
    case 0xB1: handle_B1(buf,len); break;
    case 0x10: handle_10(buf,len); break;
    case 0xE0: handle_E0(buf,len); break;
    case 0xE1: handle_E1(buf,len); break;
    case 0xF0: handle_F0(buf,len); break;
    case 0xFF: handle_FF(buf,len); break;
    case 0xD0:
      store_multi(D0, buf, len);
      try_finalize_D0();
      break;
    case 0xD1:
      store_multi(D1, buf, len);
      try_finalize_D1();
      break;
    case 0xD2:
      store_multi(D2, buf, len);
      try_finalize_D2();
      break;
    default:
      bmh_json_emit_unknown(buf, len);
      break;
  }
}

void check_multi_pkg_timeouts(unsigned long now) {
  MultiState *states[3] = {(MultiState*)&D0, (MultiState*)&D1, (MultiState*)&D2};
  const char *names[3] = {"D0","D1","D2"};
  for (int i=0;i<3;i++){
    MultiState *s = (MultiState*)states[i];
    if (!s->active) continue;
    if (s->received >= s->total) continue;
    if (s->last_ms > 0 && now - s->last_ms > MULTI_PKG_TIMEOUT_MS) {
      bmh_json_emit_multi_timeout(names[i], s->received, s->total);
      clearMulti((MultiState&)*s);
    }
  }
}
