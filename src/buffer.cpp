#include "buffer.h"
#include "protocol.h"

// RX buffer for BMH
static uint8_t rxBuf[RX_BUF_SIZE];
static size_t rxHead = 0; // next write index
static size_t rxTail = 0; // next read index

void pushRxByte(uint8_t b)
{
  rxBuf[rxHead] = b;
  rxHead = (rxHead + 1) % RX_BUF_SIZE;
  // simple overflow handling: advance tail if full
  if (rxHead == rxTail)
  {
    rxTail = (rxTail + 1) % RX_BUF_SIZE;
  }
}

size_t rxAvailable()
{
  if (rxHead >= rxTail)
    return rxHead - rxTail;
  return RX_BUF_SIZE - (rxTail - rxHead);
}

bool rxPeek(size_t i, uint8_t &out)
{
  if (i >= rxAvailable())
    return false;
  size_t idx = (rxTail + i) % RX_BUF_SIZE;
  out = rxBuf[idx];
  return true;
}

bool rxRead(uint8_t &out)
{
  if (rxAvailable() == 0)
    return false;
  out = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_BUF_SIZE;
  return true;
}

bool tryParseFrame(uint8_t *frameBuf, size_t &frameLen)
{
  // Need at least header + length + order + checksum minimal
  if (rxAvailable() < 3)
    return false;

  // Look for 0xAA header (answer frames from device start with 0xAA)
  bool found = false;
  size_t avail = rxAvailable();
  for (size_t i = 0; i < avail; ++i)
  {
    uint8_t b;
    rxPeek(i, b);
    if (b == 0xAA)
    {
      // consume preceding bytes
      for (size_t j = 0; j < i; ++j)
      {
        uint8_t tmp;
        rxRead(tmp); // drop
      }
      found = true;
      break;
    }
  }
  if (!found)
  {
    // drop all
    uint8_t tmp;
    while (rxRead(tmp))
      ;
    return false;
  }

  // Now rxTail points to header 0xAA
  if (rxAvailable() < 2)
    return false; // need length byte
  uint8_t hdr, lengthByte;
  rxPeek(0, hdr);
  rxPeek(1, lengthByte);
  
  size_t totalFrameLen = (size_t)lengthByte;
  // Safety: min frame length should be >= 5 or so
  if (totalFrameLen < 5)
  {
    // consume one byte to avoid infinite loop
    uint8_t tmp;
    rxRead(tmp);
    return false;
  }
  if (rxAvailable() < totalFrameLen)
    return false; // wait for more data

  // Read complete frame into frameBuf
  for (size_t i = 0; i < totalFrameLen; ++i)
  {
    rxRead(frameBuf[i]);
  }
  frameLen = totalFrameLen;

  // Verify checksum: check last byte is checksum
  if (frameLen < 1)
    return false;
  uint8_t checksum = frameBuf[frameLen - 1];
  uint8_t calc = computeChecksum(frameBuf, frameLen - 1);
  if (checksum != calc)
  {
    Serial.printf("Frame checksum mismatch: got %02X calc %02X\r\n", checksum, calc);
    return false;
  }
  // Valid frame returned
  return true;
}
