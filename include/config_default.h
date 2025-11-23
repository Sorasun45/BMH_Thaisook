#ifndef CONFIG_DEFAULT_H
#define CONFIG_DEFAULT_H

// ================================================================
// BMH05108 Protocol Configuration — Default Template
// ================================================================
// สำเนาไฟล์นี้ไปยัง include/config.h และปรับค่าตามฮาร์ดแวร์ของคุณ
// ================================================================

// ---- UART Configuration ----
// เลือก UART port (1=UART1, 2=UART2 ฯลฯ ตามบอร์ด)
#define BMH_UART_NUM 2

// RX และ TX pins (ปรับตามการต่อวงจรของคุณ)
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17

// Baud rate ค่าเริ่มต้น (ถ้าไม่ใช้ auto-baud)
#define BMH_DEFAULT_BAUD 115200

// ---- Frame Configuration ----
// ขนาดสูงสุดของเฟรม (bytes)
#define FRAME_MAX 512

// จำนวนแพ็กเกจสูงสุดที่รองรับใน multi-packet (D0, D1, D2)
#define MAX_PACKAGES 10

// เวลา timeout สำหรับ multi-packet (milliseconds)
// ถ้า ยังไม่ได้เฟรมแพ็กเกจครบ หลังจากเวลานี้ จะส่ง timeout message
#define MULTI_PKG_TIMEOUT_MS 5000

// ---- JSON Configuration ----
// ขนาด DynamicJsonDocument สำหรับ ArduinoJson
// ปรับเพิ่มถ้า JSON ที่ generate ใหญ่เกินไป
#define JSON_DOC_CAPACITY 2048

// ---- Bluetooth Configuration (Optional) ----
// ตั้ง 1 เพื่อเปิด Bluetooth Serial (ESP32 only)
#define ENABLE_BLUETOOTH 0

// Bluetooth device name (ถ้า ENABLE_BLUETOOTH = 1)
#define BLUETOOTH_DEVICE_NAME "BMH-ESP32"

// ---- Advanced Features ----
// Reserved for future use
#define ENABLE_SIMULATOR 0

#endif
