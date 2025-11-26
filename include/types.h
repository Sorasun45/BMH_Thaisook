#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// State machine states
enum State
{
  WAIT_JSON,
  SEND_A0_WAIT_ACK,
  TARE_WEIGHT,
  WAIT_FOR_WEIGHT,
  SEND_A1_LOOP,
  SEND_B0_WAIT_ACK,
  SEND_B1_LOOP,
  SEND_B0_2_WAIT_ACK,
  SEND_B1_LOOP2,
  BUILD_AND_SEND_FINAL,
  DONE,
  WAIT_SCALE_EMPTY
};

// Calibration data structure
struct CalibData {
  float scale_factor;  // slope
  float offset;        // offset
};

// User information from JSON
struct UserInfo
{
  uint8_t gender = 0;
  uint8_t product_id = 0;
  uint16_t height = 0;
  uint8_t age = 0;
  bool valid = false;
};

// Impedance data structure
struct ImpedanceData
{
  uint32_t rh, lh, trunk, rf, lf;
};

#endif // TYPES_H
