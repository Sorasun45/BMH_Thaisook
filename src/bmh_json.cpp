// bmh_json.cpp
// Verbose JSON emitter for BMH05108 — Raw + Processed (no scaling)
// Uses ArduinoJson v6

#include <Arduino.h>
#include "ArduinoJson.h"
#include "config.h"
#include "bmh_json.h"
#include <stdio.h>
#include <string.h>

// Helper readers (little-endian)
static inline uint16_t readU16(const uint8_t *b, int off) { return (uint16_t)(b[off] | (b[off+1] << 8)); }
static inline int16_t  readS16(const uint8_t *b, int off) { return (int16_t)(b[off] | (b[off+1] << 8)); }
static inline uint32_t readU32(const uint8_t *b, int off) { return (uint32_t)(b[off] | (b[off+1]<<8) | (b[off+2]<<16) | (b[off+3]<<24)); }

// Convert a buffer into spaced hex string (e.g. "55 06 B0 ...")
static String hexToString(const uint8_t *buf, int len) {
  String s;
  for (int i=0;i<len;i++) {
    if (i) s += ' ';
    char tmp[4];
    snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
    s += tmp;
  }
  return s;
}

static String frameInfo(const uint8_t *buf, int len) {
  return hexToString(buf, len);
}

static String ts() { return String(millis()); }

// Generic helpers to emit base JSON structure
static void emit_base_header(DynamicJsonDocument &doc, const uint8_t *buf, int len) {
  doc["timestamp"] = millis();
  doc["device"] = "BMH05108";
  JsonObject f = doc.createNestedObject("frame");
  f["raw_hex"] = frameInfo(buf, len);
  f["length"] = len;
}

static void emit_status(DynamicJsonDocument &doc, int code, const char *desc) {
  JsonObject s = doc.createNestedObject("status");
  s["code"] = code;
  s["ok"] = (code == 0);
  s["description"] = desc;
}

static void emit_command_meta(DynamicJsonDocument &doc, const char *code, const char *name, const char *category) {
  JsonObject c = doc.createNestedObject("command");
  c["code"] = code;
  c["name"] = name;
  c["category"] = category;
}

// Helper: add raw_bytes as hex string and raw array
static void add_raw_and_processed(DynamicJsonDocument &doc, const uint8_t *buf, int len, std::function<void(JsonObject)> fill_processed) {
  JsonObject data = doc.createNestedObject("data");
  data["raw_bytes"] = frameInfo(buf+3, len-4); // data payload bytes (exclude header/len/check)
  // also include raw as array
  JsonArray rawArr = data.createNestedArray("raw_array");
  for (int i=3;i<len-1;i++) rawArr.add(buf[i]);
  JsonObject proc = data.createNestedObject("processed");
  // user-supplied lambda fills processed fields
  fill_processed(proc);
}

// -------------------- Individual emitters --------------------

void bmh_json_emit_A0(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "A0", "Query Status", "device_query");
  // parse simple status payload
  int code = 0;
  const char *desc = "Success";
  if (len >= 4) {
    code = buf[3];
    if (code != 0) desc = "Non-zero status";
  }
  emit_status(doc, code, desc);
  // data: raw + processed (minimal)
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    if (len >= 4) {
      p["status_raw"] = buf[3];
      p["status_desc"] = desc;
    }
  });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_A1(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(2048);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "A1", "Weight / A1 Frame", "weight");

  if (len < 14) {
    emit_status(doc, 1, "Frame too short");
    add_raw_and_processed(doc, buf, len, [&](JsonObject p){});
    serializeJson(doc, Serial); Serial.println();
    return;
  }

  emit_status(doc, 0, "Success");

  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    // According to protocol positions used earlier
    int16_t stable_catty_x10 = readS16(buf,5);
    int16_t realtime_catty_x10 = readS16(buf,7);
    uint32_t adc = readU32(buf,9);
    // Option A: no scaling, processed = raw
    p["stable_catty_x10_raw"] = stable_catty_x10;
    p["realtime_catty_x10_raw"] = realtime_catty_x10;
    p["adc_raw"] = adc;
    // also provide human-friendly conversions but no scaling requested
    p["stable_catty_x10"] = stable_catty_x10;
    p["realtime_catty_x10"] = realtime_catty_x10;
  });

  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_B0(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "B0", "Sensor Calibration", "calibration");
  int result = 0;
  if (len >= 4) result = buf[3];
  emit_status(doc, result, result==0?"Success":"Failure");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){ if (len>=4) p["result_code"] = buf[3]; });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_B1(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(2048);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "B1", "Impedance / B1 Frame", "impedance");
  if (len >= 27) {
    emit_status(doc, 0, "Success");
    add_raw_and_processed(doc, buf, len, [&](JsonObject p){
      uint32_t rh = readU32(buf,6);
      uint32_t lh = readU32(buf,10);
      uint32_t trunk = readU32(buf,14);
      uint32_t rf = readU32(buf,18);
      uint32_t lf = readU32(buf,22);
      p["right_hand_raw"] = rh;
      p["left_hand_raw"] = lh;
      p["trunk_raw"] = trunk;
      p["right_foot_raw"] = rf;
      p["left_foot_raw"] = lf;
      p["right_hand"] = rh; // no scaling
      p["left_hand"] = lh;
      p["trunk"] = trunk;
      p["right_foot"] = rf;
      p["left_foot"] = lf;
    });
  } else if (len >= 13) {
    emit_status(doc, 0, "Success (4-electrode)");
    add_raw_and_processed(doc, buf, len, [&](JsonObject p){
      int16_t phase_x10 = readS16(buf,6);
      uint32_t impedance = readU32(buf,8);
      p["phase_x10_raw"] = phase_x10;
      p["impedance_raw"] = impedance;
      p["phase_x10"] = phase_x10;
      p["impedance"] = impedance;
    });
  } else {
    emit_status(doc, 1, "B1 frame too short");
    add_raw_and_processed(doc, buf, len, [&](JsonObject p){});
  }
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_10(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "10", "Real-time Sensor Value", "sensor_data");
  if (len < 6) {
    emit_status(doc,1,"Frame too short");
    add_raw_and_processed(doc, buf, len, [&](JsonObject p){});
    serializeJson(doc, Serial); Serial.println();
    return;
  }
  emit_status(doc,0,"Success");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    // Example: sensor id in buf[3], value in buf[4..]
    p["sensor_id_raw"] = buf[3];
    if (len >= 7) {
      uint32_t val = readU32(buf,4);
      p["value_raw"] = val;
      p["value"] = val; // no scaling
    } else if (len >= 5) {
      p["value_raw"] = buf[4];
      p["value"] = buf[4];
    }
  });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_E0(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "E0", "Config Read", "config");
  emit_status(doc,0,"Success");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    if (len >= 6) {
      uint16_t ver = readU16(buf,4);
      p["type_raw"] = buf[3];
      p["version_raw"] = ver;
      p["type"] = (int)buf[3];
      p["version"] = (int)ver;
    }
  });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_E1(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(512);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "E1", "Config Set", "config");
  int res = (len>=4)?buf[3]:1;
  emit_status(doc,res, res==0?"Success":"Failure");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){ if (len>=4) p["result"] = buf[3]; });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_F0(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "F0", "System Action", "system_action");
  emit_status(doc,0,"Success");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    // generic action payload
    if (len>=4) p["action_code"] = buf[3];
  });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_FF(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(1024);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "FF", "System Error", "system_error");
  emit_status(doc,1,"System reported error");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){
    if (len>=4) p["error_code"] = buf[3];
  });
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_unknown(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(512);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "??", "Unknown Command", "unknown");
  emit_status(doc,1,"Unknown command");
  add_raw_and_processed(doc, buf, len, [&](JsonObject p){});
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_error_checksum(const uint8_t *buf, int len) {
  DynamicJsonDocument doc(512);
  emit_base_header(doc, buf, len);
  emit_command_meta(doc, "ERR", "Checksum Fail", "error");
  emit_status(doc,2,"Checksum mismatch");
  JsonObject data = doc.createNestedObject("data");
  data["raw_bytes"] = frameInfo(buf, len);
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_multi_timeout(const char *name, int got, int total) {
  DynamicJsonDocument doc(512);
  doc["timestamp"] = millis();
  doc["device"] = "BMH05108";
  JsonObject f = doc.createNestedObject("frame");
  f["raw_hex"] = "";
  f["length"] = 0;
  emit_command_meta(doc, name, "Multi-packet Timeout", "multi_packet");
  JsonObject s = doc.createNestedObject("status");
  s["code"] = 3;
  s["ok"] = false;
  s["description"] = "Timeout waiting for all packages";
  JsonObject d = doc.createNestedObject("data");
  d["received_pkgs"] = got;
  d["expected_pkgs"] = total;
  serializeJson(doc, Serial); Serial.println();
}

// Multi-packet emitters (D0/D1/D2) — here we provide raw arrays + simple processed mapping
static void emit_multi_generic(const char *cmdName, uint8_t *pkgs[], int lens[], int total) {
  // estimate document size (conservative)
  DynamicJsonDocument doc(JSON_DOC_CAPACITY);
  doc["timestamp"] = millis();
  doc["device"] = "BMH05108";
  JsonObject f = doc.createNestedObject("frame");
  f["raw_hex"] = "<multi-packets>";
  f["length"] = 0;
  emit_command_meta(doc, cmdName, "Multi-packet Data", "multi_packet");
  emit_status(doc, 0, "Success");

  JsonArray packages = doc.createNestedArray("packages");
  for (int p=0;p<total;p++) {
    JsonObject pkg = packages.createNestedObject();
    pkg["idx"] = p+1;
    if (!pkgs[p]) { pkg["error"] = "missing"; continue; }
    pkg["raw_hex"] = hexToString(pkgs[p], lens[p]);
    JsonArray rawArr = pkg.createNestedArray("raw_array");
    for (int i=0;i<lens[p];i++) rawArr.add(pkgs[p][i]);
    // simple processed mapping: expose a few meaningful fields when present
    JsonObject proc = pkg.createNestedObject("processed");
    if (lens[p] > 5) {
      // example: read first few 16-bit values (no scaling)
      int idx = 5; // data section start
      int count = (lens[p]-6)/2; // pairs available
      for (int k=0;k<count && k<20;k++) {
        uint16_t v = readU16(pkgs[p], idx + k*2);
        char key[16]; snprintf(key, sizeof(key), "v%02d", k);
        proc[key] = v;
      }
    }
  }
  serializeJson(doc, Serial); Serial.println();
}

void bmh_json_emit_D0(uint8_t *pkgs[], int lens[], int total) { emit_multi_generic("D0", pkgs, lens, total); }
void bmh_json_emit_D1(uint8_t *pkgs[], int lens[], int total) { emit_multi_generic("D1", pkgs, lens, total); }
void bmh_json_emit_D2(uint8_t *pkgs[], int lens[], int total) { emit_multi_generic("D2", pkgs, lens, total); }



