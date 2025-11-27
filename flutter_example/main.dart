import 'package:flutter/material.dart';
import 'bmh_scale_service.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BMH Scale Demo',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: BMHScalePage(),
    );
  }
}

class BMHScalePage extends StatefulWidget {
  @override
  _BMHScalePageState createState() => _BMHScalePageState();
}

class _BMHScalePageState extends State<BMHScalePage> {
  final BMHScaleService _scaleService = BMHScaleService();
  bool _isConnected = false;
  bool _isScanning = false;
  String _statusMessage = "Not connected";
  Map<String, dynamic>? _lastResult;

  // User input controllers
  final TextEditingController _heightController = TextEditingController(
    text: "168",
  );
  final TextEditingController _ageController = TextEditingController(
    text: "23",
  );
  int _gender = 1; // 0=Female, 1=Male
  int _productId = 0; // 0=Normal, 1=Athlete, 2=Child

  @override
  void initState() {
    super.initState();
    _initializeBLE();
  }

  Future<void> _initializeBLE() async {
    setState(() {
      _isScanning = true;
      _statusMessage = "Initializing...";
    });

    // Set up callbacks
    _scaleService.onConnectionChanged = (connected) {
      setState(() {
        _isConnected = connected;
        _statusMessage = connected ? "Connected to BMH Scale" : "Disconnected";
      });
    };

    _scaleService.onResultReceived = (result) {
      setState(() {
        _lastResult = result;
        _statusMessage = "Results received!";
      });
      _showResultDialog(result);
    };

    _scaleService.onError = (error) {
      setState(() {
        _statusMessage = "Error: $error";
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(error), backgroundColor: Colors.red),
      );
    };

    // Initialize
    try {
      await _scaleService.initialize();
      setState(() {
        _isScanning = false;
      });
    } catch (e) {
      setState(() {
        _isScanning = false;
        _statusMessage = "Initialization failed: $e";
      });
    }
  }

  Future<void> _startMeasurement() async {
    if (!_isConnected) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Please connect to BMH Scale first")),
      );
      return;
    }

    int height = int.tryParse(_heightController.text) ?? 168;
    int age = int.tryParse(_ageController.text) ?? 23;

    try {
      setState(() {
        _statusMessage = "Sending user data...";
      });

      await _scaleService.sendUserData(
        gender: _gender,
        productId: _productId,
        height: height,
        age: age,
      );

      setState(() {
        _statusMessage = "Please step on scale and hold handles...";
      });

      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text("Please step on scale and hold handles"),
          duration: Duration(seconds: 3),
        ),
      );
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("Error: $e"), backgroundColor: Colors.red),
      );
    }
  }

  void _showResultDialog(Map<String, dynamic> result) {
    if (result['status'] == 'error') {
      showDialog(
        context: context,
        builder: (context) => AlertDialog(
          title: Text("Error"),
          content: Text(result['error_message'] ?? "Unknown error"),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: Text("OK"),
            ),
          ],
        ),
      );
      return;
    }

    // Success - show results
    var bodyComp = result['body_composition'];
    var healthMetrics = result['health_metrics'];

    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: Text("Measurement Results"),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              _buildResultRow("Weight", "${bodyComp['weight_kg']} kg"),
              _buildResultRow(
                "Body Fat",
                "${healthMetrics['body_fat_percent']}%",
              ),
              _buildResultRow("BMI", "${healthMetrics['bmi']}"),
              _buildResultRow(
                "Muscle Mass",
                "${bodyComp['muscle_mass_kg']} kg",
              ),
              _buildResultRow("BMR", "${healthMetrics['bmr_kcal']} kcal"),
              _buildResultRow(
                "Body Type",
                "${healthMetrics['body_type_name']}",
              ),
              _buildResultRow(
                "Physical Age",
                "${healthMetrics['physical_age']} years",
              ),
              Divider(),
              Text(
                "See console for full results",
                style: TextStyle(fontSize: 12, color: Colors.grey),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: Text("OK"),
          ),
        ],
      ),
    );

    // Print full results to console
    print("=== FULL RESULTS ===");
    print(result);
  }

  Widget _buildResultRow(String label, String value) {
    return Padding(
      padding: EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: TextStyle(fontWeight: FontWeight.bold)),
          Text(value),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text("BMH Scale"),
        actions: [
          IconButton(
            icon: Icon(
              _isConnected
                  ? Icons.bluetooth_connected
                  : Icons.bluetooth_disabled,
            ),
            onPressed: _isConnected ? null : () => _initializeBLE(),
          ),
        ],
      ),
      body: Padding(
        padding: EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Connection status
            Card(
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
                          color: _isConnected
                              ? Colors.green[900]
                              : Colors.grey[700],
                        ),
                      ),
                    ),
                    if (_isScanning) CircularProgressIndicator(),
                  ],
                ),
              ),
            ),

            SizedBox(height: 24),

            // User input form
            Text(
              "User Information",
              style: Theme.of(context).textTheme.titleLarge,
            ),
            SizedBox(height: 16),

            // Gender
            Row(
              children: [
                Text("Gender: "),
                Radio(
                  value: 1,
                  groupValue: _gender,
                  onChanged: (value) => setState(() => _gender = value as int),
                ),
                Text("Male"),
                Radio(
                  value: 0,
                  groupValue: _gender,
                  onChanged: (value) => setState(() => _gender = value as int),
                ),
                Text("Female"),
              ],
            ),

            // Product ID
            DropdownButtonFormField<int>(
              value: _productId,
              decoration: InputDecoration(labelText: "User Type"),
              items: [
                DropdownMenuItem(value: 0, child: Text("Normal")),
                DropdownMenuItem(value: 1, child: Text("Athlete")),
                DropdownMenuItem(value: 2, child: Text("Child")),
              ],
              onChanged: (value) => setState(() => _productId = value ?? 0),
            ),

            SizedBox(height: 16),

            // Height
            TextField(
              controller: _heightController,
              decoration: InputDecoration(
                labelText: "Height (cm)",
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),

            SizedBox(height: 16),

            // Age
            TextField(
              controller: _ageController,
              decoration: InputDecoration(
                labelText: "Age (years)",
                border: OutlineInputBorder(),
              ),
              keyboardType: TextInputType.number,
            ),

            SizedBox(height: 24),

            // Start button
            ElevatedButton(
              onPressed: _isConnected ? _startMeasurement : null,
              style: ElevatedButton.styleFrom(
                padding: EdgeInsets.symmetric(vertical: 16),
                backgroundColor: Colors.blue,
              ),
              child: Text("Start Measurement", style: TextStyle(fontSize: 18)),
            ),

            SizedBox(height: 16),

            // Disconnect button
            if (_isConnected)
              OutlinedButton(
                onPressed: () async {
                  await _scaleService.disconnect();
                  setState(() {
                    _isConnected = false;
                    _statusMessage = "Disconnected";
                  });
                },
                child: Text("Disconnect"),
              ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _heightController.dispose();
    _ageController.dispose();
    _scaleService.disconnect();
    super.dispose();
  }
}
