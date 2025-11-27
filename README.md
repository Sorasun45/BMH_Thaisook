# BMH Scale Project - ESP32 BLE Body Composition Analyzer

[![Platform](https://img.shields.io/badge/platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/framework-Arduino-00979D.svg)](https://www.arduino.cc/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

โปรเจคระบบชั่งวิเคราะห์องค์ประกอบร่างกาย BMH05108 ด้วย ESP32 และ BLE สำหรับเชื่อมต่อกับแอปพลิเคชัน Flutter

## 📋 สารบัญ

- [ภาพรวม](#ภาพรวม)
- [คุณสมบัติ](#คุณสมบัติ)
- [สถาปัตยกรรม](#สถาปัตยกรรม)
- [ฮาร์ดแวร์](#ฮาร์ดแวร์)
- [การติดตั้ง](#การติดตั้ง)
- [การใช้งาน](#การใช้งาน)
- [โปรโตคอลการสื่อสาร](#โปรโตคอลการสื่อสาร)
- [Flutter Integration](#flutter-integration)
- [API Reference](#api-reference)

---

## 🎯 ภาพรวม

โปรเจคนี้เป็นระบบ firmware สำหรับ ESP32 ที่ทำหน้าที่เป็น BLE Gateway เชื่อมต่อระหว่าง:
- **BMH05108 Module**: โมดูลวัดน้ำหนักและ impedance แบบ 8 จุดวัด
- **Flutter Mobile App**: แอปพลิเคชันบนมือถือสำหรับรับข้อมูลและแสดงผล

ระบบใช้ State Machine เพื่อควบคุมขั้นตอนการวัด และส่งข้อมูลแบบ real-time ผ่าน BLE

---

## ✨ คุณสมบัติ

### การวัดและวิเคราะห์
- ✅ วัดน้ำหนักแบบ real-time พร้อมแสดงความเสถียร
- ✅ วัด Bioelectrical Impedance Analysis (BIA) 8 จุด
- ✅ คำนวณองค์ประกอบร่างกาย 15+ ค่า
- ✅ วิเคราะห์แยกส่วนของร่างกาย (Segmental Analysis)
- ✅ คำนวณคุณค่าทางสุขภาพ (BMI, BMR, Body Score, etc.)

### การสื่อสาร BLE
- 🔵 **Auto-bonding**: จับคู่อุปกรณ์อัตโนมัติครั้งแรก
- 🔵 **Auto-reconnect**: เชื่อมต่อใหม่อัตโนมัติเมื่อหลุด
- 🔵 **Real-time data**: ส่งค่าน้ำหนักแบบ real-time ขณะวัด
- 🔵 **Status notifications**: แจ้งสถานะการวัดทุกขั้นตอน
- 🔵 **JSON protocol**: ใช้ JSON สำหรับความง่ายในการ integrate

### ระบบควบคุม
- 🎮 State Machine ควบคุมขั้นตอนการวัด 9 states
- 🎮 Tare calibration อัตโนมัติก่อนวัดทุกครั้ง
- 🎮 Stability detection สำหรับทั้งน้ำหนักและ impedance
- 🎮 Error handling และ timeout protection

---

## 🏗️ สถาปัตยกรรม

### State Machine Flow

```
WAIT_JSON → SEND_A0_WAIT_ACK → TARE_WEIGHT → WAIT_FOR_WEIGHT
    ↓
SEND_A1_LOOP → SEND_B0_WAIT_ACK → SEND_B1_LOOP → SEND_B0_2_WAIT_ACK
    ↓
SEND_B1_LOOP2 → BUILD_AND_SEND_FINAL → WAIT_RESULT_PACKETS
    ↓
DONE → WAIT_SCALE_EMPTY → WAIT_JSON (loop)
```

### โครงสร้างโปรเจค

```
BMH_Project/
├── include/                    # Header files
│   ├── ble_handler.h          # BLE communication
│   ├── buffer.h               # UART buffer management
│   ├── calibration.h          # Weight calibration
│   ├── config.h               # Configuration constants
│   ├── measurement.h          # Measurement logic
│   ├── protocol.h             # BMH protocol
│   ├── state_machine.h        # State machine
│   └── types.h                # Data structures
├── src/                       # Source files
│   ├── ble_handler.cpp        # BLE implementation
│   ├── buffer.cpp             # Buffer management
│   ├── calibration.cpp        # Calibration logic
│   ├── main.cpp               # Main program
│   ├── measurement.cpp        # Measurement processing
│   ├── protocol.cpp           # Protocol implementation
│   └── state_machine.cpp      # State machine logic
├── flutter_example/           # Flutter example code
│   ├── bmh_scale_service.dart # BLE service class
│   ├── main.dart              # Example app UI
│   └── README.md              # Flutter guide
├── platformio.ini             # PlatformIO configuration
├── BLE_FLUTTER_GUIDE.md       # Complete BLE guide
└── README.md                  # This file
```

---

## 🔌 ฮาร์ดแวร์

### ESP32 Connections

```
ESP32              BMH05108 Module
GPIO 16 (RX2) ←──→ TX
GPIO 17 (TX2) ←──→ RX
GND           ←──→ GND
5V            ←──→ VCC
```

### Requirements
- **ESP32 DevKit** (ESP32-WROOM-32)
- **BMH05108 Module** (Body composition analyzer)
- **USB Cable** for power and programming
- **5V Power Supply** (recommended for stable operation)

---

## 📦 การติดตั้ง

### Prerequisites
- [PlatformIO](https://platformio.org/) (or PlatformIO IDE extension for VS Code)
- USB drivers for ESP32

### Installation Steps

1. **Clone repository**
```bash
git clone https://github.com/Sorasun45/BMH_Thaisook.git
cd BMH_Thaisook
```

2. **Open in PlatformIO**
```bash
cd BMH_Project
pio run
```

3. **Upload to ESP32**
```bash
pio run --target upload
```

4. **Monitor Serial Output** (optional)
```bash
pio device monitor
```

### Dependencies (Auto-installed by PlatformIO)
- `ArduinoJson` ^6.21.0 - JSON parsing
- Arduino-ESP32 framework

---

## 🚀 การใช้งาน

### ทดสอบผ่าน Serial Monitor

1. เปิด Serial Monitor (115200 baud)
2. ส่ง JSON ข้อมูลผู้ใช้:
```json
{"gender":1,"product_id":0,"height":168,"age":23}
```
3. ขึ้นชั่งและจับ handles
4. รอผลลัพธ์ประมาณ 15-20 วินาที

### การใช้งานผ่าน Flutter App

ดูคู่มือโดยละเอียดที่: [BLE_FLUTTER_GUIDE.md](BLE_FLUTTER_GUIDE.md)

**Quick Start:**
1. เพิ่ม dependencies ใน `pubspec.yaml`
2. ใช้ `BMHScaleService` class (ดูตัวอย่างใน `flutter_example/`)
3. เชื่อมต่อผ่าน BLE
4. ส่งข้อมูลผู้ใช้และรอผลลัพธ์

---

## 📡 โปรโตคอลการสื่อสาร

### BLE Configuration

**Device Name:** `BMH_Scale`

**Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`

**Characteristics:**
- **RX (Write):** `beb5483e-36e1-4688-b7f5-ea07361b26a8` - รับข้อมูลจากแอป
- **TX (Notify):** `beb5483f-36e1-4688-b7f5-ea07361b26a8` - ส่งข้อมูลไปแอป

### Message Types

#### 1. Input: User Data (App → ESP32)
```json
{
  "gender": 1,
  "product_id": 0,
  "height": 168,
  "age": 23
}
```

| Field | Type | Description | Values |
|-------|------|-------------|--------|
| gender | int | เพศ | 0=หญิง, 1=ชาย |
| product_id | int | ประเภทผู้ใช้ | 0=ปกติ, 1=นักกีฬา, 2=เด็ก |
| height | int | ส่วนสูง (ซม.) | 100-220 |
| age | int | อายุ (ปี) | 10-99 |

#### 2. Output: Real-time Weight (ESP32 → App)
```json
{
  "type": "weight_realtime",
  "weight": 65.42,
  "stable_count": 3
}
```

#### 3. Output: Weight Finalized (ESP32 → App)
```json
{
  "type": "weight_finalized",
  "weight": 65.5,
  "status": "starting_impedance_measurement"
}
```

#### 4. Output: Final Results (ESP32 → App)
```json
{
  "status": "success",
  "total_packets": 5,
  "received_packets": 5,
  "body_composition": {
    "weight_kg": 80.4,
    "weight_std_min_kg": 52.8,
    "weight_std_max_kg": 71.4,
    "moisture_kg": 43.8,
    "moisture_std_min_kg": 34.8,
    "moisture_std_max_kg": 42.6,
    "body_fat_mass_kg": 20.6,
    "body_fat_std_min_kg": 7.5,
    "body_fat_std_max_kg": 15.0,
    "protein_mass_kg": 11.8,
    "protein_std_min_kg": 9.3,
    "protein_std_max_kg": 11.4,
    "inorganic_salt_kg": 4.2,
    "inorganic_std_min_kg": 3.2,
    "inorganic_std_max_kg": 3.9,
    "lean_body_weight_kg": 59.8,
    "lean_body_std_min_kg": 45.3,
    "lean_body_std_max_kg": 56.4,
    "muscle_mass_kg": 55.6,
    "muscle_std_min_kg": 44.8,
    "muscle_std_max_kg": 61.6,
    "bone_mass_kg": 3.3,
    "bone_std_min_kg": 2.7,
    "bone_std_max_kg": 3.3,
    "skeletal_muscle_kg": 33.9,
    "skeletal_std_min_kg": 26.4,
    "skeletal_std_max_kg": 32.3,
    "intracellular_water_kg": 27.5,
    "ic_water_std_min_kg": 21.6,
    "ic_water_std_max_kg": 26.5,
    "extracellular_water_kg": 16.2,
    "ec_water_std_min_kg": 13.3,
    "ec_water_std_max_kg": 16.2,
    "body_cell_mass_kg": 39.4,
    "bcm_std_min_kg": 31.0,
    "bcm_std_max_kg": 37.9,
    "subcutaneous_fat_mass_kg": 18.3
  },
  "segmental_analysis": {
    "fat_mass_kg": {
      "right_hand": 1.2,
      "left_hand": 1.2,
      "trunk": 10.6,
      "right_foot": 2.7,
      "left_foot": 2.8
    },
    "fat_percent": {
      "right_hand": 240.0,
      "left_hand": 240.0,
      "trunk": 271.7,
      "right_foot": 180.0,
      "left_foot": 186.6
    },
    "muscle_mass_kg": {
      "right_hand": 3.0,
      "left_hand": 3.1,
      "trunk": 26.5,
      "right_foot": 9.6,
      "left_foot": 9.3
    },
    "muscle_ratio_percent": {
      "right_hand": 100.0,
      "left_hand": 103.3,
      "trunk": 106.8,
      "right_foot": 111.6,
      "left_foot": 108.1
    }
  },
  "health_metrics": {
    "body_score": 76,
    "physical_age": 28,
    "body_type": 5,
    "body_type_name": "Fat muscular type",
    "smi": 8.8,
    "whr": 0.86,
    "whr_std_min": 0.80,
    "whr_std_max": 0.90,
    "visceral_fat": 8,
    "vf_std_min": 1,
    "vf_std_max": 9,
    "obesity_percent": 12.9,
    "obesity_std_min": 9.0,
    "obesity_std_max": 11.0,
    "bmi": 28.4,
    "bmi_std_min": 18.5,
    "bmi_std_max": 23.0,
    "body_fat_percent": 25.6,
    "body_fat_std_min": 10.0,
    "body_fat_std_max": 20.0,
    "bmr_kcal": 1661,
    "bmr_std_min_kcal": 1699,
    "bmr_std_max_kcal": 1994,
    "recommended_intake_kcal": 2159,
    "ideal_weight_kg": 62.1,
    "target_weight_kg": 70.4,
    "weight_control_kg": -10.0,
    "muscle_control_kg": 0.0,
    "fat_control_kg": -10.0,
    "subcutaneous_fat_percent": 22.7,
    "subq_std_min": 8.6,
    "subq_std_max": 16.7
  },
  "energy_consumption_kcal_per_30min": {
    "walk": 160,
    "golf": 141,
    "croquet": 152,
    "tennis_cycling_basketball": 241,
    "squash_tkd_fencing": 402,
    "mountain_climbing": 262,
    "swimming_aerobic_jog": 281,
    "badminton_table_tennis": 181
  },
  "segmental_standards": {
    "fat_standard": {
      "right_hand": "high",
      "left_hand": "high",
      "trunk": "high",
      "right_foot": "high",
      "left_foot": "high"
    },
    "muscle_standard": {
      "right_hand": "normal",
      "left_hand": "normal",
      "trunk": "normal",
      "right_foot": "high",
      "left_foot": "normal"
    }
  }
}
```

ดูรายละเอียดโครงสร้าง JSON ทั้งหมดที่: [BLE_FLUTTER_GUIDE.md](BLE_FLUTTER_GUIDE.md#2-receiving-results-esp32--flutter)

---

## 📱 Flutter Integration

### Quick Example

```dart
import 'bmh_scale_service.dart';

// Initialize
final service = BMHScaleService();

// Set up callbacks
service.onWeightUpdate = (weight, stableCount) {
  print('Weight: $weight kg (stable: $stableCount/5)');
};

service.onImpedanceStart = () {
  print('Starting impedance measurement...');
};

service.onResultReceived = (result) {
  if (result['status'] == 'success') {
    print('Weight: ${result['body_composition']['weight_kg']} kg');
    print('Body Fat: ${result['health_metrics']['body_fat_percent']}%');
  }
};

// Connect and measure
await service.initialize();
await service.sendUserData(
  gender: 1,
  productId: 0,
  height: 168,
  age: 23,
);
```

ตัวอย่างแอปสมบูรณ์: [flutter_example/](flutter_example/)

---

## 🔧 API Reference

### State Machine

#### States
- `WAIT_JSON` - รอข้อมูลผู้ใช้
- `SEND_A0_WAIT_ACK` - Handshake กับ BMH module
- `TARE_WEIGHT` - สอบเทียบค่า zero
- `WAIT_FOR_WEIGHT` - รอให้ขึ้นชั่ง
- `SEND_A1_LOOP` - วัดน้ำหนักจนเสถียร
- `SEND_B0_WAIT_ACK` - เริ่มวัด impedance 20kHz
- `SEND_B1_LOOP` - วัด impedance 20kHz
- `SEND_B0_2_WAIT_ACK` - เริ่มวัด impedance 100kHz
- `SEND_B1_LOOP2` - วัด impedance 100kHz
- `BUILD_AND_SEND_FINAL` - ส่งข้อมูลไปยัง BMH module
- `WAIT_RESULT_PACKETS` - รอผลลัพธ์
- `DONE` - เสร็จสิ้น
- `WAIT_SCALE_EMPTY` - รอลงจากชั่ง

### Configuration (config.h)

```cpp
// UART Configuration
#define BMH_BAUD 115200
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17

// Measurement Thresholds
#define MIN_WEIGHT_TO_START 15.0f    // kg
#define MAX_WEIGHT_EMPTY 5.0f        // kg
#define STABLE_DELTA 5               // units
#define STABLE_REQUIRED_CNT 5        // samples
#define TARE_SAMPLES 10              // samples

// Timing
#define POLL_INTERVAL_MS 500         // ms
#define WEIGHT_POLL_INTERVAL_MS 300  // ms
```

### Calibration

น้ำหนักถูกคำนวณจาก ADC โดยใช้สูตร:
```cpp
weight_kg = (ADC_value - offset - tare_offset) * scale_factor
```

ค่า calibration เก็บใน NVS:
- `offset`: ADC offset (default: 8388608)
- `scale_factor`: kg per ADC unit (default: 0.000022f)

---

## 🐛 Troubleshooting

### ปัญหา: ไม่สามารถ compile ได้
- ตรวจสอบว่าติดตั้ง PlatformIO แล้ว
- ลองลบ `.pio` folder และ build ใหม่
- ตรวจสอบ `platformio.ini` มี dependencies ครบ

### ปัญหา: Upload ไม่ได้
- กด Boot button บน ESP32 ค้างไว้ขระ upload
- ตรวจสอบ USB driver สำหรับ ESP32
- ลองเปลี่ยน USB cable หรือ port

### ปัญหา: แอปหา BMH_Scale ไม่เจอ
- ตรวจสอบ ESP32 มีไฟเลี้ยงอยู่
- ดู Serial Monitor ว่า BLE เริ่มทำงานหรือไม่
- ลอง restart ESP32

### ปัญหา: น้ำหนักไม่แม่นยำ
- ตรวจสอบการเชื่อมต่อกับ BMH module
- ปรับค่า calibration ใน `calibration.cpp`
- ทำ tare calibration ใหม่ (ยืนลงจากชั่ง)

### ปัญหา: ไม่ได้รับผลลัพธ์
- ตรวจสอบข้อมูลผู้ใช้ถูกต้อง (อายุ, ส่วนสูง)
- ต้องขึ้นชั่งและจับ handles ให้เรียบร้อย
- ดู error code ที่ส่งกลับมา

---

## 📖 เอกสารเพิ่มเติม

- **[BLE_FLUTTER_GUIDE.md](BLE_FLUTTER_GUIDE.md)** - คู่มือการใช้งาน BLE และ Flutter แบบละเอียด
- **[flutter_example/README.md](flutter_example/README.md)** - วิธีใช้ Flutter example app
- **[BMH Protocol Specification](docs/BMH_PROTOCOL.md)** - รายละเอียด protocol ของ BMH05108

---

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

---

## 👥 Authors

- **Sorasun45** - *Initial work* - [GitHub](https://github.com/Sorasun45)

---

## 🙏 Acknowledgments

- BMH05108 Module Documentation
- ESP32 Arduino Core
- Flutter Blue Plus community
- PlatformIO team

---

## 📞 Contact

สำหรับคำถามหรือข้อเสนอแนะ:
- GitHub Issues: [BMH_Thaisook/issues](https://github.com/Sorasun45/BMH_Thaisook/issues)
- Repository: [BMH_Thaisook](https://github.com/Sorasun45/BMH_Thaisook)
