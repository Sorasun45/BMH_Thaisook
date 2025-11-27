#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "beb5483f-36e1-4688-b7f5-ea07361b26a8"

// BLE Device Name
#define BLE_DEVICE_NAME "BMH_Scale"

// Callback function type for received data
typedef void (*BLEDataCallback)(const String &data);

class BLEHandler {
public:
    BLEHandler();
    void begin(BLEDataCallback callback);
    void sendData(const String &data);
    bool isConnected();
    String getDeviceName();
    
private:
    BLEServer *bleServer;
    BLECharacteristic *txCharacteristic;
    BLECharacteristic *rxCharacteristic;
    BLEDataCallback dataCallback;
    bool deviceConnected;
    bool oldDeviceConnected;
    Preferences preferences;
    
    void setupBLE();
    void loadBondedDevices();
    void saveBondedDevice(const String &address);
    
    friend class ServerCallbacks;
    friend class CharacteristicCallbacks;
};

// Global instance
extern BLEHandler bleHandler;

#endif // BLE_HANDLER_H
