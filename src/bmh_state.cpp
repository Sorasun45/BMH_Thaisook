#include <Arduino.h>
#include "bmh_state.h"
#include "bmh_cmds.h"      // process_bmh_frame() อยู่ในนี้
#include "bmh_frame.h"
#include "config.h"

// เก็บสถานะภายใน state machine
static unsigned long lastTick = 0;

void state_init() {
    lastTick = millis();
}

// callback จาก main.cpp
void state_handle_frame(const uint8_t *buf, int len) {
    // ส่ง frame ไปให้ decoder กลาง
    process_bmh_frame(buf, len);
}

void state_tick() {
    unsigned long now = millis();

    // เรียก timeout checker สำหรับ multi-packet
    check_multi_pkg_timeouts(now);

    lastTick = now;
}
