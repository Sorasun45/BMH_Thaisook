#pragma once
#include <stdint.h>

void state_init();
void state_handle_frame(const uint8_t *buf, int len);
void state_tick(); // call in main loop for timeouts
