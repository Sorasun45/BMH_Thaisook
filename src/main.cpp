#include <Arduino.h>
#include "config.h"
#include "bmh_uart.h"
#include "bmh_frame.h"
#include "bmh_state.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("BMH Professional stack starting...");

  // try auto-baud list
  uint32_t bauds[] = {115200, 57600, 38400, 19200, 9600};
  uint32_t found = BMH_UART_autoBaud(bauds, sizeof(bauds)/sizeof(bauds[0]), 400);
  if (found == 0) {
    Serial.println("Auto-baud failed; using default");
    BMH_UART_begin(BMH_DEFAULT_BAUD);
  } else {
    Serial.printf("Auto-baud: %u\n", found);
  }

  frame_init();
  state_init();
  Serial.println("Ready - send hex frames via Serial Monitor (e.g. 55 05 A0 01 05)");
}

void loop() {
  // feed bytes from UART into frame assembly
  while (BMH_UART_available()) {
    int b = BMH_UART_read();
    if (b < 0) break;
    frame_push_byte((uint8_t)b);
  }

  // process frames
  if (frame_has_frame()) {
    Frame f = frame_pop();
    if (f.valid) {
      state_handle_frame(f.raw, f.len);
    }
  }

  // periodic tasks
  state_tick();

  // forward user-typed hex to module (Serial Monitor -> BMH)
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      uint8_t out[128];
      int n = 0;
      char tmp[256];
      line.toCharArray(tmp, sizeof(tmp));
      char *p = strtok(tmp, " ,\t\r\n");
      while (p && n < (int)sizeof(out)) {
        out[n++] = (uint8_t) strtol(p, NULL, 16);
        p = strtok(NULL, " ,\t\r\n");
      }
      if (n>0) BMH_UART_write(out, n);
      Serial.printf("[TX→BMH] %d bytes\n", n);
    }
  }
}
