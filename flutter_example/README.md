# BMH Scale Flutter Example

ตัวอย่างการใช้งาน BMH Scale Service ใน Flutter Application

---

## 📱 Overview

โฟลเดอร์นี้มีโค้ดตัวอย่างสำหรับการเชื่อมต่อและใช้งาน BMH Scale ผ่าน BLE ใน Flutter:

- `bmh_scale_service.dart` - BLE Service class สำหรับเชื่อมต่อและสื่อสารกับ ESP32
- `main.dart` - ตัวอย่าง UI และการใช้งาน
- `USAGE_EXAMPLES.md` - ตัวอย่างการใช้งานแบบละเอียด

---

## 🚀 Quick Start

### 1. สร้างโปรเจค Flutter ใหม่

```bash
flutter create bmh_scale_app
cd bmh_scale_app
```

### 2. เพิ่ม Dependencies

แก้ไข `pubspec.yaml`:

```yaml
dependencies:
  flutter:
    sdk: flutter
  
  # BLE Communication
  flutter_blue_plus: ^1.31.0
  
  # Local Storage
  shared_preferences: ^2.2.0
  
  # Permissions
  permission_handler: ^11.0.0
```

จากนั้น run:
```bash
flutter pub get
```

### 3. คัดลอกไฟล์

คัดลอกไฟล์จากโฟลเดอร์นี้:
- `bmh_scale_service.dart` → `lib/bmh_scale_service.dart`
- `main.dart` → `lib/main.dart`

### 4. ตั้งค่า Permissions

#### Android (`android/app/src/main/AndroidManifest.xml`)

```xml
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <!-- Bluetooth Permissions -->
    <uses-permission android:name="android.permission.BLUETOOTH" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" 
                     android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
    <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
    
    <!-- For Android 12+ -->
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN"
                     tools:targetApi="31" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT"
                     tools:targetApi="31" />
    
    <application
        android:label="BMH Scale"
        android:name="${applicationName}"
        android:icon="@mipmap/ic_launcher">
        ...
    </application>
</manifest>
```

#### iOS (`ios/Runner/Info.plist`)

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to BMH Scale for body composition measurement</string>

<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to connect to BMH Scale</string>

<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs location permission to scan for Bluetooth devices</string>
```

### 5. Run

```bash
# สำหรับ Android
flutter run

# สำหรับ iOS
flutter run
```

**หมายเหตุ**: ต้องทดสอบบน device จริง (BLE ไม่ทำงานบน emulator/simulator)

---

## 📖 การใช้งาน

### Basic Usage

```dart
import 'package:flutter/material.dart';
import 'bmh_scale_service.dart';

class MyMeasurementPage extends StatefulWidget {
  @override
  _MyMeasurementPageState createState() => _MyMeasurementPageState();
}

class _MyMeasurementPageState extends State<MyMeasurementPage> {
  final BMHScaleService _scaleService = BMHScaleService();

  @override
  void initState() {
    super.initState();
    _setupService();
  }

  void _setupService() async {
    // Setup callbacks
    _scaleService.onConnectionChanged = (connected) {
      print('Connected: $connected');
    };

    _scaleService.onWeightUpdate = (weight, stableCount) {
      print('Weight: $weight kg (stable: $stableCount/5)');
    };

    _scaleService.onImpedanceStart = () {
      print('Starting impedance measurement...');
    };

    _scaleService.onResultReceived = (result) {
      print('Results received!');
      // Process results
    };

    _scaleService.onError = (error) {
      print('Error: $error');
    };

    // Initialize and connect
    await _scaleService.initialize();
  }

  Future<void> _startMeasurement() async {
    await _scaleService.sendUserData(
      gender: 1,        // Male
      productId: 0,     // Normal user
      height: 168,      // cm
      age: 23,          // years
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('BMH Scale')),
      body: Center(
        child: ElevatedButton(
          onPressed: _startMeasurement,
          child: Text('Start Measurement'),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _scaleService.disconnect();
    super.dispose();
  }
}
```

---

## 🎯 Features

### BMHScaleService Class

#### Properties
- `isConnected` - สถานะการเชื่อมต่อ

#### Callbacks
- `onConnectionChanged(bool connected)` - เรียกเมื่อสถานะการเชื่อมต่อเปลี่ยน
- `onWeightUpdate(double weight, int stableCount)` - เรียกเมื่อได้รับค่าน้ำหนัก real-time
- `onImpedanceStart()` - เรียกเมื่อเริ่มวัด impedance
- `onResultReceived(Map<String, dynamic> result)` - เรียกเมื่อได้รับผลลัพธ์
- `onError(String error)` - เรียกเมื่อเกิด error

#### Methods
- `initialize()` - เริ่มต้นและเชื่อมต่อ BLE
- `scanAndConnect()` - Scan และเชื่อมต่อกับ BMH Scale
- `sendUserData({...})` - ส่งข้อมูลผู้ใช้และเริ่มวัด
- `disconnect()` - ตัดการเชื่อมต่อ

---

## 📊 Data Types

### Input: User Data

```dart
await service.sendUserData(
  gender: 1,        // 0=Female, 1=Male
  productId: 0,     // 0=Normal, 1=Athlete, 2=Child
  height: 168,      // cm (100-220)
  age: 23,          // years (10-99)
);
```

### Output: Real-time Weight

```dart
service.onWeightUpdate = (weight, stableCount) {
  // weight: double (kg)
  // stableCount: int (0-5)
};
```

### Output: Final Results

```dart
service.onResultReceived = (result) {
  if (result['status'] == 'success') {
    // Body composition
    double weight = result['body_composition']['weight_kg'];
    double bodyFat = result['body_composition']['body_fat_mass_kg'];
    double muscleMass = result['body_composition']['muscle_mass_kg'];
    
    // Health metrics
    double bmi = result['health_metrics']['bmi'];
    double bodyFatPercent = result['health_metrics']['body_fat_percent'];
    int bmr = result['health_metrics']['bmr_kcal'];
    int bodyScore = result['health_metrics']['body_score'];
    
    // Segmental analysis
    Map segmental = result['segmental_analysis'];
    double rightHandFat = segmental['fat_mass_kg']['right_hand'];
    double trunkMuscle = segmental['muscle_mass_kg']['trunk'];
  } else {
    // Error handling
    String errorCode = result['error_code'];
    String errorMessage = result['error_message'];
  }
};
```

---

## 🔧 Customization

### ปรับแต่ง UI

คุณสามารถปรับแต่ง `main.dart` ตามต้องการ:

```dart
// Custom weight display
Widget buildWeightDisplay(double weight, int stableCount) {
  return Column(
    children: [
      Text(
        '${weight.toStringAsFixed(2)} kg',
        style: TextStyle(fontSize: 48, fontWeight: FontWeight.bold),
      ),
      LinearProgressIndicator(value: stableCount / 5.0),
      Text('Stability: $stableCount/5'),
    ],
  );
}

// Custom results display
Widget buildResultsCard(Map<String, dynamic> results) {
  return Card(
    child: Column(
      children: [
        ListTile(
          title: Text('Weight'),
          trailing: Text('${results['body_composition']['weight_kg']} kg'),
        ),
        ListTile(
          title: Text('BMI'),
          trailing: Text('${results['health_metrics']['bmi']}'),
        ),
        // Add more fields...
      ],
    ),
  );
}
```

### ปรับแต่ง Service

แก้ไข `bmh_scale_service.dart`:

```dart
// เปลี่ยน timeout
await device.connect(timeout: Duration(seconds: 20));

// เปลี่ยน scan timeout
FlutterBluePlus.startScan(timeout: Duration(seconds: 15));

// เพิ่ม logging
print('Connecting to ${device.platformName}...');
```

---

## 🐛 Troubleshooting

### แอปไม่สามารถหา BMH Scale

**สาเหตุ**:
- Bluetooth ไม่เปิด
- Permissions ไม่ได้รับอนุญาต
- ESP32 ไม่มีไฟหรือไม่ทำงาน
- อยู่ไกลเกินไป (> 10 เมตร)

**แก้ไข**:
```dart
// ตรวจสอบ Bluetooth state
FlutterBluePlus.adapterState.listen((state) {
  if (state != BluetoothAdapterState.on) {
    print('Bluetooth is OFF!');
  }
});

// ตรวจสอบ permissions
if (Platform.isAndroid) {
  var status = await Permission.bluetoothScan.status;
  if (!status.isGranted) {
    await Permission.bluetoothScan.request();
  }
}
```

### เชื่อมต่อแล้วหลุด

**สาเหตุ**:
- สัญญาณอ่อน
- Interference
- Battery ESP32 หมด

**แก้ไข**:
```dart
// Implement auto-reconnect
_device.connectionState.listen((state) {
  if (state == BluetoothConnectionState.disconnected) {
    Future.delayed(Duration(seconds: 2), () {
      _reconnect();
    });
  }
});
```

### ไม่ได้รับค่าน้ำหนัก Real-time

**สาเหตุ**:
- Callback ไม่ได้ตั้งค่า
- MTU เล็กเกินไป

**แก้ไข**:
```dart
// ตั้งค่า callback ก่อน initialize
service.onWeightUpdate = (weight, stableCount) {
  setState(() {
    _currentWeight = weight;
  });
};

await service.initialize();

// Request larger MTU
int mtu = await device.requestMtu(512);
print('MTU: $mtu');
```

### ได้รับผลลัพธ์ไม่ครบ

**สาเหตุ**:
- JSON ใหญ่เกินไป แบ่งส่งหลาย packets
- Buffer overflow

**แก้ไข**:
ระบบจะจัดการ buffering อัตโนมัติ แต่ถ้ามีปัญหา:
```dart
// เพิ่ม buffer size
String _receivedBuffer = "";
int _maxBufferSize = 10240; // 10KB

void _onDataReceived(List<int> value) {
  _receivedBuffer += utf8.decode(value);
  
  if (_receivedBuffer.length > _maxBufferSize) {
    _receivedBuffer = "";
    print('Buffer overflow!');
  }
}
```

---

## 📚 Additional Resources

- **[USAGE_EXAMPLES.md](USAGE_EXAMPLES.md)** - ตัวอย่างโค้ดละเอียด
- **[../BLE_FLUTTER_GUIDE.md](../BLE_FLUTTER_GUIDE.md)** - คู่มือ BLE และ Protocol
- **[../docs/BMH_PROTOCOL.md](../docs/BMH_PROTOCOL.md)** - โปรโตคอล BMH05108

### External Links
- [flutter_blue_plus Documentation](https://pub.dev/packages/flutter_blue_plus)
- [Flutter BLE Tutorial](https://docs.flutter.dev/cookbook)
- [ESP32 BLE Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html)

---

## 🤝 Contributing

พบ bug หรือมีข้อเสนอแนะ? 
- เปิด Issue: [GitHub Issues](https://github.com/Sorasun45/BMH_Thaisook/issues)
- ส่ง Pull Request: [GitHub Pull Requests](https://github.com/Sorasun45/BMH_Thaisook/pulls)

---

## 📄 License

MIT License - ดูรายละเอียดใน [LICENSE](../LICENSE)

---

## 👨‍💻 Author

**Sorasun45**
- GitHub: [@Sorasun45](https://github.com/Sorasun45)
- Repository: [BMH_Thaisook](https://github.com/Sorasun45/BMH_Thaisook)

---

## 📞 Support

ติดปัญหาหรือต้องการความช่วยเหลือ?
1. อ่าน [USAGE_EXAMPLES.md](USAGE_EXAMPLES.md)
2. ตรวจสอบ [BLE_FLUTTER_GUIDE.md](../BLE_FLUTTER_GUIDE.md)
3. ค้นหาใน [Issues](https://github.com/Sorasun45/BMH_Thaisook/issues)
4. เปิด Issue ใหม่

---

**Happy Coding! 🚀📱**
