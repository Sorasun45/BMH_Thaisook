import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:convert';
import 'dart:io';

class BMHScaleService {
  static const String SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b";
  static const String RX_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8";
  static const String TX_CHAR_UUID = "beb5483f-36e1-4688-b7f5-ea07361b26a8";
  static const String BONDED_DEVICE_KEY = "bmh_bonded_device";

  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _rxCharacteristic;
  BluetoothCharacteristic? _txCharacteristic;
  String _receivedBuffer = "";

  // Callbacks
  Function(Map<String, dynamic>)? onResultReceived;
  Function(String)? onError;
  Function(bool)? onConnectionChanged;

  bool get isConnected => _connectedDevice != null;

  // Initialize and request permissions
  Future<void> initialize() async {
    try {
      // Request permissions
      await _requestPermissions();

      // Check for previously bonded device
      SharedPreferences prefs = await SharedPreferences.getInstance();
      String? bondedDeviceId = prefs.getString(BONDED_DEVICE_KEY);

      if (bondedDeviceId != null) {
        print("Attempting to reconnect to bonded device: $bondedDeviceId");
        await _reconnectToDevice(bondedDeviceId);
      }

      // If not connected, start scanning
      if (_connectedDevice == null) {
        await scanAndConnect();
      }
    } catch (e) {
      _handleError("Initialization error: $e");
    }
  }

  // Request necessary permissions
  Future<void> _requestPermissions() async {
    if (Platform.isAndroid) {
      Map<Permission, PermissionStatus> statuses = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.location,
      ].request();

      if (statuses.values.any((status) => !status.isGranted)) {
        throw Exception("Bluetooth permissions not granted");
      }
    }
  }

  // Scan for BMH Scale
  Future<void> scanAndConnect() async {
    try {
      print("Starting BLE scan...");
      FlutterBluePlus.startScan(timeout: Duration(seconds: 10));

      FlutterBluePlus.scanResults.listen((results) async {
        for (ScanResult r in results) {
          print(
            "Found device: ${r.device.platformName} (${r.device.remoteId})",
          );

          if (r.device.platformName == "BMH_Scale") {
            print("BMH Scale found! Connecting...");
            await FlutterBluePlus.stopScan();
            await _connectToDevice(r.device);
            break;
          }
        }
      });
    } catch (e) {
      _handleError("Scan error: $e");
    }
  }

  // Connect to device
  Future<void> _connectToDevice(BluetoothDevice device) async {
    try {
      // Connect
      await device.connect(timeout: Duration(seconds: 15));
      print("Connected to ${device.platformName}");

      _connectedDevice = device;

      // Request larger MTU for better performance
      try {
        int mtu = await device.requestMtu(512);
        print("MTU set to: $mtu");
      } catch (e) {
        print("MTU request failed: $e");
      }

      // Save bonded device
      SharedPreferences prefs = await SharedPreferences.getInstance();
      await prefs.setString(BONDED_DEVICE_KEY, device.remoteId.toString());
      print("Device bonded and saved");

      // Monitor connection state
      device.connectionState.listen((state) {
        print("Connection state changed: $state");
        if (state == BluetoothConnectionState.disconnected) {
          _connectedDevice = null;
          _rxCharacteristic = null;
          _txCharacteristic = null;

          if (onConnectionChanged != null) {
            onConnectionChanged!(false);
          }

          // Auto-reconnect after 2 seconds
          print("Device disconnected, attempting to reconnect...");
          Future.delayed(Duration(seconds: 2), () {
            _reconnectToDevice(device.remoteId.toString());
          });
        } else if (state == BluetoothConnectionState.connected) {
          if (onConnectionChanged != null) {
            onConnectionChanged!(true);
          }
        }
      });

      // Discover services
      await _discoverServices(device);
    } catch (e) {
      _handleError("Connection error: $e");
      _connectedDevice = null;
    }
  }

  // Discover services and characteristics
  Future<void> _discoverServices(BluetoothDevice device) async {
    try {
      print("Discovering services...");
      List<BluetoothService> services = await device.discoverServices();

      for (BluetoothService service in services) {
        print("Service found: ${service.uuid}");

        if (service.uuid.toString().toLowerCase() ==
            SERVICE_UUID.toLowerCase()) {
          print("BMH Service found!");

          for (BluetoothCharacteristic char in service.characteristics) {
            print("Characteristic: ${char.uuid}");

            if (char.uuid.toString().toLowerCase() ==
                RX_CHAR_UUID.toLowerCase()) {
              _rxCharacteristic = char;
              print("RX Characteristic set");
            } else if (char.uuid.toString().toLowerCase() ==
                TX_CHAR_UUID.toLowerCase()) {
              _txCharacteristic = char;
              print("TX Characteristic set");

              // Enable notifications
              await char.setNotifyValue(true);
              print("Notifications enabled");

              // Listen for data
              char.value.listen(_onDataReceived);
            }
          }
        }
      }

      if (_rxCharacteristic == null || _txCharacteristic == null) {
        throw Exception("Required characteristics not found");
      }

      print("Device ready!");
    } catch (e) {
      _handleError("Service discovery error: $e");
    }
  }

  // Handle incoming data
  void _onDataReceived(List<int> value) {
    try {
      String chunk = utf8.decode(value);
      _receivedBuffer += chunk;

      print(
        "Received chunk (${value.length} bytes): ${chunk.substring(0, chunk.length > 50 ? 50 : chunk.length)}...",
      );

      // Check if JSON is complete
      if (_isCompleteJson(_receivedBuffer)) {
        print("Complete JSON received (${_receivedBuffer.length} bytes)");

        try {
          Map<String, dynamic> result = json.decode(_receivedBuffer);

          if (onResultReceived != null) {
            onResultReceived!(result);
          }

          print("Result parsed successfully");
        } catch (e) {
          _handleError("JSON parse error: $e");
        }

        _receivedBuffer = ""; // Clear buffer
      }
    } catch (e) {
      _handleError("Data receive error: $e");
      _receivedBuffer = "";
    }
  }

  // Check if JSON is complete
  bool _isCompleteJson(String str) {
    if (str.isEmpty) return false;

    try {
      json.decode(str);
      return true;
    } catch (e) {
      return false;
    }
  }

  // Reconnect to previously bonded device
  Future<void> _reconnectToDevice(String deviceId) async {
    try {
      // Check if already connected
      var devices = await FlutterBluePlus.connectedDevices;
      for (var device in devices) {
        if (device.remoteId.toString() == deviceId) {
          await _connectToDevice(device);
          return;
        }
      }

      // Not connected, start scanning
      print("Device not found in connected devices, starting scan...");
      await scanAndConnect();
    } catch (e) {
      _handleError("Reconnect error: $e");
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

    try {
      Map<String, dynamic> userData = {
        "gender": gender,
        "product_id": productId,
        "height": height,
        "age": age,
      };

      String jsonStr = json.encode(userData);
      List<int> bytes = utf8.encode(jsonStr);

      print("Sending user data: $jsonStr");
      await _rxCharacteristic!.write(bytes, withoutResponse: false);
      print("User data sent successfully");
    } catch (e) {
      _handleError("Send error: $e");
      rethrow;
    }
  }

  // Disconnect
  Future<void> disconnect() async {
    try {
      if (_connectedDevice != null) {
        await _connectedDevice!.disconnect();
        _connectedDevice = null;
        _rxCharacteristic = null;
        _txCharacteristic = null;
        _receivedBuffer = "";
        print("Disconnected");
      }
    } catch (e) {
      _handleError("Disconnect error: $e");
    }
  }

  // Unpair device (clear bonding)
  Future<void> unpair() async {
    try {
      await disconnect();

      SharedPreferences prefs = await SharedPreferences.getInstance();
      await prefs.remove(BONDED_DEVICE_KEY);
      print("Device unpaired");
    } catch (e) {
      _handleError("Unpair error: $e");
    }
  }

  // Error handler
  void _handleError(String message) {
    print("ERROR: $message");
    if (onError != null) {
      onError!(message);
    }
  }
}
