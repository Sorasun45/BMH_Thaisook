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
  WAIT_RESULT_PACKETS,
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

// Body Type classification
enum BodyType : uint8_t
{
  BODY_TYPE_THIN = 0x01,
  BODY_TYPE_THIN_MUSCLE = 0x02,
  BODY_TYPE_MUSCULAR = 0x03,
  BODY_TYPE_OBESE_FAT = 0x04,
  BODY_TYPE_FAT_MUSCLE = 0x05,
  BODY_TYPE_MUSCLE_FAT = 0x06,
  BODY_TYPE_LACK_EXERCISE = 0x07,
  BODY_TYPE_STANDARD = 0x08,
  BODY_TYPE_STANDARD_MUSCLE = 0x09
};

// Error Type for D0 result
enum ErrorType : uint8_t
{
  ERROR_TYPE_NONE = 0x00,
  ERROR_TYPE_AGE = 0x01,
  ERROR_TYPE_HEIGHT = 0x02,
  ERROR_TYPE_WEIGHT = 0x03,
  ERROR_TYPE_SEX = 0x04,
  ERROR_TYPE_PEOPLE_TYPE = 0x05,
  ERROR_TYPE_Z_TWO_LEGS = 0x06,
  ERROR_TYPE_Z_TWO_ARMS = 0x07,
  ERROR_TYPE_Z_LEFT_BODY = 0x08,
  ERROR_TYPE_Z_LEFT_ARM = 0x09,
  ERROR_TYPE_Z_RIGHT_ARM = 0x0A,
  ERROR_TYPE_Z_LEFT_LEG = 0x0B,
  ERROR_TYPE_Z_RIGHT_LEG = 0x0C,
  ERROR_TYPE_Z_TRUNK = 0x0D
};

// Result packets storage (0x51-0x55)
struct ResultPackets
{
  uint8_t packet1[256];  // 0x51
  uint8_t packet2[256];  // 0x52
  uint8_t packet3[256];  // 0x53
  uint8_t packet4[256];  // 0x54
  uint8_t packet5[256];  // 0x55
  size_t len1, len2, len3, len4, len5;
  bool received1, received2, received3, received4, received5;
  uint8_t total_packets;  // จำนวน packet ทั้งหมด
  uint8_t received_count; // จำนวน packet ที่ได้รับแล้ว
  ErrorType error_type;   // Error from packet
  
  void reset() {
    len1 = len2 = len3 = len4 = len5 = 0;
    received1 = received2 = received3 = received4 = received5 = false;
    total_packets = 5;
    received_count = 0;
    error_type = ERROR_TYPE_NONE;
  }
  
  bool isComplete() const {
    return (received_count >= total_packets);
  }
  
  bool hasError() const {
    return (error_type != ERROR_TYPE_NONE);
  }
};

#endif // TYPES_H
