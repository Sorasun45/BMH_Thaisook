# BMH05108 Protocol Documentation

เอกสารโปรโตคอลการสื่อสารกับ BMH05108 Body Composition Analyzer Module

---

## 📋 Table of Contents

1. [ภาพรวม](#ภาพรวม)
2. [โครงสร้าง Frame](#โครงสร้าง-frame)
3. [คำสั่งทั้งหมด](#คำสั่งทั้งหมด)
4. [ขั้นตอนการวัด](#ขั้นตอนการวัด)
5. [ตัวอย่างการใช้งาน](#ตัวอย่างการใช้งาน)

---

## 🎯 ภาพรวม

BMH05108 ใช้โปรโตคอลแบบ UART สำหรับการสื่อสาร:
- **Baud rate**: 115200
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Flow control**: None

---

## 📦 โครงสร้าง Frame

### Standard Frame Format

```
+--------+--------+-------+--------+--------+------...------+----------+
| Header | Length | Order |  Data  |  Data  |     Data     | Checksum |
|  (1B)  |  (1B)  | (1B)  |  (1B)  |  (1B)  |    (N-5)     |   (1B)   |
+--------+--------+-------+--------+--------+------...------+----------+
```

### Field Descriptions

| Field | Size | Description |
|-------|------|-------------|
| Header | 1 byte | เสมอ `0xF5` |
| Length | 1 byte | ความยาวทั้งหมดของ frame (รวม header และ checksum) |
| Order | 1 byte | รหัสคำสั่ง (0xA0, 0xA1, 0xB0, 0xB1, 0xD0) |
| Data | N bytes | ข้อมูลตามคำสั่ง |
| Checksum | 1 byte | XOR ของทุก byte ยกเว้น checksum |

### Checksum Calculation

```cpp
uint8_t computeChecksum(const uint8_t *buf, size_t lenWithoutChecksum) {
  uint8_t chk = 0;
  for (size_t i = 0; i < lenWithoutChecksum; i++) {
    chk ^= buf[i];
  }
  return chk;
}
```

---

## 📡 คำสั่งทั้งหมด

### 1. Command A0 - Handshake

**Purpose**: เริ่มต้นการสื่อสาร

**Frame Structure**:
```
F5 05 A0 00 B1
```

**Response** (ACK):
```
F5 05 A0 00 B1
```

**Usage**:
```cpp
void send_cmd_A0() {
  uint8_t body[] = { 0xA0, 0x00, 0xB1 };
  buildAndSend(body, 3);
}
```

---

### 2. Command A1 - Read Weight

**Purpose**: อ่านค่าน้ำหนักแบบ real-time

**Frame Structure**:
```
F5 05 A1 00 B0
```

**Response**:
```
F5 0E A1 00 00 00 [WEIGHT_2B] 00 [ADC_4B] [CHECKSUM]
```

**Response Fields**:
- Byte 7-8: Weight (int16, little-endian, unit: 0.1 kg)
- Byte 9: Reserved (0x00)
- Byte 10-13: ADC value (uint32, little-endian)

**Example Response**:
```
F5 0E A1 00 00 00 00 B4 02 00 AC F1 80 00 XX
```
- Weight = 0x02B4 = 692 → 69.2 kg
- ADC = 0x0080F1AC = 8450476

**Usage**:
```cpp
void send_cmd_A1() {
  uint8_t body[] = { 0xA1, 0x00, 0xB0 };
  buildAndSend(body, 3);
}

// Parse response
int16_t weight = le_i16(&frame[7]);  // 0.1 kg units
uint32_t adc = le_u32(&frame[10]);
float weight_kg = weight / 10.0f;
```

---

### 3. Command B0 - Start Impedance Measurement

**Purpose**: เริ่มการวัด impedance ที่ความถี่ต่างๆ

**Type 1: 20kHz (mode 03)**
```
F5 07 B0 00 03 03 B5
```

**Type 2: 100kHz (mode 01)**
```
F5 07 B0 00 01 06 B3
```

**Response** (ACK):
```
F5 05 B0 00 A1
```

**Mode Values**:
- `0x03`: วัดที่ 20 kHz
- `0x01`: วัดที่ 100 kHz

**Usage**:
```cpp
void send_cmd_B0_len6_0303() {
  // Start 20kHz measurement
  uint8_t body[] = { 0xB0, 0x00, 0x03, 0x03 };
  buildAndSend(body, 4);
}

void send_cmd_B0_len6_0106() {
  // Start 100kHz measurement
  uint8_t body[] = { 0xB0, 0x00, 0x01, 0x06 };
  buildAndSend(body, 4);
}
```

---

### 4. Command B1 - Read Impedance

**Purpose**: อ่านค่า impedance แบบ real-time

**Frame Structure**:
```
F5 05 B1 00 A0
```

**Response**:
```
F5 1B B1 00 [STATE] 00 [RH_4B] [LH_4B] [TR_4B] [RF_4B] [LF_4B] [CHECKSUM]
```

**Response Fields**:
- Byte 4: State (0x00=not ready, 0x03=ready)
- Byte 6-9: Right Hand impedance (uint32, little-endian, unit: 0.1Ω)
- Byte 10-13: Left Hand impedance
- Byte 14-17: Trunk impedance
- Byte 18-21: Right Foot impedance
- Byte 22-25: Left Foot impedance

**Example Response**:
```
F5 1B B1 00 03 00 [RH] [LH] [TR] [RF] [LF] XX
```

**Usage**:
```cpp
void send_cmd_B1() {
  uint8_t body[] = { 0xB1, 0x00, 0xA0 };
  buildAndSend(body, 3);
}

// Parse response
uint8_t state = frame[4];
if (state == 0x03) {
  uint32_t rh = le_u32(&frame[6]);   // Right hand
  uint32_t lh = le_u32(&frame[10]);  // Left hand
  uint32_t tr = le_u32(&frame[14]);  // Trunk
  uint32_t rf = le_u32(&frame[18]);  // Right foot
  uint32_t lf = le_u32(&frame[22]);  // Left foot
  
  // Convert to ohms
  float rh_ohm = rh * 0.1f;
}
```

---

### 5. Command D0 - Send Final Data

**Purpose**: ส่งข้อมูลผู้ใช้และค่าวัดทั้งหมดเพื่อคำนวณผล

**Frame Structure**:
```
F5 39 D0 [GENDER] [PRODUCT_ID] [HEIGHT_2B] [AGE] [WEIGHT_4B] 
   [IMP_20k_5x4B] [IMP_100k_5x4B] [CHECKSUM]
```

**Field Details**:
- Byte 3: Gender (0=Female, 1=Male)
- Byte 4: Product ID (0=Normal, 1=Athlete, 2=Child)
- Byte 5-6: Height (uint16, little-endian, unit: cm)
- Byte 7: Age (uint8, unit: years)
- Byte 8-11: Weight (uint32, little-endian, unit: 0.1 kg)
- Byte 12-31: 20kHz impedance (5 values × 4 bytes)
  - RH, LH, Trunk, RF, LF
- Byte 32-51: 100kHz impedance (5 values × 4 bytes)
  - RH, LH, Trunk, RF, LF

**Usage**:
```cpp
void buildAndSendFinalPacket(UserInfo &user, MeasurementData &mData) {
  uint8_t buf[256];
  int idx = 0;
  
  buf[idx++] = 0xF5;                    // Header
  buf[idx++] = 57;                      // Length (0x39)
  buf[idx++] = 0xD0;                    // Order
  buf[idx++] = user.gender;             // Gender
  buf[idx++] = user.product_id;         // Product ID
  
  // Height (little-endian)
  buf[idx++] = (user.height & 0xFF);
  buf[idx++] = (user.height >> 8) & 0xFF;
  
  buf[idx++] = user.age;                // Age
  
  // Weight (little-endian, 4 bytes)
  uint32_t w = (uint32_t)mData.weight_final;
  buf[idx++] = (w & 0xFF);
  buf[idx++] = (w >> 8) & 0xFF;
  buf[idx++] = (w >> 16) & 0xFF;
  buf[idx++] = (w >> 24) & 0xFF;
  
  // 20kHz impedance (5 values)
  writeU32LE(&buf[idx], mData.imp_20k.rh);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_20k.lh);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_20k.trunk); idx += 4;
  writeU32LE(&buf[idx], mData.imp_20k.rf);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_20k.lf);    idx += 4;
  
  // 100kHz impedance (5 values)
  writeU32LE(&buf[idx], mData.imp_100k.rh);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_100k.lh);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_100k.trunk); idx += 4;
  writeU32LE(&buf[idx], mData.imp_100k.rf);    idx += 4;
  writeU32LE(&buf[idx], mData.imp_100k.lf);    idx += 4;
  
  // Checksum
  buf[idx] = computeChecksum(buf, idx);
  idx++;
  
  sendRaw(buf, idx);
}
```

---

### 6. Result Packets (E1-E5)

**Purpose**: รับผลการคำนวณจาก BMH module

Module จะส่งผลลัพธ์กลับมา 5 packets:

#### Packet E1 - Body Composition
```
F5 66 E1 00 [DATA...] [CHECKSUM]
```

Contains:
- Weight, Moisture, Body Fat Mass, Protein Mass, etc.

#### Packet E2 - Segmental Fat
```
F5 39 E2 00 [DATA...] [CHECKSUM]
```

Contains:
- Fat mass and fat percentage for 5 body segments

#### Packet E3 - Segmental Muscle
```
F5 39 E3 00 [DATA...] [CHECKSUM]
```

Contains:
- Muscle mass and muscle ratio for 5 body segments

#### Packet E4 - Health Metrics
```
F5 47 E4 00 [DATA...] [CHECKSUM]
```

Contains:
- Body score, Physical age, BMI, BMR, etc.

#### Packet E5 - Energy Consumption
```
F5 2B E5 00 [DATA...] [CHECKSUM]
```

Contains:
- Calorie consumption for various activities

**Usage**:
```cpp
void processDeviceFrame(const uint8_t *frame, size_t frameLen) {
  uint8_t order = frame[2];
  
  switch (order) {
    case 0xE1:
      parseE1_BodyComposition(frame, frameLen, resultData);
      break;
    case 0xE2:
      parseE2_SegmentalFat(frame, frameLen, resultData);
      break;
    case 0xE3:
      parseE3_SegmentalMuscle(frame, frameLen, resultData);
      break;
    case 0xE4:
      parseE4_HealthMetrics(frame, frameLen, resultData);
      break;
    case 0xE5:
      parseE5_EnergyConsumption(frame, frameLen, resultData);
      break;
  }
}
```

---

## 🔄 ขั้นตอนการวัด

### Complete Measurement Sequence

```
┌─────────────────┐
│  1. Send A0     │  Handshake
│  Wait ACK       │
└────────┬────────┘
         │
┌────────▼────────┐
│  2. Send A1     │  Tare calibration
│  Loop 10 times  │  (empty scale)
└────────┬────────┘
         │
┌────────▼────────┐
│  3. Send A1     │  Wait for user
│  Loop until     │  to step on scale
│  weight > 15kg  │
└────────┬────────┘
         │
┌────────▼────────┐
│  4. Send A1     │  Measure weight
│  Loop until     │  until stable
│  stable (5x)    │
└────────┬────────┘
         │
┌────────▼────────┐
│  5. Send B0     │  Start impedance
│  (mode 0x03)    │  measurement 20kHz
│  Wait ACK       │
└────────┬────────┘
         │
┌────────▼────────┐
│  6. Send B1     │  Read impedance
│  Loop until     │  until stable
│  state=0x03     │
└────────┬────────┘
         │
┌────────▼────────┐
│  7. Send B0     │  Start impedance
│  (mode 0x01)    │  measurement 100kHz
│  Wait ACK       │
└────────┬────────┘
         │
┌────────▼────────┐
│  8. Send B1     │  Read impedance
│  Loop until     │  until stable
│  state=0x03     │
└────────┬────────┘
         │
┌────────▼────────┐
│  9. Send D0     │  Send all data
│                 │  for calculation
└────────┬────────┘
         │
┌────────▼────────┐
│ 10. Receive     │  Receive 5 result
│     E1-E5       │  packets
└─────────────────┘
```

### Timing Requirements

| Phase | Interval | Notes |
|-------|----------|-------|
| A1 polling (tare) | 300ms | Fast for tare |
| A1 polling (weight) | 300ms | Real-time weight |
| B1 polling (impedance) | 500ms | Normal polling |
| Timeout | 30s | Per measurement phase |

---

## 💡 ตัวอย่างการใช้งาน

### Example 1: Simple Weight Reading

```cpp
#include <HardwareSerial.h>

HardwareSerial BMH(2);

void setup() {
  Serial.begin(115200);
  BMH.begin(115200, SERIAL_8N1, 16, 17);  // RX=16, TX=17
  
  // Send handshake
  uint8_t a0[] = {0xF5, 0x05, 0xA0, 0x00, 0xB1};
  BMH.write(a0, 5);
  delay(100);
  
  // Wait for ACK
  while (!BMH.available()) delay(10);
  
  // Read response
  uint8_t response[32];
  int len = BMH.readBytes(response, 32);
  
  Serial.println("Handshake complete!");
}

void loop() {
  // Send A1 command
  uint8_t a1[] = {0xF5, 0x05, 0xA1, 0x00, 0xB0};
  BMH.write(a1, 5);
  
  delay(100);
  
  // Read weight
  if (BMH.available() >= 14) {
    uint8_t frame[14];
    BMH.readBytes(frame, 14);
    
    if (frame[0] == 0xF5 && frame[2] == 0xA1) {
      int16_t weight_raw = frame[7] | (frame[8] << 8);
      float weight_kg = weight_raw / 10.0f;
      
      Serial.printf("Weight: %.1f kg\n", weight_kg);
    }
  }
  
  delay(300);
}
```

### Example 2: Full Measurement with State Machine

```cpp
enum State {
  SEND_A0,
  WAIT_FOR_WEIGHT,
  MEASURE_WEIGHT,
  START_IMPEDANCE,
  MEASURE_IMPEDANCE,
  SEND_DATA,
  WAIT_RESULTS
};

State currentState = SEND_A0;

void processStateMachine() {
  switch (currentState) {
    case SEND_A0:
      send_cmd_A0();
      if (ack_received) {
        currentState = WAIT_FOR_WEIGHT;
      }
      break;
      
    case WAIT_FOR_WEIGHT:
      send_cmd_A1();
      if (weight > 15.0) {
        currentState = MEASURE_WEIGHT;
      }
      break;
      
    case MEASURE_WEIGHT:
      send_cmd_A1();
      if (weight_stable) {
        currentState = START_IMPEDANCE;
      }
      break;
      
    case START_IMPEDANCE:
      send_cmd_B0_20kHz();
      if (ack_received) {
        currentState = MEASURE_IMPEDANCE;
      }
      break;
      
    case MEASURE_IMPEDANCE:
      send_cmd_B1();
      if (impedance_stable) {
        if (frequency == 20) {
          frequency = 100;
          currentState = START_IMPEDANCE;
        } else {
          currentState = SEND_DATA;
        }
      }
      break;
      
    case SEND_DATA:
      send_cmd_D0();
      currentState = WAIT_RESULTS;
      break;
      
    case WAIT_RESULTS:
      if (all_packets_received()) {
        displayResults();
        currentState = SEND_A0;  // Ready for next measurement
      }
      break;
  }
}
```

---

## 🛠️ Utilities

### Little-Endian Parsers

```cpp
uint16_t le_u16(const uint8_t *buf) {
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

int16_t le_i16(const uint8_t *buf) {
  return (int16_t)le_u16(buf);
}

uint32_t le_u32(const uint8_t *buf) {
  return (uint32_t)buf[0] 
       | ((uint32_t)buf[1] << 8)
       | ((uint32_t)buf[2] << 16)
       | ((uint32_t)buf[3] << 24);
}

void writeU32LE(uint8_t *buf, uint32_t val) {
  buf[0] = (val & 0xFF);
  buf[1] = (val >> 8) & 0xFF;
  buf[2] = (val >> 16) & 0xFF;
  buf[3] = (val >> 24) & 0xFF;
}
```

---

## 📖 References

- BMH05108 Datasheet
- BIA (Bioelectrical Impedance Analysis) Principles
- UART Communication Protocol

---

## 📞 Support

For technical support or questions about the protocol:
- GitHub: [BMH_Thaisook Issues](https://github.com/Sorasun45/BMH_Thaisook/issues)

---

**Version**: 1.0  
**Last Updated**: November 2025
