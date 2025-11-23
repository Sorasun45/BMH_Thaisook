#include <Arduino.h>
#include <config.h>
#include "bmh_frame.h"
#include <string.h>

static uint8_t frameBuf[FRAME_MAX];
static int frameIdx = 0;
static bool hasFrame = false;
static Frame lastFrame;

uint8_t compute_checksum(const uint8_t *buf, int len) {
  uint32_t sum=0;
  for (int i=0;i<len-1;i++) sum += buf[i];
  return (uint8_t)((~sum)+1);
}

void frame_init() {
  frameIdx = 0; hasFrame = false;
}

void frame_push_byte(uint8_t b) {
  if (hasFrame) return; // haven't consumed last
  if (frameIdx == 0) {
    if (b != 0xAA) return;
    frameBuf[frameIdx++] = b;
    return;
  }
  frameBuf[frameIdx++] = b;
  if (frameIdx == 2) {
    if (frameBuf[1] == 0 || frameBuf[1] > FRAME_MAX) { frameIdx = 0; return; }
  }
  if (frameIdx > FRAME_MAX) { frameIdx = 0; return; }
  if (frameIdx >= 2) {
    int expected = frameBuf[1];
    if (frameIdx == expected) {
      // copy out
      lastFrame.valid = true;
      lastFrame.len = expected;
      memcpy(lastFrame.raw, frameBuf, expected);
      hasFrame = true;
      frameIdx = 0;
    }
  }
}

bool frame_has_frame() {
  return hasFrame;
}

Frame frame_pop() {
  Frame tmp = lastFrame;
  hasFrame = false;
  lastFrame.valid = false;
  return tmp;
}
