**BMH05108 Protocol — เอกสารภาษาไทย**

ภาพรวม
- **คำอธิบาย:** โปรโตคอลนี้ใช้สื่อสารกับโมดูล BMH05108 ผ่าน UART โดยข้อมูลถูกส่งเป็นเฟรมไบนารีที่มี header, length, command, payload และ checksum
- **โครงสร้างซอร์ส:** โมดูลที่เกี่ยวข้องในโปรเจคอยู่ที่ `src/` เช่น `bmh_uart.*`, `bmh_frame.*`, `bmh_cmds.*`, `bmh_state.*`, `bmh_json.*` (ไฟล์อ้างอิงในโค้ด)

**รูปแบบเฟรม (Frame Format)**
- **Byte 0 (Start):** `0xAA`  (โค้ด `frame_push_byte()` ใช้ค่าเริ่มต้นนี้เป็น start byte)
- **Byte 1 (Length):** ความยาวทั้งหมดของเฟรมเป็นไบต์ (รวม start, length, payload และ checksum)
- **Byte 2 (Command):** รหัสคำสั่ง (เช่น `0xA0`, `0xA1`)
- **Byte 3..(N-2) (Payload):** ข้อมูลของคำสั่ง (ความยาวขึ้นกับคำสั่ง)
- **Byte N-1 (Checksum):** ค่า checksum แบบสอง's complement: `(~sum_of_bytes[0..N-2]) + 1` (ฟังก์ชัน `compute_checksum()` ใช้วิธีนี้)

หมายเหตุสำคัญ: ตัวอย่างที่ปรากฏใน `main.cpp` คือ `55 05 A0 01 05` ซึ่งใช้ `0x55` เป็น start byte — แต่ในโค้ดจริง `frame_push_byte()` ตรวจสอบ `0xAA` เป็น start byte ทำให้มีความไม่สอดคล้องกัน แนะนำให้เลือกและใช้ค่าเดียวกัน (แนะนำ `0xAA` ตามโค้ดปัจจุบัน) เพื่อหลีกเลี่ยงความสับสน

**การคำนวณ Checksum (ตัวอย่าง)**
- ให้เฟรม (ก่อน checksum): `AA 05 A0 01` (4 ไบต์)
- ผลรวม (sum) ของไบต์ 0..2 คือ `0xAA + 0x05 + 0xA0 + 0x01` (จริง ๆ จะรวมจนถึง N-2)
- checksum = `(~sum) + 1` => ใส่เป็นไบต์สุดท้าย

**รูปแบบข้อมูล (Endianness)**
- ข้อมูลตัวเลขหลายไบต์จะเป็น little-endian (เช่น `readU16`, `readS16`, `readU32` ใน `bmh_json.cpp`) — หมายความว่าไบต์ต่ำก่อนตามด้วยไบต์สูง

**คำสั่งที่รองรับ (Implemented Commands)**
- `0xA0` — Query Status
  - Payload: 1 ไบต์สถานะที่ตำแหน่ง `buf[3]` (ถ้ามี)
  - JSON: `status.code`, `status.description`

- `0xA1` — Weight / A1 Frame
  - คาดหวังความยาว >= 14 ไบต์
  - ตำแหน่งข้อมูล (ตามโค้ด):
    - `stable_catty_x10` : 16-bit signed @ offset 5
    - `realtime_catty_x10` : 16-bit signed @ offset 7
    - `adc` : 32-bit unsigned @ offset 9
  - JSON ให้ `raw_array`, `stable_catty_x10_raw`, `realtime_catty_x10_raw`, `adc_raw`

- `0xB0` — Sensor Calibration
  - Payload: result code ใน `buf[3]`

- `0xB1` — Impedance / B1 Frame
  - สองรูปแบบ:
    - ถ้ายาว >= 27 ไบต์ (ชุดค่า 5 ช่อง): ค่า 32-bit หลายตัวที่ตำแหน่ง 6,10,14,18,22
    - ถ้ายาว >= 13 ไบต์ (4-electrode): phase_x10 (16-bit) @6, impedance (32-bit) @8

- `0x10` — Real-time Sensor Value
  - ตำแหน่ง `sensor_id` ใน `buf[3]` และ value ใน `buf[4..]` (4 ไบต์หรือ 1 ไบต์ตามความยาว)

- `0xE0` — Config Read
  - ถ้ายาว >= 6: type `buf[3]`, version (16-bit) @4

- `0xE1` — Config Set
  - result code ใน `buf[3]`

- `0xF0` — System Action
  - action code ใน `buf[3]`

- `0xFF` — System Error
  - error code ใน `buf[3]`

- Multi-packet: `0xD0`, `0xD1`, `0xD2`
  - ข้อมูลแพ็กเกจหลายชิ้น (multipart)
  - byte `buf[3]` เป็น `pkgInfo` โดย nibble สูง (bits 7..4) = total packages, nibble ต่ำ (bits 3..0) = current index
  - ถ้า `total == 0` ให้ถือเป็น 1
  - เซิร์ฟเวอร์จะจัดเก็บแต่ละแพ็กเกจใน `MultiState` และรวม/emit เมื่อได้ครบ
  - timeout ค่าคงที่: `MULTI_PKG_TIMEOUT_MS` (กำหนดใน `config.h`) — ถ้าหมดเวลาก่อนครบ จะส่งข้อความ timeout ผ่าน JSON

**การตอบกลับเป็น JSON (bmh_json module)**
- ทุกคำสั่งที่ผ่านการตรวจสอบ checksum จะถูกแปลงเป็น JSON แล้วส่งไปที่ `Serial` (ดู `bmh_json_emit_*()`)
- โครงสร้างทั่วไป:
  - `timestamp`: มิลลิวินาทีจาก `millis()`
  - `device`: ชื่ออุปกรณ์ (เช่น `BMH05108`)
  - `frame`: `{ raw_hex, length }` — raw hex ของเฟรม
  - `command`: `{ code, name, category }`
  - `status`: `{ code, ok, description }`
  - `data`: `{ raw_bytes, raw_array, processed: { ... } }`

ตัวอย่าง JSON (A0) — รูปแบบตัวอย่าง
```
{"timestamp":12345,"device":"BMH05108","frame":{"raw_hex":"AA 04 A0 00","length":4},"command":{"code":"A0","name":"Query Status","category":"device_query"},"status":{"code":0,"ok":true,"description":"Success"},"data":{"raw_bytes":"A0 00","raw_array":[160,0],"processed":{"status_raw":0,"status_desc":"Success"}}}
```

ตัวอย่าง JSON (checksum fail)
```
{"timestamp":...,"device":"BMH05108","frame":{"raw_hex":"AA 04 A1 00","length":4},"command":{"code":"ERR","name":"Checksum Fail","category":"error"},"status":{"code":2,"ok":false,"description":"Checksum mismatch"},"data":{"raw_bytes":"AA 04 A1 00"}}
```

**Flow การทำงาน (สรุปการเชื่อมต่อกับโค้ด)**
- `main.cpp`:
  - เริ่มต้น Serial, เลือก baud (มี `BMH_UART_autoBaud()`), เรียก `frame_init()` และ `state_init()`
  - ใน `loop()`: อ่านไบต์จาก `BMH_UART_read()` → `frame_push_byte()`
  - เมื่อ `frame_has_frame()` เป็นจริง: `Frame f = frame_pop()` → `state_handle_frame(f.raw, f.len)`
  - เรียก `state_tick()` เป็นงานประจำเพื่อตรวจ multi-packet timeouts

- `bmh_frame.*`: ประกอบเฟรมจากไบต์เดี่ยว, ตรวจความยาว, คืน `Frame` ที่ประกอบเสร็จ
- `bmh_cmds.*` (`process_bmh_frame()`): ตรวจ checksum และ dispatch ไปยัง handler ตาม `cmd` (รวม multi-packet store/emit)
- `bmh_json.*`: แปลงเฟรมเป็น JSON ที่เป็นมิตรกับคนอ่าน
- `bmh_uart.*`: wrapper สำหรับการสื่อสาร UART (รวม auto-baud)

**ค่าคงที่และการตั้งค่า (จาก `config.h`)**
- `FRAME_MAX` — ขนาดบัฟเฟอร์สูงสุดของเฟรม
- `MAX_PACKAGES` — จำนวนแพ็กเกจสูงสุดของ multi-packet
- `MULTI_PKG_TIMEOUT_MS` — เวลารอให้ครบทุกแพ็กเกจ
- `JSON_DOC_CAPACITY` — ขนาดเอกสารสำหรับ `DynamicJsonDocument`
- `BMH_DEFAULT_BAUD` — ค่า Baud ค่าเริ่มต้น

**ข้อแนะนำเชิงปฏิบัติ/การพัฒนา**
- แก้ความไม่สอดคล้องของ start byte (`0xAA` vs `0x55`) ให้เป็นค่าเดียวกัน
- ระบุในเอกสารเวอร์ชันของโปรโตคอลและตัวอย่างเฟรมสำหรับแต่ละคำสั่ง
- ระบุขนาด payload ที่คาดหวังอย่างชัดเจนสำหรับแต่ละรหัสคำสั่ง เพื่อช่วยการดีบัก
- ตรวจสอบค่า `JSON_DOC_CAPACITY` ว่าเพียงพอสำหรับ multi-packet emiters (ซึ่งรวมหลายแพ็กเกจ)
- พิจารณาใช้ header สาธารณะในโฟลเดอร์ `include/` เพื่อให้ API ชัดเจน

**ไฟล์/ฟังก์ชันอ้างอิงในโค้ด**
- `src/main.cpp` — loop และการเชื่อมต่อโมดูล
- `src/bmh_uart.*` — UART wrapper (`BMH_UART_*`, `BMH_UART_autoBaud`)
- `src/bmh_frame.*` — frame assembly (`frame_init`, `frame_push_byte`, `frame_has_frame`, `frame_pop`, `compute_checksum`)
- `src/bmh_cmds.*` — central dispatch (`process_bmh_frame`, `check_multi_pkg_timeouts`) และการเก็บแพ็กเกจ
- `src/bmh_json.*` — JSON emitter (`bmh_json_emit_*` functions)
- `src/bmh_state.*` — รอบการทำงานของ state machine (`state_handle_frame`, `state_tick`)

**ตัวอย่างเฟรม (ไบต์ตัวอย่าง)**
- หมายเหตุ: ทุกตัวอย่างใช้ `Start = 0xAA` และรูปแบบเฟรมคือ `[Start][Length][Cmd][Payload...][Checksum]` โดย `Length` รวมทุกไบต์ของเฟรม (รวม Start และ Checksum)

1) `0xA0` — Query Status (สถานะปกติ)
- ตัวอย่าง: `AA 05 A0 00 B1`
  - ความหมาย: Start=`0xAA`, Length=`0x05`, Cmd=`0xA0`, Payload=`0x00` (status ok), Checksum=`0xB1`
  - การคำนวณ checksum: sum(AA+05+A0+00)=0x4F -> checksum = (~0x4F)+1 = 0xB1
  - การแมปในโค้ด: `buf[3]` = status byte

2) `0xA1` — Weight / A1 Frame (ตัวอย่าง minimal, ความยาว >= 14 ไบต์)
- ตัวอย่าง: `AA 0E A1 00 00 7B 00 78 00 33 22 11 00 4E`
  - ความหมาย (offset-based):
    - `buf[5..6]` = `0x007B` (stable_catty_x10 = 123)
    - `buf[7..8]` = `0x0078` (realtime_catty_x10 = 120)
    - `buf[9..12]` = `0x00112233` (adc = 0x00112233)
  - checksum คำนวณจากไบต์ 0..12 -> ผล = 0x4E

3) `0xB0` — Sensor Calibration (ผลการคาลิเบรต)
- ตัวอย่าง (failure): `AA 05 B0 01 A0`
  - `buf[3]` = 0x01 (result code)
  - checksum = 0xA0

4) `0xB1` — Impedance / B1 Frame (4-electrode example)
- ตัวอย่าง (4-electrode): `AA 0D B1 00 00 00 FA 00 56 34 12 00 02`
  - `buf[6..7]` = phase_x10 = `0x00FA` (250)
  - `buf[8..11]` = impedance = `0x00123456` (little-endian 56 34 12 00)
  - checksum = 0x02

5) `0x10` — Real-time Sensor Value
- ตัวอย่าง (sensor id + 32-bit value): `AA 08 10 05 64 00 00 00 D5`
  - `buf[3]` = 0x05 (sensor id)
  - `buf[4..7]` = 0x00000064 (value 100)
  - checksum = 0xD5

6) `0xE0` — Config Read (ตัวอย่างที่มีเวอร์ชัน)
- ตัวอย่าง: `AA 07 E0 01 03 02 69`
  - `buf[3]` = type = 0x01
  - `buf[4..5]` = version (16-bit little-endian) = 0x0203
  - checksum = 0x69

7) `0xE1` — Config Set (result)
- ตัวอย่าง: `AA 05 E1 00 70`
  - `buf[3]` = result code (0x00 success)
  - checksum = 0x70

8) `0xF0` — System Action
- ตัวอย่าง: `AA 05 F0 02 5F`
  - `buf[3]` = action code 0x02
  - checksum = 0x5F

9) `0xFF` — System Error
- ตัวอย่าง: `AA 05 FF 03 4F`
  - `buf[3]` = error code 0x03
  - checksum = 0x4F

10) `0xD0` — Multi-packet (ตัวอย่าง 2 แพ็กเกจ)
- packet 1: `AA 08 D0 21 10 20 30 FD`
  - `buf[3]` = 0x21 -> total=2 (high nibble), idx=1 (low nibble)
  - payload bytes = `10 20 30`
  - checksum = 0xFD
- packet 2: `AA 08 D0 22 40 50 60 6C`
  - `buf[3]` = 0x22 -> total=2, idx=2
  - payload bytes = `40 50 60`
  - checksum = 0x6C
  - รหัส `bmh_cmds` จะเก็บแพ็กเกจทั้งสองและรวม/emit เมื่อครบทั้งชุด

11) Checksum fail (ตัวอย่าง)
- ตัวอย่าง: `AA 05 A0 00 B2`  (เหมือน `A0` ปกติ แต่ checksum ผิด)
  - ระบบจะส่ง JSON แบบ error (ดู `bmh_json_emit_error_checksum`)

12) Multi-packet timeout (ตัวอย่างเหตุการณ์)
- ถ้ามีเพียงแพ็กเกจเดียวที่มาพร้อม `pkgInfo` เช่น total=3 แต่ได้รับเพียง 1 แล้วเวลาหมด (`MULTI_PKG_TIMEOUT_MS`) โค้ดจะเรียก `bmh_json_emit_multi_timeout(name, got, total)` ซึ่ง JSON จะระบุ `received_pkgs` และ `expected_pkgs` เพื่อแจ้งเหตุการณ์

