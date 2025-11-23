// config.h - central configuration
#pragma once

// Maximum frame buffer size from protocol (big enough)
#define FRAME_MAX 512

// Multi-package maximum (D0/D1/D2 → always <= 5 in BMH protocol)
#define MAX_PACKAGES 5
#define BMH_UART_NUM 2
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17

// Default starting baud - auto-baud will test alternatives
#define BMH_DEFAULT_BAUD 115200

// Multi-package timeout
#define MULTI_PKG_TIMEOUT_MS 3000U

// JSON buffer size
#define JSON_DOC_CAPACITY 16384

// Enable features
#define ENABLE_SIMULATOR 0   // set 1 to run simulator mode on ESP (no hardware)
#define ENABLE_BLUETOOTH 1   // set 1 to enable Bluetooth Serial output

// Logging level (0=none,1=error,2=info,3=debug)
#define LOG_LEVEL 3

