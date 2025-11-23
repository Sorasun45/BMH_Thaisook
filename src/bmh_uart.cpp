#include "bmh_uart.h"
#include "config.h"

#if ENABLE_BLUETOOTH
#include "BluetoothSerial.h"
static BluetoothSerial SerialBT;
#endif

static HardwareSerial BMH(BMH_UART_NUM);
static bool simulator = false;

void BMH_UART_begin(uint32_t baud) {
  if (simulator) return;
  BMH.begin(baud, SERIAL_8N1, BMH_RX_PIN, BMH_TX_PIN);
  delay(10);
#if ENABLE_BLUETOOTH
  if (!SerialBT.hasClient()) {
    SerialBT.begin("BMH-ESP32");
  }
#endif
}

void BMH_UART_write(const uint8_t *buf, size_t len) {
  if (simulator) return; // simulator uses internal injection
  BMH.write(buf, len);
  BMH.flush();
}

int BMH_UART_available() {
  if (simulator) return 0;
  return BMH.available();
}

int BMH_UART_read() {
  if (simulator) return -1;
  return BMH.read();
}

void BMH_UART_flush() {
  if (!simulator) BMH.flush();
}

void BMH_UART_enableSimulator(bool en) {
  simulator = en;
}

bool BMH_UART_isSimulator() {
  return simulator;
}

// Auto-baud: try sending a simple "get version" frame at each baud and wait for response
// Note: frame 55 05 10 05 35 (per spec) => request parameter read
uint32_t BMH_UART_autoBaud(const uint32_t *baudList, size_t nlist, unsigned long timeoutMs) {
  uint8_t probe[] = {0x55,0x05,0x10,0x05,0x35};
  unsigned long start = millis();
  for (size_t i=0;i<nlist;i++){
    uint32_t b = baudList[i];
    BMH_UART_begin(b);
    delay(50);
    BMH_UART_write(probe, sizeof(probe));
    unsigned long t0 = millis();
    while (millis() - t0 < timeoutMs) {
      if (BMH_UART_available()) {
        // We saw something -> assume success
        return b;
      }
      delay(5);
    }
    // no response, try next
  }
  return 0;
}
