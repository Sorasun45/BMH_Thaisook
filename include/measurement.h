#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include "types.h"
#include <Arduino.h>

// Global measurement variables
struct MeasurementData {
  // Weight
  long weight_final;
  bool weight_final_valid;
  
  // Tare
  long tare_offset;
  bool tare_completed;
  int tare_sample_count;
  long tare_sum;
  bool weight_threshold_reached;
  
  // Impedance
  ImpedanceData imp_20k;
  ImpedanceData imp_100k;
  uint32_t imp_right_hand;
  uint32_t imp_left_hand;
  uint32_t imp_trunk;
  uint32_t imp_right_foot;
  uint32_t imp_left_foot;
  bool impedance_final_valid;
  
  // Stability tracking
  long lastWeightValue;
  int weightStableCount;
  bool weightHasInitial;
  
  uint32_t lastImpRH, lastImpLH, lastImpT, lastImpRF, lastImpLF;
  int impStableCount;
  bool impHasInitial;
  
  // Result packets from device
  ResultPackets resultPackets;
};

// Initialize measurement data
void initMeasurementData(MeasurementData &data);

// Reset measurement data for new cycle
void resetMeasurementData(MeasurementData &data);

// Process incoming frame
void processDeviceFrame(const uint8_t *frame, size_t frameLen, 
                        MeasurementData &mData, CalibData &calib, 
                        UserInfo &userInfo, State &state);

// Build and send final packet
void buildAndSendFinalPacket(const UserInfo &userInfo, const MeasurementData &mData);

// Parse and display result packets as JSON
void parseAndDisplayResultJSON(const ResultPackets &packets);

#endif // MEASUREMENT_H
