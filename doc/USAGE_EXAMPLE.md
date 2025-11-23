# BMH05108 Protocol — ตัวอย่างการใช้งาน

เอกสารนี้อธิบายวิธีการใช้โปรโตคอล BMH05108 ในโปรเจคต่าง ๆ

---

## **1. การใช้งานพื้นฐาน (Basic Usage)**

### **โปรเจคใหม่ใน PlatformIO**

#### ขั้นตอนที่ 1: ตั้งค่า `platformio.ini`
```ini
[env:esp32]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200

lib_deps =
    ArduinoJson@^6.x
    # หากโปรโตคอลเป็น library: 
    # BMH-Protocol (หรือ URL ของ git repo)
```

#### ขั้นตอนที่ 2: คัดลอกไฟล์โปรโตคอล
สำเนาไฟล์เหล่านี้จากโปรเจคเดิมไปยังโปรเจคใหม่:
```
src/bmh_uart.cpp
src/bmh_uart.h
src/bmh_frame.cpp
src/bmh_frame.h
src/bmh_cmds.cpp
src/bmh_cmds.h
src/bmh_json.cpp
src/bmh_json.h
src/bmh_state.cpp
src/bmh_state.h
src/bmh_state_struct.h
include/config.h (หรือ config_default.h)
```

#### ขั้นตอนที่ 3: ตั้งค่า `include/config.h`
ปรับค่า pins, baud, ขนาดบัฟเฟอร์ตามฮาร์ดแวร์ของคุณ:
```cpp
#ifndef CONFIG_H
#define CONFIG_H

// UART Configuration
#define BMH_UART_NUM 2
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17
#define BMH_DEFAULT_BAUD 115200

// Frame Configuration
#define FRAME_MAX 512
#define MAX_PACKAGES 10
#define MULTI_PKG_TIMEOUT_MS 5000

// JSON Configuration
#define JSON_DOC_CAPACITY 2048

// Bluetooth (optional)
#define ENABLE_BLUETOOTH 0

#endif
```

#### ขั้นตอนที่ 4: เขียน `main.cpp`
```cpp
#include <Arduino.h>
#include "config.h"
#include "bmh_uart.h"
#include "bmh_frame.h"
#include "bmh_state.h"

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("BMH Protocol Test starting...");

  // เริ่มต้น UART (เลือก auto-baud หรือ fixed)
  BMH_UART_begin(BMH_DEFAULT_BAUD);
  
  // เริ่มต้น frame parser
  frame_init();
  
  // เริ่มต้น state machine
  state_init();
  
  Serial.println("Ready to receive BMH frames");
}

void loop() {
  // อ่านข้อมูลจาก UART
  while (BMH_UART_available()) {
    int b = BMH_UART_read();
    if (b < 0) break;
    frame_push_byte((uint8_t)b);
  }

  // ประมวลผลเฟรมที่ได้รับ
  if (frame_has_frame()) {
    Frame f = frame_pop();
    if (f.valid) {
      // ส่งไปให้ handler (จะแปลงเป็น JSON ตามคำสั่ง)
      state_handle_frame(f.raw, f.len);
    }
  }

  // ตรวจสอบ timeout สำหรับ multi-packet
  state_tick();

  // (optional) อ่านจากคีย์บอร์ด/Serial Monitor เพื่อส่งเฟรมทดสอบ
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) {
      uint8_t out[128];
      int n = 0;
      char tmp[256];
      line.toCharArray(tmp, sizeof(tmp));
      char *p = strtok(tmp, " ,\t\r\n");
      while (p && n < (int)sizeof(out)) {
        out[n++] = (uint8_t) strtol(p, NULL, 16);
        p = strtok(NULL, " ,\t\r\n");
      }
      if (n > 0) {
        BMH_UART_write(out, n);
        Serial.printf("[TX→BMH] %d bytes\n", n);
      }
    }
  }
}
```

---

## **2. การใช้งานขั้นสูง (Advanced Usage)**

### **2.1 สำหรับการประมวลผลคำสั่งเฉพาะ**

ถ้าคุณต้องการจัดการเฉพาะคำสั่งบางตัว แทนที่จะใช้ `bmh_json.*` ทั้งหมด สามารถแก้ไข `bmh_cmds.cpp` เพื่อโทรไปยังฟังก์ชัน callback ของคุณเอง:

```cpp
// ตัวอย่าง: เพิ่ม callback handler
typedef void (*BMH_Handler)(const uint8_t *buf, int len);

static BMH_Handler custom_handler_A0 = NULL;

void bmh_register_handler_A0(BMH_Handler handler) {
  custom_handler_A0 = handler;
}

static void handle_A0(const uint8_t *buf, int len) {
  if (custom_handler_A0) {
    custom_handler_A0(buf, len);
  } else {
    bmh_json_emit_A0(buf, len); // default: JSON
  }
}
```

### **2.2 หลายโปรเจคใช้ config ต่างกัน**

สร้าง wrapper init function ที่ยอมรับ config struct:

```cpp
// bmh_config.h
typedef struct {
  uint32_t uart_baud;
  uint16_t frame_max;
  uint16_t json_capacity;
  uint32_t multi_timeout_ms;
} BMH_Config;

// bmh_init.cpp
static BMH_Config g_config;

void bmh_init(const BMH_Config *cfg) {
  g_config = *cfg;
  BMH_UART_begin(g_config.uart_baud);
  frame_init();
  state_init();
}
```

### **2.3 ส่วนเชื่อมต่อกับ Cloud / Database**

แทนการส่ง JSON ไปยัง Serial ตรง ๆ สามารถเก็บไปในคิว แล้วส่งไป cloud/database:

```cpp
// ตัวอย่าง pseudocode
#include <queue>

std::queue<String> json_queue;

// override bmh_json emit functions เพื่อใส่คิว แทนการ Serial.println
void custom_emit_A0(const uint8_t *buf, int len) {
  String json = generateJsonA0(buf, len);
  json_queue.push(json);
  // ส่งไป cloud ในเวลาที่เหมาะสม
}
```

---

## **3. ตัวอย่างการส่งเฟรมทดสอบ**

### **ผ่าน Serial Monitor (PlatformIO)**

เปิด Serial Monitor แล้วพิมพ์เฟรมเป็น hex (คั่นด้วยเว้นวรรค):

```
AA 05 A0 00 B1
```

ระบบจะส่ง JSON กลับ:
```json
{"timestamp":1234,"device":"BMH05108",...}
```

### **ผ่าน Python Script**

```python
import serial
import json
import time

# เชื่อมต่อ Serial
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# ส่งเฟรม Query Status
frame = bytes([0xAA, 0x05, 0xA0, 0x00, 0xB1])
ser.write(frame)

# รอ JSON ตอบกลับ
time.sleep(0.1)
response = ser.readline().decode('utf-8').strip()
if response:
    data = json.loads(response)
    print(f"Status: {data['status']['description']}")

ser.close()
```

---

## **4. Debugging Tips**

### **4.1 ตรวจสอบ checksum**
ใช้ฟังก์ชัน `compute_checksum()` ในโค้ด:
```cpp
uint8_t frame[] = {0xAA, 0x05, 0xA0, 0x00, 0x00};
uint8_t check = compute_checksum(frame, 5);
// frame[4] ควรเป็น check (0xB1)
```

### **4.2 ตรวจ UART connection**
```cpp
void loop() {
  if (BMH_UART_available()) {
    int b = BMH_UART_read();
    Serial.printf("Received: 0x%02X\n", b);
  }
}
```

### **4.3 ตรวจ JSON ที่ generate**
เปิด Serial Monitor ที่ 115200 baud แล้ออ่าน JSON ที่ print ออกมา

---

## **5. Troubleshooting**

| ปัญหา | สาเหตุ | วิธีแก้ |
|-------|--------|--------|
| "Checksum fail" JSON | Checksum ผิด | ตรวจการคำนวณ checksum ตามสูตร `(~sum)+1` |
| ไม่ได้รับเฟรม | UART pin ผิด | ตรวจสอบ `BMH_RX_PIN`, `BMH_TX_PIN` ใน config |
| Multi-packet timeout | เฟรมแพ็กเกจถัดไปไม่มา | ส่งแพ็กเกจที่ 2, 3 ให้ครบตามจำนวน |
| JSON Document too large | ขนาด `JSON_DOC_CAPACITY` เล็กเกินไป | เพิ่มค่า `JSON_DOC_CAPACITY` ใน config |

---

## **6. API Quick Reference**

| ฟังก์ชัน | ความหมาย |
|----------|---------|
| `BMH_UART_begin(baud)` | เริ่มต้น UART |
| `frame_push_byte(b)` | เพิ่มไบต์เข้าไปในเฟรม |
| `frame_has_frame()` | ตรวจว่ามีเฟรมสมบูรณ์ |
| `frame_pop()` | คืนเฟรมที่ประกอบเสร็จ |
| `process_bmh_frame(buf, len)` | ประมวลผลเฟรม (checksum + dispatch) |
| `bmh_json_emit_*()` | ส่ง JSON สำหรับแต่ละคำสั่ง |
| `state_handle_frame(buf, len)` | ส่งเฟรมไปให้ state handler |
| `state_tick()` | เรียกตรวจ timeout ทุกรอบ |

---

ถ้ามีคำถามเพิ่มเติม ดูเอกสารอื่น (`PROTOCOL_TH.md`, `API_REFERENCE.md`) ได้เลย ครับ.
