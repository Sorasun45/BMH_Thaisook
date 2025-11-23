#pragma once
#include <stdint.h>

void process_bmh_frame(const uint8_t *buf, int len);
void check_multi_pkg_timeouts(unsigned long now);
