#ifndef BUFFER_H
#define BUFFER_H

#include <Arduino.h>
#include "config.h"

// Circular buffer management
void pushRxByte(uint8_t b);
size_t rxAvailable();
bool rxPeek(size_t i, uint8_t &out);
bool rxRead(uint8_t &out);

// Frame parsing
bool tryParseFrame(uint8_t *frameBuf, size_t &frameLen);

#endif // BUFFER_H
