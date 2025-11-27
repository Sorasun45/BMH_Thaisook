# BLE Integration Guide for Flutter App

## Overview
ESP32 BMH Scale ใช้ BLE (Bluetooth Low Energy) สำหรับการสื่อสารกับแอปพลิเคชัน Flutter โดยมีคุณสมบัติ:
- **Auto-bonding**: เชื่อมต่อครั้งแรกแล้วจำอุปกรณ์ไว้ตลอด
- **Always ready**: เครื่องชั่งรอรับคำสั่งจากแอปตลอดเวลา (เมื่อมีไฟเลี้ยง)
- **JSON communication**: ใช้ JSON format ทั้งส่งและรับ

---

## BLE Configuration

### Device Information
- **Device Name**: `BMH_Scale`
- **Service UUID**: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`

### Characteristics
1. **RX Characteristic** (Write - สำหรับส่งข้อมูลไปยัง ESP32)
   - UUID: `beb5483e-36e1-4688-b7f5-ea07361b26a8`
   - Properties: WRITE
   - Use: ส่ง JSON ข้อมูลผู้ใช้

2. **TX Characteristic** (Notify - สำหรับรับข้อมูลจาก ESP32)
   - UUID: `beb5483f-36e1-4688-b7f5-ea07361b26a8`
   - Properties: NOTIFY
   - Use: รับผลลัพธ์การวิเคราะห์

---

## Communication Protocol

### 1. Sending User Data (Flutter → ESP32)

ส่ง JSON ผ่าน RX Characteristic:

```json
{
  "gender": 1,
  "product_id": 0,
  "height": 168,
  "age": 23
}
```

**Field Descriptions:**
- `gender`: เพศ (0=หญิง, 1=ชาย)
- `product_id`: ประเภทผู้ใช้ (0=ปกติ, 1=นักกีฬา, 2=เด็ก)
- `height`: ส่วนสูง (ซม.)
- `age`: อายุ (ปี)

### 2. Receiving Results (ESP32 → Flutter)

รับ JSON ผ่าน TX Characteristic (Notify):

#### Success Response:
```json
{
  "status": "success",
  "total_packets": 5,
  "received_packets": 5,
  "body_composition": {
    "weight_kg": 70.5,
    "weight_std_min_kg": 65.0,
    "weight_std_max_kg": 75.0,
    "moisture_kg": 42.3,
    "body_fat_mass_kg": 15.2,
    "protein_mass_kg": 12.5,
    "inorganic_salt_kg": 3.8,
    "lean_body_weight_kg": 55.3,
    "muscle_mass_kg": 52.1,
    "bone_mass_kg": 3.2,
    "skeletal_muscle_kg": 30.5,
    "intracellular_water_kg": 25.6,
    "extracellular_water_kg": 16.7,
    "body_cell_mass_kg": 38.1,
    "subcutaneous_fat_mass_kg": 9.8
  },
  "segmental_analysis": {
    "fat_mass_kg": {
      "right_hand": 0.8,
      "left_hand": 0.8,
      "trunk": 10.2,
      "right_foot": 1.7,
      "left_foot": 1.7
    },
    "fat_percent": {
      "right_hand": 18.5,
      "left_hand": 18.3,
      "trunk": 22.1,
      "right_foot": 19.2,
      "left_foot": 19.4
    },
    "muscle_mass_kg": {
      "right_hand": 3.5,
      "left_hand": 3.6,
      "trunk": 35.8,
      "right_foot": 7.1,
      "left_foot": 7.2
    },
    "muscle_ratio_percent": {
      "right_hand": 102.5,
      "left_hand": 103.1,
      "trunk": 98.7,
      "right_foot": 99.8,
      "left_foot": 100.2
    }
  },
  "health_metrics": {
    "body_score": 85,
    "physical_age": 25,
    "body_type": 8,
    "body_type_name": "Standard type",
    "smi": 7.8,
    "whr": 0.85,
    "visceral_fat": 8,
    "obesity_percent": 21.5,
    "bmi": 25.0,
    "body_fat_percent": 21.5,
    "bmr_kcal": 1650,
    "recommended_intake_kcal": 2300,
    "ideal_weight_kg": 68.0,
    "target_weight_kg": 68.0,
    "weight_control_kg": -2.5,
    "muscle_control_kg": 1.2,
    "fat_control_kg": -3.7,
    "subcutaneous_fat_percent": 13.9
  },
  "energy_consumption_kcal_per_30min": {
    "walk": 95,
    "golf": 105,
    "croquet": 110,
    "tennis_cycling_basketball": 130,
    "squash_tkd_fencing": 145,
    "mountain_climbing": 155,
    "swimming_aerobic_jog": 165,
    "badminton_table_tennis": 140
  },
  "segmental_standards": {
    "fat_standard": {
      "right_hand": "normal",
      "left_hand": "normal",
      "trunk": "high",
      "right_foot": "normal",
      "left_foot": "normal"
    },
    "muscle_standard": {
      "right_hand": "normal",
      "left_hand": "normal",
      "trunk": "normal",
      "right_foot": "low",
      "left_foot": "low"
    }
  }
}
```

#### Error Response:
```json
{
  "status": "error",
  "error_code": "0x03",
  "error_message": "Wrong weight"
}
```

**Error Codes:**
- `0x01`: Wrong age
- `0x02`: Wrong height
- `0x03`: Wrong weight
- `0x04`: Wrong gender
- `0x05`: User type error
- `0x06`: Wrong impedance of both feet
- `0x07`: Hand impedance error
- `0x08`: Left whole body impedance error
- `0x09`: Left hand impedance error
- `0x0A`: Right hand impedance error
- `0x0B`: Left foot impedance error
- `0x0C`: Right foot impedance error
- `0x0D`: Torso impedance error

---

## Flutter Implementation Example

### Dependencies
เพิ่มใน `pubspec.yaml`:
```yaml
dependencies:
  flutter_blue_plus: ^1.31.0
  shared_preferences: ^2.2.0
```

### Basic Implementation

```dart
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:convert';

class BMHScaleService {
  static const String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const String RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static const String TX_CHAR_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8";
  static const String BONDED_DEVICE_KEY = "bmh_bonded_device";
  
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _rxCharacteristic;
  BluetoothCharacteristic? _txCharacteristic;
  
  // Callback for receiving results
  Function(Map<String, dynamic>)? onResultReceived;
  
  // Initialize and auto-connect
  Future<void> initialize() async {
    // Check for previously bonded device
    SharedPreferences prefs = await SharedPreferences.getInstance();
    String? bondedDeviceId = prefs.getString(BONDED_DEVICE_KEY);
    
    if (bondedDeviceId != null) {
      // Try to reconnect to bonded device
      await _reconnectToDevice(bondedDeviceId);
    }
    
    // If not connected, start scanning
    if (_connectedDevice == null) {
      await scanAndConnect();
    }
  }
  
  // Scan for BMH Scale
  Future<void> scanAndConnect() async {
    FlutterBluePlus.startScan(timeout: Duration(seconds: 10));
    
    FlutterBluePlus.scanResults.listen((results) async {
      for (ScanResult r in results) {
        if (r.device.platformName == "BMH_Scale") {
          FlutterBluePlus.stopScan();
          await _connectToDevice(r.device);
          break;
        }
      }
    });
  }
  
  // Connect to device
  Future<void> _connectToDevice(BluetoothDevice device) async {
    await device.connect();
    _connectedDevice = device;
    
    // Save bonded device
    SharedPreferences prefs = await SharedPreferences.getInstance();
    await prefs.setString(BONDED_DEVICE_KEY, device.remoteId.toString());
    
    // Discover services
    List<BluetoothService> services = await device.discoverServices();
    for (BluetoothService service in services) {
      if (service.uuid.toString().toLowerCase() == SERVICE_UUID.toLowerCase()) {
        for (BluetoothCharacteristic char in service.characteristics) {
          if (char.uuid.toString().toLowerCase() == RX_CHAR_UUID.toLowerCase()) {
            _rxCharacteristic = char;
          } else if (char.uuid.toString().toLowerCase() == TX_CHAR_UUID.toLowerCase()) {
            _txCharacteristic = char;
            // Enable notifications
            await char.setNotifyValue(true);
            char.value.listen((value) {
              String jsonStr = utf8.decode(value);
              Map<String, dynamic> result = json.decode(jsonStr);
              if (onResultReceived != null) {
                onResultReceived!(result);
              }
            });
          }
        }
      }
    }
  }
  
  // Reconnect to previously bonded device
  Future<void> _reconnectToDevice(String deviceId) async {
    var devices = FlutterBluePlus.connectedDevices;
    for (var device in devices) {
      if (device.remoteId.toString() == deviceId) {
        await _connectToDevice(device);
        return;
      }
    }
  }
  
  // Send user data
  Future<void> sendUserData({
    required int gender,
    required int productId,
    required int height,
    required int age,
  }) async {
    if (_rxCharacteristic == null) {
      throw Exception("Not connected to BMH Scale");
    }
    
    Map<String, dynamic> userData = {
      "gender": gender,
      "product_id": productId,
      "height": height,
      "age": age,
    };
    
    String jsonStr = json.encode(userData);
    List<int> bytes = utf8.encode(jsonStr);
    
    await _rxCharacteristic!.write(bytes);
    print("User data sent: $jsonStr");
  }
  
  // Disconnect
  Future<void> disconnect() async {
    if (_connectedDevice != null) {
      await _connectedDevice!.disconnect();
      _connectedDevice = null;
      _rxCharacteristic = null;
      _txCharacteristic = null;
    }
  }
  
  // Check connection status
  bool isConnected() {
    return _connectedDevice != null;
  }
}
```

### Usage Example

```dart
// Initialize service
BMHScaleService scaleService = BMHScaleService();

// Set callback for results
scaleService.onResultReceived = (result) {
  if (result['status'] == 'success') {
    print('Weight: ${result['body_composition']['weight_kg']} kg');
    print('Body Fat: ${result['health_metrics']['body_fat_percent']}%');
    print('BMI: ${result['health_metrics']['bmi']}');
    // Update UI with results
  } else {
    print('Error: ${result['error_message']}');
  }
};

// Connect
await scaleService.initialize();

// Send user data when ready to measure
await scaleService.sendUserData(
  gender: 1,        // Male
  productId: 0,     // Normal user
  height: 168,      // 168 cm
  age: 23,          // 23 years old
);

// Results will be received via callback
```

---

## Measurement Flow

1. **App starts** → Auto-connect to previously bonded device (or scan for new device)
2. **User enters data** → Age, gender, height, product type
3. **User presses "Start Measurement"** → App sends JSON to ESP32
4. **ESP32 receives JSON** → Starts measurement sequence
5. **User steps on scale** → Weight measurement begins
6. **User holds handles** → Impedance measurement
7. **Analysis complete** → ESP32 sends results JSON back to app
8. **App displays results** → Show all body composition data
9. **User steps off** → System ready for next measurement

---

## Bonding & Security

### First Connection
- แอปจะค้นหาอุปกรณ์ชื่อ `BMH_Scale`
- เชื่อมต่อและบันทึก Device ID ใน SharedPreferences
- ESP32 บันทึก Device Address ใน NVS (Non-Volatile Storage)

### Subsequent Connections
- แอปจะตรวจสอบ SharedPreferences สำหรับ Device ID ที่เคยจับคู่
- พยายามเชื่อมต่อโดยอัตโนมัติ
- ไม่ต้อง scan หรือ pair ใหม่

### Security Features
- **Bonding**: ใช้ ESP_LE_AUTH_REQ_SC_MITM_BOND
- **Encryption**: ข้อมูลถูก encrypt ด้วย BLE security
- **Persistent pairing**: การจับคู่ถูกเก็บไว้แม้ reset

---

## Troubleshooting

### แอปหาเครื่องไม่เจอ
- ตรวจสอบ Bluetooth เปิดอยู่
- ตรวจสอบ Permission (Location, Bluetooth)
- ตรวจสอบเครื่อง ESP32 มีไฟเลี้ยงอยู่
- ลอง scan ใหม่

### เชื่อมต่อแล้วหลุด
- ตรวจสอบระยะห่าง (ควรอยู่ภายใน 10 เมตร)
- ESP32 จะ restart advertising อัตโนมัติ
- แอปควร implement auto-reconnect

### ไม่ได้รับผลลัพธ์
- ตรวจสอบ JSON ที่ส่งถูกต้อง (gender, height, age, product_id)
- ตรวจสอบผู้ใช้ขึ้นชั่งและจับ handles
- ดู error_message ใน response

---

## Testing

### ทดสอบโดยใช้ Serial Monitor
สามารถทดสอบโดยส่ง JSON ผ่าน Serial Monitor:
```json
{"gender":1,"product_id":0,"height":168,"age":23}
```

### ทดสอบด้วย nRF Connect App
1. ดาวน์โหลด nRF Connect (iOS/Android)
2. Scan หา `BMH_Scale`
3. Connect และ bond
4. เขียน JSON ไปที่ RX Characteristic
5. Enable notification บน TX Characteristic
6. ดูผลลัพธ์

---

## Notes
- **JSON size limit**: แต่ละ packet ~512 bytes (BLE MTU), ระบบจะแบ่งส่งหลาย packet อัตโนมัติ
- **Measurement time**: ใช้เวลาประมาณ 15-20 วินาทีต่อครั้ง
- **Power**: ควรใช้ไฟเลี้ยงแบบ USB หรือ adapter 5V
- **Range**: ระยะสัญญาณ BLE ประมาณ 10 เมตร

---

## Contact
สำหรับคำถามเพิ่มเติมเกี่ยวกับการ integrate กับ Flutter app ติดต่อทีมพัฒนา
