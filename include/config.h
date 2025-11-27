#ifndef CONFIG_H
#define CONFIG_H

// Serial settings
#define SERIAL_BAUD 115200
#define BMH_BAUD 115200
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17

// Timings
const unsigned long POLL_INTERVAL_MS = 200; // 200 ms
const unsigned long WEIGHT_POLL_INTERVAL_MS = 200; // 200 ms for faster weight reading

// Stability thresholds
const int STABLE_DELTA = 10;
const int STABLE_WEIGHT_DELTA = 10;
const int STABLE_IMPEDANCE_DELTA = 100;

const int STABLE_REQUIRED_CNT = 30;  // consecutive samples
const float MIN_WEIGHT_TO_START = 20.0; // minimum weight in kg to start measuring
const float MAX_WEIGHT_EMPTY = 5.0;     // maximum weight in kg to consider scale empty

// Tare settings
const int TARE_SAMPLES = 20;  // จำนวนตัวอย่างที่ใช้ในการ tare

// RX buffer size
#define RX_BUF_SIZE 512

#endif // CONFIG_H
