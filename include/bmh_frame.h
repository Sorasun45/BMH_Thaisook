#pragma once
#include <stdint.h>

struct Frame {
  bool valid;
  uint8_t raw[512];
  int len;
};

void frame_init();
void frame_push_byte(uint8_t b);
bool frame_has_frame();
Frame frame_pop();
uint8_t compute_checksum(const uint8_t *buf, int len);
