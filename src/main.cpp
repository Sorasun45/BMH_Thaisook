/*
  BMH UART state machine - Refactored Version
  
  Main entry point for the BMH body composition analyzer system.
  Handles JSON input, state machine coordination, and serial communication.
*/

#include <Arduino.h>
#include <HardwareSerial.h>

#include "config.h"
#include "types.h"
#include "calibration.h"
#include "protocol.h"
#include "buffer.h"
#include "measurement.h"
#include "state_machine.h"

HardwareSerial BMH(2); // UART2
StateMachineContext smContext;

void pollBMHReceive()
{
  while (BMH.available())
  {
    uint8_t b = BMH.read();
    pushRxByte(b);
  }
  
  uint8_t frameBuf[256];
  size_t frameLen = 0;
  while (tryParseFrame(frameBuf, frameLen))
  {
    processDeviceFrame(frameBuf, frameLen, smContext.mData, 
                      smContext.calib, smContext.userInfo, 
                      smContext.currentState);
    
    // Check for ACK frames
    if (frameLen >= 5)
    {
      uint8_t order = frameBuf[2];
      if (order == 0xA0 && frameBuf[3] == 0x00 && frameBuf[4] == 0xB1)
      {
        smContext.ack_A0_received = true;
      }
      else if (order == 0xB0 && frameBuf[3] == 0x00 && frameBuf[4] == 0xA1)
      {
        if (!smContext.ack_B0_received)
          smContext.ack_B0_received = true;
        else
          smContext.ack_B0_2_received = true;
      }
    }
  }
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(50);
  BMH.begin(BMH_BAUD, SERIAL_8N1, BMH_RX_PIN, BMH_TX_PIN);

  Serial.println();
  Serial.println("=== BMH05108 UART StateMachine Ready (Refactored) ===");
  
  loadCalibration(smContext.calib);
  initStateMachine(smContext);
  
  Serial.println("Paste JSON like: {\"gender\":1,\"product_id\":0,\"height\":168,\"age\":23}");
  Serial.println();
}

void loop()
{
  // Handle JSON input from Serial Monitor
  if (Serial.available())
  {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() > 0)
    {
      handleJsonInput(s, smContext);
    }
  }

  // Poll incoming data from BMH device
  pollBMHReceive();

  // Process state machine
  processStateMachine(smContext);

  // Small delay to avoid busy loop
  delay(5);
}
