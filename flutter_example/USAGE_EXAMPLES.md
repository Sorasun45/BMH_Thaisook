# Flutter Usage Examples - BMH Scale

ตัวอย่างการใช้งาน BMH Scale Service ใน Flutter แบบละเอียด

---

## 📋 สารบัญ

1. [Setup เบื้องต้น](#setup-เบื้องต้น)
2. [การเชื่อมต่อ BLE](#การเชื่อมต่อ-ble)
3. [การส่งข้อมูลผู้ใช้](#การส่งข้อมูลผู้ใช้)
4. [การรับข้อมูล Real-time](#การรับข้อมูล-real-time)
5. [การแสดงผลลัพธ์](#การแสดงผลลัพธ์)
6. [Error Handling](#error-handling)
7. [Complete Example](#complete-example)

---

## 🚀 Setup เบื้องต้น

### 1. เพิ่ม Dependencies

```yaml
# pubspec.yaml
dependencies:
  flutter:
    sdk: flutter
  
  # BLE
  flutter_blue_plus: ^1.31.0
  
  # Storage
  shared_preferences: ^2.2.0
  
  # Permissions
  permission_handler: ^11.0.0
```

### 2. Android Configuration

```xml
<!-- android/app/src/main/AndroidManifest.xml -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
    <!-- Bluetooth Permissions -->
    <uses-permission android:name="android.permission.BLUETOOTH" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
    <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
    
    <application>
        ...
    </application>
</manifest>
```

### 3. iOS Configuration

```xml
<!-- ios/Runner/Info.plist -->
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app needs Bluetooth to connect to BMH Scale</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>This app needs Bluetooth to connect to BMH Scale</string>
<key>NSLocationWhenInUseUsageDescription</key>
<string>This app needs location permission to scan Bluetooth devices</string>
```

---

## 🔵 การเชื่อมต่อ BLE

### Example 1: เชื่อมต่อแบบพื้นฐาน

```dart
import 'package:flutter/material.dart';
import 'bmh_scale_service.dart';

class BMHConnectionPage extends StatefulWidget {
  @override
  _BMHConnectionPageState createState() => _BMHConnectionPageState();
}

class _BMHConnectionPageState extends State<BMHConnectionPage> {
  final BMHScaleService _scaleService = BMHScaleService();
  bool _isConnected = false;
  String _statusMessage = "Not connected";

  @override
  void initState() {
    super.initState();
    _connectToScale();
  }

  Future<void> _connectToScale() async {
    setState(() {
      _statusMessage = "Connecting...";
    });

    // Set up connection callback
    _scaleService.onConnectionChanged = (connected) {
      setState(() {
        _isConnected = connected;
        _statusMessage = connected 
            ? "Connected to BMH Scale" 
            : "Disconnected";
      });
    };

    // Set up error callback
    _scaleService.onError = (error) {
      setState(() {
        _statusMessage = "Error: $error";
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(error)),
      );
    };

    // Initialize and connect
    try {
      await _scaleService.initialize();
    } catch (e) {
      setState(() {
        _statusMessage = "Failed to connect: $e";
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('BMH Scale Connection')),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(
              _isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
              size: 100,
              color: _isConnected ? Colors.green : Colors.grey,
            ),
            SizedBox(height: 20),
            Text(
              _statusMessage,
              style: TextStyle(fontSize: 18),
              textAlign: TextAlign.center,
            ),
            if (!_isConnected) ...[
              SizedBox(height: 20),
              ElevatedButton(
                onPressed: _connectToScale,
                child: Text('Retry Connection'),
              ),
            ],
          ],
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

### Example 2: Auto-reconnect Pattern

```dart
class AutoReconnectService {
  final BMHScaleService _scaleService = BMHScaleService();
  Timer? _reconnectTimer;
  bool _isConnecting = false;

  void startAutoReconnect() {
    _scaleService.onConnectionChanged = (connected) {
      if (!connected && !_isConnecting) {
        print('Disconnected, attempting to reconnect...');
        _scheduleReconnect();
      } else if (connected) {
        _cancelReconnect();
      }
    };

    _connectToScale();
  }

  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: 3), () {
      _connectToScale();
    });
  }

  void _cancelReconnect() {
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
  }

  Future<void> _connectToScale() async {
    if (_isConnecting) return;
    
    _isConnecting = true;
    try {
      await _scaleService.initialize();
    } catch (e) {
      print('Reconnect failed: $e');
    } finally {
      _isConnecting = false;
    }
  }

  void dispose() {
    _reconnectTimer?.cancel();
    _scaleService.disconnect();
  }
}
```

---

## 📤 การส่งข้อมูลผู้ใช้

### Example 3: ส่งข้อมูลพื้นฐาน

```dart
Future<void> sendUserDataExample() async {
  if (!_scaleService.isConnected) {
    print('Not connected to scale');
    return;
  }

  try {
    await _scaleService.sendUserData(
      gender: 1,        // 0=Female, 1=Male
      productId: 0,     // 0=Normal, 1=Athlete, 2=Child
      height: 168,      // cm
      age: 23,          // years
    );
    print('User data sent successfully');
  } catch (e) {
    print('Failed to send user data: $e');
  }
}
```

### Example 4: ฟอร์มกรอกข้อมูลผู้ใช้

```dart
class UserDataForm extends StatefulWidget {
  final BMHScaleService scaleService;
  
  UserDataForm({required this.scaleService});

  @override
  _UserDataFormState createState() => _UserDataFormState();
}

class _UserDataFormState extends State<UserDataForm> {
  final _formKey = GlobalKey<FormState>();
  
  int _gender = 1; // Male
  int _productId = 0; // Normal
  int _height = 168;
  int _age = 23;

  @override
  Widget build(BuildContext context) {
    return Form(
      key: _formKey,
      child: Column(
        children: [
          // Gender Selection
          DropdownButtonFormField<int>(
            value: _gender,
            decoration: InputDecoration(labelText: 'Gender'),
            items: [
              DropdownMenuItem(value: 0, child: Text('Female')),
              DropdownMenuItem(value: 1, child: Text('Male')),
            ],
            onChanged: (value) => setState(() => _gender = value!),
          ),
          
          SizedBox(height: 16),
          
          // User Type Selection
          DropdownButtonFormField<int>(
            value: _productId,
            decoration: InputDecoration(labelText: 'User Type'),
            items: [
              DropdownMenuItem(value: 0, child: Text('Normal')),
              DropdownMenuItem(value: 1, child: Text('Athlete')),
              DropdownMenuItem(value: 2, child: Text('Child')),
            ],
            onChanged: (value) => setState(() => _productId = value!),
          ),
          
          SizedBox(height: 16),
          
          // Height Input
          TextFormField(
            initialValue: _height.toString(),
            decoration: InputDecoration(
              labelText: 'Height (cm)',
              suffixText: 'cm',
            ),
            keyboardType: TextInputType.number,
            validator: (value) {
              if (value == null || value.isEmpty) {
                return 'Please enter height';
              }
              int? height = int.tryParse(value);
              if (height == null || height < 100 || height > 220) {
                return 'Height must be between 100-220 cm';
              }
              return null;
            },
            onSaved: (value) => _height = int.parse(value!),
          ),
          
          SizedBox(height: 16),
          
          // Age Input
          TextFormField(
            initialValue: _age.toString(),
            decoration: InputDecoration(
              labelText: 'Age (years)',
              suffixText: 'years',
            ),
            keyboardType: TextInputType.number,
            validator: (value) {
              if (value == null || value.isEmpty) {
                return 'Please enter age';
              }
              int? age = int.tryParse(value);
              if (age == null || age < 10 || age > 99) {
                return 'Age must be between 10-99 years';
              }
              return null;
            },
            onSaved: (value) => _age = int.parse(value!),
          ),
          
          SizedBox(height: 32),
          
          // Submit Button
          ElevatedButton(
            onPressed: _submitData,
            style: ElevatedButton.styleFrom(
              padding: EdgeInsets.symmetric(vertical: 16, horizontal: 32),
            ),
            child: Text('Start Measurement'),
          ),
        ],
      ),
    );
  }

  Future<void> _submitData() async {
    if (!_formKey.currentState!.validate()) {
      return;
    }

    _formKey.currentState!.save();

    if (!widget.scaleService.isConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Not connected to scale')),
      );
      return;
    }

    try {
      await widget.scaleService.sendUserData(
        gender: _gender,
        productId: _productId,
        height: _height,
        age: _age,
      );

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Please step on scale and hold handles'),
          duration: Duration(seconds: 3),
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }
}
```

## 🧩 ตัวอย่าง JSON ที่ส่ง/รับ

ต่อไปนี้เป็นตัวอย่าง JSON ที่แอปต้องส่งไปยัง ESP32 และที่แอปจะได้รับกลับจาก ESP32 ผ่าน BLE

### App → ESP32 (ส่งข้อมูลผู้ใช้)

```json
{
  "gender": 1,
  "product_id": 0,
  "height": 168,
  "age": 23
}
```

- gender: 0=หญิง, 1=ชาย
- product_id: 0=ทั่วไป, 1=นักกีฬา, 2=เด็ก
- height: ส่วนสูง (ซม.)
- age: อายุ (ปี)

### ESP32 → App (รับข้อมูลจากเครื่อง)

1) น้ำหนักแบบ Real-time ระหว่างการชั่ง

```json
{
  "type": "weight_realtime",
  "weight": 65.42,
  "stable_count": 3
}
```

2) น้ำหนักล็อกแล้ว และกำลังเริ่มวัด Impedance

```json
{
  "type": "weight_finalized",
  "weight": 65.5,
  "status": "starting_impedance_measurement"
}
```

3) ผลลัพธ์สุดท้าย

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

4) ข้อผิดพลาด (กรณีข้อมูลไม่ถูกต้องหรือวัดไม่ได้)

```json
{
  "status": "error",
  "error_code": "0x03",
  "error_message": "Wrong weight"
}
```

หมายเหตุ: รายละเอียดโครงสร้างผลลัพธ์แบบเต็มดูได้ที่ `BLE_FLUTTER_GUIDE.md` และ `docs/BMH_PROTOCOL.md`

---

## 📊 การรับข้อมูล Real-time

### Example 5: แสดงน้ำหนัก Real-time

```dart
class RealTimeWeightWidget extends StatefulWidget {
  final BMHScaleService scaleService;
  
  RealTimeWeightWidget({required this.scaleService});

  @override
  _RealTimeWeightWidgetState createState() => _RealTimeWeightWidgetState();
}

class _RealTimeWeightWidgetState extends State<RealTimeWeightWidget> {
  double _currentWeight = 0.0;
  int _stableCount = 0;
  bool _isMeasuring = false;

  @override
  void initState() {
    super.initState();
    
    // Set up real-time weight callback
    widget.scaleService.onWeightUpdate = (weight, stableCount) {
      setState(() {
        _currentWeight = weight;
        _stableCount = stableCount;
        _isMeasuring = true;
      });
    };
  }

  @override
  Widget build(BuildContext context) {
    if (!_isMeasuring) {
      return SizedBox.shrink();
    }

    return Card(
      color: Colors.blue[50],
      elevation: 4,
      child: Padding(
        padding: EdgeInsets.all(24),
        child: Column(
          children: [
            Text(
              'Current Weight',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
                color: Colors.blue[900],
              ),
            ),
            SizedBox(height: 12),
            Text(
              '${_currentWeight.toStringAsFixed(2)} kg',
              style: TextStyle(
                fontSize: 48,
                fontWeight: FontWeight.bold,
                color: Colors.blue[700],
              ),
            ),
            SizedBox(height: 16),
            LinearProgressIndicator(
              value: _stableCount / 5.0,
              backgroundColor: Colors.blue[100],
              valueColor: AlwaysStoppedAnimation<Color>(Colors.blue[700]!),
            ),
            SizedBox(height: 8),
            Text(
              'Stability: $_stableCount/5',
              style: TextStyle(
                fontSize: 14,
                color: Colors.blue[600],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
```

### Example 6: แสดงสถานะการวัด

```dart
class MeasurementStatusWidget extends StatefulWidget {
  final BMHScaleService scaleService;
  
  MeasurementStatusWidget({required this.scaleService});

  @override
  _MeasurementStatusWidgetState createState() => 
      _MeasurementStatusWidgetState();
}

class _MeasurementStatusWidgetState extends State<MeasurementStatusWidget> {
  String _status = 'Idle';
  bool _isMeasuringWeight = false;
  bool _isMeasuringImpedance = false;

  @override
  void initState() {
    super.initState();
    
    widget.scaleService.onWeightUpdate = (weight, stableCount) {
      setState(() {
        _isMeasuringWeight = true;
        _isMeasuringImpedance = false;
        _status = 'Measuring weight... Stay still';
      });
    };

    widget.scaleService.onImpedanceStart = () {
      setState(() {
        _isMeasuringWeight = false;
        _isMeasuringImpedance = true;
        _status = 'Measuring impedance... Hold handles steady';
      });
    };
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: ListTile(
        leading: _buildStatusIcon(),
        title: Text(_status),
        subtitle: _buildProgressIndicator(),
      ),
    );
  }

  Widget _buildStatusIcon() {
    if (_isMeasuringWeight) {
      return Icon(Icons.monitor_weight, color: Colors.blue);
    } else if (_isMeasuringImpedance) {
      return Icon(Icons.sensors, color: Colors.orange);
    }
    return Icon(Icons.check_circle, color: Colors.green);
  }

  Widget? _buildProgressIndicator() {
    if (_isMeasuringWeight || _isMeasuringImpedance) {
      return LinearProgressIndicator();
    }
    return null;
  }
}
```

---

## 📈 การแสดงผลลัพธ์

### Example 7: แสดงผลลัพธ์แบบเต็ม

```dart
class ResultsDisplayPage extends StatelessWidget {
  final Map<String, dynamic> results;
  
  ResultsDisplayPage({required this.results});

  @override
  Widget build(BuildContext context) {
    if (results['status'] != 'success') {
      return _buildErrorDisplay();
    }

    final bodyComp = results['body_composition'] as Map<String, dynamic>;
    final healthMetrics = results['health_metrics'] as Map<String, dynamic>;

    return Scaffold(
      appBar: AppBar(title: Text('Measurement Results')),
      body: ListView(
        padding: EdgeInsets.all(16),
        children: [
          _buildSummaryCard(bodyComp, healthMetrics),
          SizedBox(height: 16),
          _buildBodyCompositionCard(bodyComp),
          SizedBox(height: 16),
          _buildHealthMetricsCard(healthMetrics),
          SizedBox(height: 16),
          _buildSegmentalAnalysisCard(results['segmental_analysis']),
        ],
      ),
    );
  }

  Widget _buildSummaryCard(Map bodyComp, Map healthMetrics) {
    return Card(
      color: Colors.blue[50],
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          children: [
            Text(
              'Summary',
              style: TextStyle(
                fontSize: 20,
                fontWeight: FontWeight.bold,
              ),
            ),
            SizedBox(height: 16),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                _buildSummaryItem(
                  'Weight',
                  '${bodyComp['weight_kg'].toStringAsFixed(1)} kg',
                  Icons.monitor_weight,
                ),
                _buildSummaryItem(
                  'BMI',
                  healthMetrics['bmi'].toStringAsFixed(1),
                  Icons.straighten,
                ),
                _buildSummaryItem(
                  'Body Fat',
                  '${healthMetrics['body_fat_percent'].toStringAsFixed(1)}%',
                  Icons.local_fire_department,
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSummaryItem(String label, String value, IconData icon) {
    return Column(
      children: [
        Icon(icon, size: 32, color: Colors.blue[700]),
        SizedBox(height: 8),
        Text(
          value,
          style: TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
          ),
        ),
        Text(
          label,
          style: TextStyle(
            fontSize: 12,
            color: Colors.grey[600],
          ),
        ),
      ],
    );
  }

  Widget _buildBodyCompositionCard(Map<String, dynamic> data) {
    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Body Composition',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
              ),
            ),
            Divider(),
            _buildDataRow('Moisture', '${data['moisture_kg'].toStringAsFixed(1)} kg'),
            _buildDataRow('Body Fat', '${data['body_fat_mass_kg'].toStringAsFixed(1)} kg'),
            _buildDataRow('Protein', '${data['protein_mass_kg'].toStringAsFixed(1)} kg'),
            _buildDataRow('Muscle Mass', '${data['muscle_mass_kg'].toStringAsFixed(1)} kg'),
            _buildDataRow('Bone Mass', '${data['bone_mass_kg'].toStringAsFixed(1)} kg'),
          ],
        ),
      ),
    );
  }

  Widget _buildHealthMetricsCard(Map<String, dynamic> data) {
    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Health Metrics',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
              ),
            ),
            Divider(),
            _buildDataRow('Body Score', '${data['body_score']}'),
            _buildDataRow('Physical Age', '${data['physical_age']} years'),
            _buildDataRow('BMR', '${data['bmr_kcal']} kcal'),
            _buildDataRow('Visceral Fat', '${data['visceral_fat']}'),
            _buildDataRow('Body Type', data['body_type_name']),
          ],
        ),
      ),
    );
  }

  Widget _buildSegmentalAnalysisCard(Map<String, dynamic> data) {
    final fatMass = data['fat_mass_kg'] as Map<String, dynamic>;
    final muscleMass = data['muscle_mass_kg'] as Map<String, dynamic>;

    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'Segmental Analysis',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
              ),
            ),
            Divider(),
            _buildSegmentRow('Right Hand', 
                fatMass['right_hand'], muscleMass['right_hand']),
            _buildSegmentRow('Left Hand', 
                fatMass['left_hand'], muscleMass['left_hand']),
            _buildSegmentRow('Trunk', 
                fatMass['trunk'], muscleMass['trunk']),
            _buildSegmentRow('Right Foot', 
                fatMass['right_foot'], muscleMass['right_foot']),
            _buildSegmentRow('Left Foot', 
                fatMass['left_foot'], muscleMass['left_foot']),
          ],
        ),
      ),
    );
  }

  Widget _buildDataRow(String label, String value) {
    return Padding(
      padding: EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label),
          Text(
            value,
            style: TextStyle(fontWeight: FontWeight.bold),
          ),
        ],
      ),
    );
  }

  Widget _buildSegmentRow(String segment, dynamic fat, dynamic muscle) {
    return Padding(
      padding: EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Expanded(child: Text(segment)),
          Text('Fat: ${fat.toStringAsFixed(1)} kg'),
          SizedBox(width: 16),
          Text('Muscle: ${muscle.toStringAsFixed(1)} kg'),
        ],
      ),
    );
  }

  Widget _buildErrorDisplay() {
    return Scaffold(
      appBar: AppBar(title: Text('Error')),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(Icons.error, size: 64, color: Colors.red),
            SizedBox(height: 16),
            Text(
              'Measurement Error',
              style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 8),
            Text(results['error_message'] ?? 'Unknown error'),
          ],
        ),
      ),
    );
  }
}
```

---

## ⚠️ Error Handling

### Example 8: จัดการ Error แบบครอบคลุม

```dart
class BMHErrorHandler {
  static void handleError(BuildContext context, Map<String, dynamic> error) {
    String errorCode = error['error_code'] ?? '';
    String errorMessage = error['error_message'] ?? 'Unknown error';

    // Map error codes to Thai messages
    Map<String, String> errorMessages = {
      '0x01': 'อายุไม่ถูกต้อง (10-99 ปี)',
      '0x02': 'ส่วนสูงไม่ถูกต้อง (100-220 ซม.)',
      '0x03': 'น้ำหนักไม่อยู่ในช่วงที่กำหนด',
      '0x04': 'ข้อมูลเพศไม่ถูกต้อง',
      '0x05': 'ประเภทผู้ใช้ไม่ถูกต้อง',
      '0x06': 'ค่า Impedance ที่เท้าผิดปกติ',
      '0x07': 'ค่า Impedance ที่มือผิดปกติ',
      '0x08': 'ค่า Impedance ร่างกายซ้ายผิดปกติ',
      '0x09': 'ค่า Impedance มือซ้ายผิดปกติ',
      '0x0A': 'ค่า Impedance มือขวาผิดปกติ',
      '0x0B': 'ค่า Impedance เท้าซ้ายผิดปกติ',
      '0x0C': 'ค่า Impedance เท้าขวาผิดปกติ',
      '0x0D': 'ค่า Impedance ลำตัวผิดปกติ',
    };

    String displayMessage = errorMessages[errorCode] ?? errorMessage;

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Row(
          children: [
            Icon(Icons.error, color: Colors.red),
            SizedBox(width: 8),
            Text('Measurement Error'),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              displayMessage,
              style: TextStyle(fontSize: 16),
            ),
            if (errorCode.isNotEmpty) ...[
              SizedBox(height: 8),
              Text(
                'Error Code: $errorCode',
                style: TextStyle(
                  fontSize: 12,
                  color: Colors.grey[600],
                ),
              ),
            ],
            SizedBox(height: 16),
            Text(
              'แนะนำ:',
              style: TextStyle(fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 4),
            Text(_getErrorSuggestion(errorCode)),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text('OK'),
          ),
        ],
      ),
    );
  }

  static String _getErrorSuggestion(String errorCode) {
    switch (errorCode) {
      case '0x03':
        return '• ตรวจสอบว่ายืนบนชั่งอย่างเดียว\n'
               '• น้ำหนักควรอยู่ระหว่าง 15-150 กก.';
      case '0x06':
      case '0x07':
      case '0x08':
      case '0x09':
      case '0x0A':
      case '0x0B':
      case '0x0C':
      case '0x0D':
        return '• ตรวจสอบว่าจับ handles ทั้ง 2 ข้างแน่น\n'
               '• เท้าต้องเปียกและสัมผัสแผ่นโลหะทั้ง 2 ข้าง\n'
               '• ลองวัดใหม่อีกครั้ง';
      default:
        return '• ตรวจสอบข้อมูลผู้ใช้ที่กรอก\n'
               '• ลองวัดใหม่อีกครั้ง';
    }
  }
}
```

---

## 📱 Complete Example

### Example 9: Complete App

```dart
import 'package:flutter/material.dart';
import 'bmh_scale_service.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BMH Scale',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: BMHMeasurementPage(),
    );
  }
}

class BMHMeasurementPage extends StatefulWidget {
  @override
  _BMHMeasurementPageState createState() => _BMHMeasurementPageState();
}

class _BMHMeasurementPageState extends State<BMHMeasurementPage> {
  final BMHScaleService _service = BMHScaleService();
  
  // Connection state
  bool _isConnected = false;
  String _statusMessage = 'Initializing...';
  
  // Measurement state
  bool _isMeasuring = false;
  double _currentWeight = 0.0;
  int _stableCount = 0;
  bool _isMeasuringImpedance = false;
  
  // User data
  int _gender = 1;
  int _productId = 0;
  int _height = 168;
  int _age = 23;

  @override
  void initState() {
    super.initState();
    _setupCallbacks();
    _initializeService();
  }

  void _setupCallbacks() {
    _service.onConnectionChanged = (connected) {
      setState(() {
        _isConnected = connected;
        _statusMessage = connected 
            ? 'Connected to BMH Scale' 
            : 'Disconnected';
      });
    };

    _service.onWeightUpdate = (weight, stableCount) {
      setState(() {
        _currentWeight = weight;
        _stableCount = stableCount;
        _isMeasuring = true;
        _isMeasuringImpedance = false;
        _statusMessage = 'Measuring weight: ${weight.toStringAsFixed(2)} kg';
      });
    };

    _service.onImpedanceStart = () {
      setState(() {
        _isMeasuringImpedance = true;
        _statusMessage = 'Measuring impedance... Hold handles';
      });
    };

    _service.onResultReceived = (result) {
      setState(() {
        _isMeasuring = false;
        _isMeasuringImpedance = false;
      });

      if (result['status'] == 'success') {
        _showResults(result);
      } else {
        BMHErrorHandler.handleError(context, result);
      }
    };

    _service.onError = (error) {
      setState(() {
        _statusMessage = 'Error: $error';
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(error), backgroundColor: Colors.red),
      );
    };
  }

  Future<void> _initializeService() async {
    try {
      await _service.initialize();
    } catch (e) {
      setState(() {
        _statusMessage = 'Failed to initialize: $e';
      });
    }
  }

  Future<void> _startMeasurement() async {
    if (!_isConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Not connected to scale')),
      );
      return;
    }

    setState(() {
      _statusMessage = 'Sending user data...';
      _currentWeight = 0.0;
      _stableCount = 0;
      _isMeasuring = false;
      _isMeasuringImpedance = false;
    });

    try {
      await _service.sendUserData(
        gender: _gender,
        productId: _productId,
        height: _height,
        age: _age,
      );

      setState(() {
        _statusMessage = 'Please step on scale...';
      });
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Error: $e'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  void _showResults(Map<String, dynamic> results) {
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (context) => ResultsDisplayPage(results: results),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('BMH Scale Measurement'),
        actions: [
          IconButton(
            icon: Icon(
              _isConnected 
                  ? Icons.bluetooth_connected 
                  : Icons.bluetooth_disabled,
            ),
            onPressed: _isConnected ? null : _initializeService,
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Status Card
            _buildStatusCard(),
            SizedBox(height: 16),
            
            // Real-time Weight Display
            if (_isMeasuring) _buildWeightDisplay(),
            if (_isMeasuringImpedance) _buildImpedanceDisplay(),
            if (_isMeasuring || _isMeasuringImpedance) SizedBox(height: 16),
            
            // User Input Form
            _buildUserForm(),
            SizedBox(height: 24),
            
            // Start Button
            ElevatedButton(
              onPressed: _isConnected && !_isMeasuring 
                  ? _startMeasurement 
                  : null,
              style: ElevatedButton.styleFrom(
                padding: EdgeInsets.symmetric(vertical: 16),
              ),
              child: Text(
                _isMeasuring ? 'Measuring...' : 'Start Measurement',
                style: TextStyle(fontSize: 18),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildStatusCard() {
    return Card(
      color: _isConnected ? Colors.green[50] : Colors.grey[200],
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Row(
          children: [
            Icon(
              _isConnected ? Icons.check_circle : Icons.error_outline,
              color: _isConnected ? Colors.green : Colors.grey,
            ),
            SizedBox(width: 8),
            Expanded(
              child: Text(
                _statusMessage,
                style: TextStyle(
                  color: _isConnected ? Colors.green[900] : Colors.grey[700],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildWeightDisplay() {
    return Card(
      color: Colors.blue[50],
      child: Padding(
        padding: EdgeInsets.all(24),
        child: Column(
          children: [
            Text(
              'Current Weight',
              style: TextStyle(
                fontSize: 18,
                fontWeight: FontWeight.bold,
                color: Colors.blue[900],
              ),
            ),
            SizedBox(height: 12),
            Text(
              '${_currentWeight.toStringAsFixed(2)} kg',
              style: TextStyle(
                fontSize: 48,
                fontWeight: FontWeight.bold,
                color: Colors.blue[700],
              ),
            ),
            SizedBox(height: 16),
            LinearProgressIndicator(
              value: _stableCount / 5.0,
              backgroundColor: Colors.blue[100],
              valueColor: AlwaysStoppedAnimation<Color>(Colors.blue[700]!),
            ),
            SizedBox(height: 8),
            Text(
              'Stability: $_stableCount/5',
              style: TextStyle(fontSize: 14, color: Colors.blue[600]),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildImpedanceDisplay() {
    return Card(
      color: Colors.orange[50],
      child: Padding(
        padding: EdgeInsets.all(24),
        child: Column(
          children: [
            Icon(Icons.sensors, size: 64, color: Colors.orange[700]),
            SizedBox(height: 12),
            Text(
              'Measuring Impedance',
              style: TextStyle(
                fontSize: 20,
                fontWeight: FontWeight.bold,
                color: Colors.orange[900],
              ),
            ),
            SizedBox(height: 8),
            Text(
              'Please hold handles steady',
              style: TextStyle(fontSize: 14, color: Colors.orange[600]),
            ),
            SizedBox(height: 12),
            CircularProgressIndicator(
              valueColor: AlwaysStoppedAnimation<Color>(Colors.orange[700]!),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildUserForm() {
    return Card(
      child: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'User Information',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            SizedBox(height: 16),
            
            // Gender
            Row(
              children: [
                Text('Gender: '),
                Radio(
                  value: 1,
                  groupValue: _gender,
                  onChanged: (value) => setState(() => _gender = value as int),
                ),
                Text('Male'),
                Radio(
                  value: 0,
                  groupValue: _gender,
                  onChanged: (value) => setState(() => _gender = value as int),
                ),
                Text('Female'),
              ],
            ),
            
            // User Type
            DropdownButtonFormField<int>(
              value: _productId,
              decoration: InputDecoration(labelText: 'User Type'),
              items: [
                DropdownMenuItem(value: 0, child: Text('Normal')),
                DropdownMenuItem(value: 1, child: Text('Athlete')),
                DropdownMenuItem(value: 2, child: Text('Child')),
              ],
              onChanged: (value) => setState(() => _productId = value ?? 0),
            ),
            
            SizedBox(height: 16),
            
            // Height & Age
            Row(
              children: [
                Expanded(
                  child: TextField(
                    decoration: InputDecoration(
                      labelText: 'Height (cm)',
                      border: OutlineInputBorder(),
                    ),
                    keyboardType: TextInputType.number,
                    onChanged: (value) => _height = int.tryParse(value) ?? 168,
                    controller: TextEditingController(text: _height.toString()),
                  ),
                ),
                SizedBox(width: 16),
                Expanded(
                  child: TextField(
                    decoration: InputDecoration(
                      labelText: 'Age (years)',
                      border: OutlineInputBorder(),
                    ),
                    keyboardType: TextInputType.number,
                    onChanged: (value) => _age = int.tryParse(value) ?? 23,
                    controller: TextEditingController(text: _age.toString()),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _service.disconnect();
    super.dispose();
  }
}
```

---

## 🎓 Best Practices

### 1. การจัดการ Memory
```dart
@override
void dispose() {
  _service.disconnect();
  _timer?.cancel();
  super.dispose();
}
```

### 2. การจัดการ State
```dart
// ใช้ setState เฉพาะเมื่อจำเป็น
if (mounted) {
  setState(() {
    _data = newData;
  });
}
```

### 3. Error Recovery
```dart
try {
  await _service.sendUserData(...);
} catch (e) {
  // Log error
  print('Error: $e');
  
  // Show user-friendly message
  _showErrorDialog(context, e.toString());
  
  // Attempt recovery
  await _reconnect();
}
```

### 4. Loading States
```dart
// แสดง loading indicator
if (_isLoading) {
  return Center(child: CircularProgressIndicator());
}
```

---

## 📝 Notes

- ทดสอบบน device จริง (BLE ไม่ทำงานบน emulator)
- ตรวจสอบ permissions ทุกครั้งก่อนใช้งาน BLE
- Implement timeout สำหรับการเชื่อมต่อ
- จัดการ lifecycle ของ BLE connection ให้ดี
- ใช้ FutureBuilder/StreamBuilder สำหรับ async operations

---

**Happy Coding! 🚀**
