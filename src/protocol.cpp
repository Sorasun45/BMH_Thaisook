#include "protocol.h"
#include <HardwareSerial.h>

extern HardwareSerial BMH;

uint8_t computeChecksum(const uint8_t *buf, size_t lenWithoutChecksum)
{
  uint32_t s = 0;
  for (size_t i = 0; i < lenWithoutChecksum; ++i)
    s += buf[i];
  uint8_t sum8 = (uint8_t)(s & 0xFF);
  uint8_t ch = (uint8_t)((~sum8 + 1) & 0xFF);
  return ch;
}

void sendRaw(const uint8_t *data, size_t len)
{
  BMH.write(data, len);
  // also print to Serial monitor for debug
  Serial.print("TX -> ");
  for (size_t i = 0; i < len; ++i)
  {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
}

void buildAndSend(const uint8_t *body, size_t bodyLen)
{
  uint8_t buf[64];
  if (bodyLen + 1 > sizeof(buf))
    return;
  memcpy(buf, body, bodyLen);
  uint8_t ch = computeChecksum(buf, bodyLen);
  buf[bodyLen] = ch;
  sendRaw(buf, bodyLen + 1);
}

void send_cmd_A0()
{
  uint8_t packet[] = {0x55, 0x05, 0xA0, 0x01};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_A1()
{
  uint8_t packet[] = {0x55, 0x05, 0xA1, 0x00};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B0_len6_0303()
{
  uint8_t packet[] = {0x55, 0x06, 0xB0, 0x01, 0x03};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B0_len6_0106()
{
  uint8_t packet[] = {0x55, 0x06, 0xB0, 0x01, 0x06};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B1()
{
  uint8_t packet[] = {0x55, 0x05, 0xB1, 0x01};
  buildAndSend(packet, sizeof(packet));
}

uint16_t le_u16(const uint8_t *buf)
{
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

int16_t le_i16(const uint8_t *buf)
{
  return (int16_t)(buf[0] | (buf[1] << 8));
}

uint32_t le_u32(const uint8_t *buf)
{
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}
