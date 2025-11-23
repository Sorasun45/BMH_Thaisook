# BMH05108 Protocol — API Reference

เอกสารนี้อธิบาย API สาธารณะทั้งหมดของโปรโตคอล BMH05108

---

## **UART Module (`bmh_uart.h`)**

### **`void BMH_UART_begin(uint32_t baud)`**
- **ความหมาย:** เริ่มต้น UART communication ที่ baud rate ที่กำหนด
- **พารามิเตอร์:** 
  - `baud` — baud rate (เช่น 115200)
- **ตัวอย่าง:**
  ```cpp
  BMH_UART_begin(115200);
  ```

### **`void BMH_UART_write(const uint8_t *buf, size_t len)`**
- **ความหมาย:** ส่งข้อมูลออกผ่าน UART
- **พารามิเตอร์:**
  - `buf` — pointer ไปยัง array ข้อมูลที่ต้องการส่ง
  - `len` — จำนวนไบต์
- **ตัวอย่าง:**
  ```cpp
  uint8_t frame[] = {0xAA, 0x05, 0xA0, 0x00, 0xB1};
  BMH_UART_write(frame, 5);
  ```

### **`int BMH_UART_available()`**
- **ความหมาย:** ตรวจสอบจำนวนไบต์ที่พร้อมอ่าน
- **ค่าที่คืน:** จำนวนไบต์ (0 ถ้าไม่มี)
- **ตัวอย่าง:**
  ```cpp
  while (BMH_UART_available()) {
    int b = BMH_UART_read();
    // process byte
  }
  ```

### **`int BMH_UART_read()`**
- **ความหมาย:** อ่าน 1 ไบต์จาก UART
- **ค่าที่คืน:** ไบต์ที่อ่านได้ (0-255) หรือ -1 ถ้าไม่มี
- **ตัวอย่าง:**
  ```cpp
  int byte = BMH_UART_read();
  if (byte >= 0) {
    frame_push_byte((uint8_t)byte);
  }
  ```

### **`void BMH_UART_flush()`**
- **ความหมาย:** ล้างบัฟเฟอร์ UART (รอให้ส่งเสร็จ)
- **ตัวอย่าง:**
  ```cpp
  BMH_UART_write(frame, 5);
  BMH_UART_flush();
  ```

### **`uint32_t BMH_UART_autoBaud(const uint32_t *baudList, size_t nlist, unsigned long timeoutMs)`**
- **ความหมาย:** ลองหา baud rate ที่ถูกต้องโดยอัตโนมัติ
- **พารามิเตอร์:**
  - `baudList` — array ของ baud rates ที่ต้องการลอง (เช่น {115200, 57600, 38400})
  - `nlist` — ขนาด array
  - `timeoutMs` — เวลารอ (ms) สำหรับแต่ละ baud rate
- **ค่าที่คืน:** baud rate ที่พบ หรือ 0 ถ้าไม่พบ
- **ตัวอย่าง:**
  ```cpp
  uint32_t bauds[] = {115200, 57600, 38400};
  uint32_t found = BMH_UART_autoBaud(bauds, 3, 500);
  if (found == 0) {
    Serial.println("Auto-baud failed");
  }
  ```

### **`void BMH_UART_enableSimulator(bool en)` / `bool BMH_UART_isSimulator()`**
- **ความหมาย:** (Optional) toggle simulator mode สำหรับ testing
- **ตัวอย่าง:**
  ```cpp
  BMH_UART_enableSimulator(true); // test mode
  ```

---

## **Frame Module (`bmh_frame.h`)**

### **`struct Frame`**
```cpp
struct Frame {
  bool valid;        // true ถ้าเฟรมสมบูรณ์และ checksum ถูก
  uint8_t raw[512];  // raw bytes ของเฟรม
  int len;           // ความยาวของเฟรม
};
```

### **`void frame_init()`**
- **ความหมาย:** เริ่มต้น frame parser (ต้องเรียกครั้งแรกใน setup)
- **ตัวอย่าง:**
  ```cpp
  frame_init();
  ```

### **`void frame_push_byte(uint8_t b)`**
- **ความหมาย:** เพิ่มไบต์เข้าไปในเฟรม (ต้องเรียกทีละไบต์)
- **พารามิเตอร์:**
  - `b` — ไบต์ที่เพิ่ม
- **หมายเหตุ:** ฟังก์ชันจะรอจนกว่าได้เฟรมสมบูรณ์ (length match)
- **ตัวอย่าง:**
  ```cpp
  while (BMH_UART_available()) {
    frame_push_byte(BMH_UART_read());
  }
  ```

### **`bool frame_has_frame()`**
- **ความหมาย:** ตรวจว่ามีเฟรมสมบูรณ์ที่พร้อมสำหรับการประมวลผล
- **ค่าที่คืน:** true ถ้ามี, false ถ้ายัง
- **ตัวอย่าง:**
  ```cpp
  if (frame_has_frame()) {
    Frame f = frame_pop();
    // process frame
  }
  ```

### **`Frame frame_pop()`**
- **ความหมาย:** คืนเฟรมที่ประกอบเสร็จ และเคลียร์บัฟเฟอร์สำหรับเฟรมถัดไป
- **ค่าที่คืน:** `Frame` struct
- **ตัวอย่าง:**
  ```cpp
  Frame f = frame_pop();
  if (f.valid) {
    process_bmh_frame(f.raw, f.len);
  }
  ```

### **`uint8_t compute_checksum(const uint8_t *buf, int len)`**
- **ความหมาย:** คำนวณ checksum สำหรับเฟรม
- **พารามิเตอร์:**
  - `buf` — array ของเฟรม (รวม checksum byte)
  - `len` — ความยาวของเฟรม
- **ค่าที่คืน:** checksum byte ที่คำนวณได้
- **สูตร:** `checksum = (~sum_of_bytes) + 1`
- **ตัวอย่าง:**
  ```cpp
  uint8_t frame[] = {0xAA, 0x05, 0xA0, 0x00, 0x00};
  frame[4] = compute_checksum(frame, 5);
  // frame[4] = 0xB1
  ```

---

## **Command Dispatch Module (`bmh_cmds.h`)**

### **`void process_bmh_frame(const uint8_t *buf, int len)`**
- **ความหมาย:** ประมวลผลเฟรม (ตรวจ checksum + dispatch ไปยัง handler)
- **พารามิเตอร์:**
  - `buf` — raw bytes ของเฟรม
  - `len` — ความยาว
- **ลักษณะการทำงาน:**
  - ตรวจสอบ checksum ก่อน
  - ถ้า checksum fail → send error JSON
  - ถ้า valid → dispatch ไปยัง handler ตามคำสั่ง (0xA0, 0xA1, ฯลฯ)
- **ตัวอย่าง:**
  ```cpp
  Frame f = frame_pop();
  if (f.valid) {
    process_bmh_frame(f.raw, f.len);
    // JSON ถูกส่งออกโดย handler
  }
  ```

### **`void check_multi_pkg_timeouts(unsigned long now)`**
- **ความหมาย:** ตรวจสอบ timeout สำหรับ multi-packet (D0/D1/D2)
- **พารามิเตอร์:**
  - `now` — timestamp ปัจจุบัน (มิลลิวินาที)
- **ลักษณะการทำงาน:** ถ้า multi-packet ยังไม่ครบแล้วเวลาผ่านไป `MULTI_PKG_TIMEOUT_MS` จะส่ง timeout JSON
- **ตัวอย่าง:**
  ```cpp
  void loop() {
    // ... frame processing ...
    check_multi_pkg_timeouts(millis());
  }
  ```

---

## **JSON Emitter Module (`bmh_json.h`)**

### **ฟังก์ชัน Single-packet Emitters**

#### **`void bmh_json_emit_A0(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง A0 (Query Status)
- **Output:** JSON พร้อม status.code และ status.description

#### **`void bmh_json_emit_A1(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง A1 (Weight)
- **Output:** JSON พร้อม stable_catty_x10, realtime_catty_x10, adc_raw

#### **`void bmh_json_emit_B0(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง B0 (Sensor Calibration)

#### **`void bmh_json_emit_B1(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง B1 (Impedance)
- **Output:** JSON พร้อมค่า impedance/phase สำหรับแต่ละขา

#### **`void bmh_json_emit_10(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง 10 (Real-time Sensor Value)

#### **`void bmh_json_emit_E0(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง E0 (Config Read)

#### **`void bmh_json_emit_E1(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง E1 (Config Set)

#### **`void bmh_json_emit_F0(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง F0 (System Action)

#### **`void bmh_json_emit_FF(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่ง FF (System Error)

### **ฟังก์ชัน Multi-packet Emitters**

#### **`void bmh_json_emit_D0(uint8_t *pkgs[], int lens[], int total)`**
- **ความหมาย:** ส่ง JSON สำหรับ multi-packet D0
- **พารามิเตอร์:**
  - `pkgs[]` — array ของ pointers ไปยังแต่ละแพ็กเกจ
  - `lens[]` — array ของความยาวแต่ละแพ็กเกจ
  - `total` — จำนวนแพ็กเกจทั้งหมด

#### **`void bmh_json_emit_D1(uint8_t *pkgs[], int lens[], int total)`**
- **ความหมาย:** ส่ง JSON สำหรับ multi-packet D1

#### **`void bmh_json_emit_D2(uint8_t *pkgs[], int lens[], int total)`**
- **ความหมาย:** ส่ง JSON สำหรับ multi-packet D2

### **Error Emitters**

#### **`void bmh_json_emit_unknown(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับคำสั่งที่ไม่รู้จัก

#### **`void bmh_json_emit_error_checksum(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่ง JSON สำหรับข้อผิดพลาด checksum

#### **`void bmh_json_emit_multi_timeout(const char *cmd, int received, int expected)`**
- **ความหมาย:** ส่ง JSON สำหรับ timeout ของ multi-packet
- **พารามิเตอร์:**
  - `cmd` — ชื่อคำสั่ง (เช่น "D0")
  - `received` — จำนวนแพ็กเกจที่ได้รับ
  - `expected` — จำนวนแพ็กเกจที่คาดหวัง

---

## **State Module (`bmh_state.h`)**

### **`void state_init()`**
- **ความหมาย:** เริ่มต้น state machine (ต้องเรียกในหลัง frame_init)
- **ตัวอย่าง:**
  ```cpp
  frame_init();
  state_init();
  ```

### **`void state_handle_frame(const uint8_t *buf, int len)`**
- **ความหมาย:** ส่งเฟรมไปให้ state handler เพื่อประมวลผล
- **พารามิเตอร์:**
  - `buf` — raw bytes
  - `len` — ความยาว
- **ลักษณะการทำงาน:** เรียก `process_bmh_frame()` เพื่อ dispatch
- **ตัวอย่าง:**
  ```cpp
  Frame f = frame_pop();
  if (f.valid) {
    state_handle_frame(f.raw, f.len);
  }
  ```

### **`void state_tick()`**
- **ความหมาย:** เรียกงานประจำ (timeout check)
- **ลักษณะการทำงาน:** เรียก `check_multi_pkg_timeouts()`
- **ตัวอย่าง:**
  ```cpp
  void loop() {
    // ... frame processing ...
    state_tick(); // ตรวจ timeout
  }
  ```

---

## **JSON Output Structure**

### **Common Structure (ทั้ง Single และ Multi-packet)**
```json
{
  "timestamp": 12345,
  "device": "BMH05108",
  "frame": {
    "raw_hex": "AA 05 A0 00",
    "length": 4
  },
  "command": {
    "code": "A0",
    "name": "Query Status",
    "category": "device_query"
  },
  "status": {
    "code": 0,
    "ok": true,
    "description": "Success"
  },
  "data": {
    "raw_bytes": "00",
    "raw_array": [0],
    "processed": {
      "status_raw": 0,
      "status_desc": "Success"
    }
  }
}
```

### **Multi-packet Structure (D0/D1/D2)**
```json
{
  "timestamp": 12345,
  "device": "BMH05108",
  "command": {
    "code": "D0",
    "name": "Multi-packet Data",
    "category": "multi_packet"
  },
  "status": {
    "code": 0,
    "ok": true,
    "description": "Success"
  },
  "packages": [
    {
      "idx": 1,
      "raw_hex": "AA 08 D0 21 10 20 30",
      "raw_array": [170, 8, 208, 33, 16, 32, 48],
      "processed": {
        "v00": 5136,
        "v01": 12288
      }
    },
    {
      "idx": 2,
      "raw_hex": "AA 08 D0 22 40 50 60",
      "raw_array": [170, 8, 208, 34, 64, 80, 96],
      "processed": {
        "v00": 20544,
        "v01": 24576
      }
    }
  ]
}
```

---

## **Configuration Constants (จาก `config.h`)**

| ค่าคงที่ | ความหมาย | ค่าเริ่มต้น |
|--------|---------|---------|
| `BMH_UART_NUM` | หมายเลข UART | 2 |
| `BMH_RX_PIN` | RX pin | 16 |
| `BMH_TX_PIN` | TX pin | 17 |
| `BMH_DEFAULT_BAUD` | Baud rate | 115200 |
| `FRAME_MAX` | ขนาดบัฟเฟอร์เฟรม | 512 |
| `MAX_PACKAGES` | แพ็กเกจสูงสุด | 10 |
| `MULTI_PKG_TIMEOUT_MS` | Timeout (ms) | 5000 |
| `JSON_DOC_CAPACITY` | ขนาด JSON | 2048 |

---

## **Endianness และข้อมูล**

ทั้งหมดเป็น **little-endian**:
- 16-bit: `buf[offset]` (low) + `buf[offset+1] << 8` (high)
- 32-bit: `buf[offset]` + `buf[offset+1]<<8` + `buf[offset+2]<<16` + `buf[offset+3]<<24`

Helper functions ใน `bmh_json.cpp`:
```cpp
readU16(buf, offset);  // read unsigned 16-bit little-endian
readS16(buf, offset);  // read signed 16-bit
readU32(buf, offset);  // read unsigned 32-bit
```

---

ดู `PROTOCOL_TH.md` และ `USAGE_EXAMPLE.md` สำหรับรายละเอียดเพิ่มเติม ครับ.
