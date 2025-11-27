# Flutter BMH Scale Example

This directory contains a complete Flutter example for connecting to the BMH Scale via BLE.

## Files

1. **bmh_scale_service.dart** - Complete BLE service with all improvements
2. **main.dart** - Example Flutter UI demonstrating usage

## Setup

### 1. Create new Flutter project:
```bash
flutter create bmh_scale_app
cd bmh_scale_app
```

### 2. Update `pubspec.yaml`:
```yaml
dependencies:
  flutter:
    sdk: flutter
  flutter_blue_plus: ^1.31.0
  shared_preferences: ^2.2.0
  permission_handler: ^11.0.0
```

### 3. Copy files:
- Copy `bmh_scale_service.dart` to `lib/`
- Replace `lib/main.dart` with `main.dart` from this folder

### 4. Android configuration:

Add to `android/app/src/main/AndroidManifest.xml`:
```xml
<manifest>
  <uses-permission android:name="android.permission.BLUETOOTH" />
  <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
  <uses-permission android:name="android.permission.BLUETOOTH_SCAN" />
  <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
  <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
  <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
</manifest>
```

### 5. iOS configuration:

Add to `ios/Runner/Info.plist`:
```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Need Bluetooth to connect to BMH Scale</string>
<key>NSBluetoothPeripheralUsageDescription</key>
<string>Need Bluetooth to connect to BMH Scale</string>
```

## Key Improvements Over Basic Example

### ✅ JSON Chunking
- Handles large JSON responses split across multiple BLE packets
- Buffers incoming data until complete JSON received

### ✅ Auto-Reconnect
- Automatically reconnects if connection drops
- Remembers bonded device across app restarts

### ✅ Permission Handling
- Requests all necessary Bluetooth permissions
- Works on both Android and iOS

### ✅ Error Handling
- Comprehensive try-catch blocks
- User-friendly error messages via callbacks

### ✅ MTU Negotiation
- Requests 512-byte MTU for faster data transfer
- Falls back gracefully if not supported

### ✅ Connection State Monitoring
- Real-time connection status updates
- Callback notifications for UI updates

## Usage

```dart
// Initialize service
BMHScaleService scaleService = BMHScaleService();

// Set up callbacks
scaleService.onConnectionChanged = (connected) {
  print("Connection: $connected");
};

scaleService.onResultReceived = (result) {
  if (result['status'] == 'success') {
    print("Weight: ${result['body_composition']['weight_kg']} kg");
  }
};

scaleService.onError = (error) {
  print("Error: $error");
};

// Connect
await scaleService.initialize();

// Send measurement request
await scaleService.sendUserData(
  gender: 1,
  productId: 0,
  height: 168,
  age: 23,
);

// Results will arrive via onResultReceived callback
```

## Testing

1. Run the app: `flutter run`
2. Grant Bluetooth permissions
3. Wait for auto-connection to BMH Scale
4. Enter user data (gender, height, age)
5. Press "Start Measurement"
6. Step on scale and hold handles
7. View results in dialog

## Troubleshooting

### Device not found
- Ensure BMH Scale is powered on
- Check Bluetooth is enabled
- Verify permissions are granted
- Try manual scan button

### Connection drops
- Check distance (< 10 meters)
- Auto-reconnect should trigger automatically
- Check battery/power supply

### No results received
- Ensure user stepped on scale
- Verify handles are held properly
- Check Serial Monitor on ESP32 for errors

## Production Checklist

- [ ] Test on real devices (not simulator)
- [ ] Test Android + iOS
- [ ] Test permission requests
- [ ] Test auto-reconnect
- [ ] Test with multiple devices nearby
- [ ] Handle app going to background
- [ ] Add loading indicators
- [ ] Add timeout handling
- [ ] Save measurement history
- [ ] Export results (PDF/CSV)
