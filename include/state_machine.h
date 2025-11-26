#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include "types.h"
#include "measurement.h"

// State machine context
struct StateMachineContext {
  State currentState;
  unsigned long lastPollSendMs;
  bool ack_A0_received;
  bool ack_B0_received;
  bool ack_B0_2_received;
  
  UserInfo userInfo;
  MeasurementData mData;
  CalibData calib;
};

// Initialize state machine
void initStateMachine(StateMachineContext &ctx);

// Process state machine
void processStateMachine(StateMachineContext &ctx);

// Handle JSON input
void handleJsonInput(const String &jsonStr, StateMachineContext &ctx);

#endif // STATE_MACHINE_H
