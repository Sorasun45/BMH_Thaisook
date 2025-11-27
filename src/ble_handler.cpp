#include "ble_handler.h"

// Security callbacks
class MySecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() {
        Serial.println("PassKeyRequest");
        return 123456;
    }
    
    void onPassKeyNotify(uint32_t pass_key) {
        Serial.printf("PassKeyNotify: %d\n", pass_key);
    }
    
    bool onSecurityRequest() {
        Serial.println("SecurityRequest");
        return true;
    }
    
    void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
        if (auth_cmpl.success) {
            Serial.println("Authentication Success");
        } else {
            Serial.println("Authentication Failed");
        }
    }
    
    bool onConfirmPIN(uint32_t pin) {
        Serial.printf("ConfirmPIN: %d\n", pin);
        return true;
    }
};

// Server callbacks
class ServerCallbacks: public BLEServerCallbacks {
    BLEHandler* handler;
public:
    ServerCallbacks(BLEHandler* h) : handler(h) {}
    
    void onConnect(BLEServer* pServer) {
        handler->deviceConnected = true;
        Serial.println("BLE Client Connected");
        
        // Get connected device address
        std::map<uint16_t, conn_status_t> connections = pServer->getPeerDevices(true);
        if (!connections.empty()) {
            auto it = connections.begin();
            esp_bd_addr_t addr;
            memcpy(addr, it->second.peer_device, sizeof(esp_bd_addr_t));
            BLEAddress bleAddr = BLEAddress(addr);
            String addrStr = String(bleAddr.toString().c_str());
            Serial.printf("Device address: %s\n", addrStr.c_str());
            handler->saveBondedDevice(addrStr);
        }
    }

    void onDisconnect(BLEServer* pServer) {
        handler->deviceConnected = false;
        Serial.println("BLE Client Disconnected");
        
        // Restart advertising to allow reconnection
        delay(500);
        pServer->startAdvertising();
        Serial.println("BLE Advertising restarted");
    }
};

// Characteristic callbacks for RX (receiving data)
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
    BLEHandler* handler;
public:
    CharacteristicCallbacks(BLEHandler* h) : handler(h) {}
    
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            String data = String(value.c_str());
            Serial.println("BLE Data Received:");
            Serial.println(data);
            
            if (handler->dataCallback) {
                handler->dataCallback(data);
            }
        }
    }
};

BLEHandler::BLEHandler() 
    : bleServer(nullptr)
    , txCharacteristic(nullptr)
    , rxCharacteristic(nullptr)
    , dataCallback(nullptr)
    , deviceConnected(false)
    , oldDeviceConnected(false)
{
}

void BLEHandler::begin(BLEDataCallback callback) {
    dataCallback = callback;
    
    // Initialize Preferences for storing bonded devices
    preferences.begin("ble-bonds", false);
    
    setupBLE();
    loadBondedDevices();
    
    Serial.println("BLE Handler initialized");
    Serial.printf("Device Name: %s\n", BLE_DEVICE_NAME);
    Serial.println("Waiting for BLE connection...");
}

void BLEHandler::setupBLE() {
    // Initialize BLE
    BLEDevice::init(BLE_DEVICE_NAME);
    
    // Enable encryption and bonding
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
    
    // Security settings for persistent bonding
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSecurity->setCapability(ESP_IO_CAP_NONE);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    // Create BLE Server
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks(this));
    
    // Create BLE Service
    BLEService *pService = bleServer->createService(SERVICE_UUID);
    
    // Create TX Characteristic (for sending data to app)
    txCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    txCharacteristic->addDescriptor(new BLE2902());
    
    // Create RX Characteristic (for receiving data from app)
    rxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    rxCharacteristic->setCallbacks(new CharacteristicCallbacks(this));
    
    // Start service
    pService->start();
    
    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("BLE Service started, advertising...");
}

void BLEHandler::loadBondedDevices() {
    // Load last bonded device address
    String lastDevice = preferences.getString("last_device", "");
    if (lastDevice.length() > 0) {
        Serial.printf("Last bonded device: %s\n", lastDevice.c_str());
        Serial.println("Device will auto-reconnect if in range");
    } else {
        Serial.println("No previously bonded devices");
    }
}

void BLEHandler::saveBondedDevice(const String &address) {
    preferences.putString("last_device", address);
    Serial.printf("Saved bonded device: %s\n", address.c_str());
}

void BLEHandler::sendData(const String &data) {
    if (deviceConnected && txCharacteristic) {
        // Split data if too large (BLE limit ~512 bytes per notification)
        const size_t maxChunkSize = 512;
        size_t dataLen = data.length();
        size_t offset = 0;
        
        while (offset < dataLen) {
            size_t chunkSize = min(maxChunkSize, dataLen - offset);
            String chunk = data.substring(offset, offset + chunkSize);
            
            txCharacteristic->setValue(chunk.c_str());
            txCharacteristic->notify();
            
            offset += chunkSize;
            
            if (offset < dataLen) {
                delay(20); // Small delay between chunks
            }
        }
        
        Serial.println("BLE Data Sent");
    } else {
        Serial.println("BLE not connected, cannot send data");
    }
}

bool BLEHandler::isConnected() {
    return deviceConnected;
}

String BLEHandler::getDeviceName() {
    return String(BLE_DEVICE_NAME);
}

// Global instance
BLEHandler bleHandler;
