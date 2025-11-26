#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

// Compute checksum for protocol
uint8_t computeChecksum(const uint8_t *buf, size_t lenWithoutChecksum);

// Send raw bytes
void sendRaw(const uint8_t *data, size_t len);

// Build and send command with checksum
void buildAndSend(const uint8_t *body, size_t bodyLen);

// Protocol commands
void send_cmd_A0();
void send_cmd_A1();
void send_cmd_B0_len6_0303();
void send_cmd_B0_len6_0106();
void send_cmd_B1();

// Little-endian parsers
uint16_t le_u16(const uint8_t *buf);
int16_t le_i16(const uint8_t *buf);
uint32_t le_u32(const uint8_t *buf);

#endif // PROTOCOL_H
