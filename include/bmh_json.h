#pragma once
#include <stdint.h>

void bmh_json_emit_A0(const uint8_t *buf,int len);
void bmh_json_emit_A1(const uint8_t *buf,int len);
void bmh_json_emit_B0(const uint8_t *buf,int len);
void bmh_json_emit_B1(const uint8_t *buf,int len);
void bmh_json_emit_10(const uint8_t *buf,int len);
void bmh_json_emit_E0(const uint8_t *buf,int len);
void bmh_json_emit_E1(const uint8_t *buf,int len);
void bmh_json_emit_F0(const uint8_t *buf,int len);
void bmh_json_emit_FF(const uint8_t *buf,int len);
void bmh_json_emit_D0(uint8_t **pkgs, int *pkglens, int total);
void bmh_json_emit_D1(uint8_t **pkgs, int *pkglens, int total);
void bmh_json_emit_D2(uint8_t **pkgs, int *pkglens, int total);
void bmh_json_emit_unknown(const uint8_t *buf,int len);
void bmh_json_emit_error_checksum(const uint8_t*buf,int len);
void bmh_json_emit_multi_timeout(const char *cmd, int received, int expected);
