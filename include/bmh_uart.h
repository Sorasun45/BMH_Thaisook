#pragma once
#include <Arduino.h>

void BMH_UART_begin(uint32_t baud);
void BMH_UART_write(const uint8_t *buf, size_t len);
int  BMH_UART_available();
int  BMH_UART_read();
void BMH_UART_flush();

// Auto-baud: try list of bauds, returns successful baud or 0
uint32_t BMH_UART_autoBaud(const uint32_t *baudList, size_t nlist, unsigned long timeoutMs);

// Simulator toggle
void BMH_UART_enableSimulator(bool en);
bool BMH_UART_isSimulator();
