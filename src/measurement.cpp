#include "measurement.h"
#include "protocol.h"
#include "config.h"

void initMeasurementData(MeasurementData &data) {
  data.weight_final = 0;
  data.weight_final_valid = false;
  data.tare_offset = 0;
  data.tare_completed = false;
  data.tare_sample_count = 0;
  data.tare_sum = 0;
  data.weight_threshold_reached = false;
  
  data.imp_right_hand = 0;
  data.imp_left_hand = 0;
  data.imp_trunk = 0;
  data.imp_right_foot = 0;
  data.imp_left_foot = 0;
  data.impedance_final_valid = false;
  
  data.lastWeightValue = 0;
  data.weightStableCount = 0;
  data.weightHasInitial = false;
  
  data.lastImpRH = 0;
  data.lastImpLH = 0;
  data.lastImpT = 0;
  data.lastImpRF = 0;
  data.lastImpLF = 0;
  data.impStableCount = 0;
  data.impHasInitial = false;
  
  data.resultPackets.reset();
}

void resetMeasurementData(MeasurementData &data) {
  data.weight_final_valid = false;
  data.impedance_final_valid = false;
  data.weightHasInitial = false;
  data.impHasInitial = false;
  data.weightStableCount = 0;
  data.impStableCount = 0;
  data.tare_offset = 0;
  data.tare_completed = false;
  data.tare_sample_count = 0;
  data.tare_sum = 0;
  data.weight_threshold_reached = false;
  
  data.resultPackets.reset();
}

void processDeviceFrame(const uint8_t *frame, size_t frameLen, 
                        MeasurementData &mData, CalibData &calib, 
                        UserInfo &userInfo, State &state)
{
  if (frameLen < 3)
    return;
  uint8_t header = frame[0];
  uint8_t lengthByte = frame[1];
  uint8_t order = frame[2];

  // Debug print
  Serial.print("RX <- ");
  for (size_t i = 0; i < frameLen; ++i)
    Serial.printf("%02X ", frame[i]);
  Serial.println();

  if (order == 0xA1)
  {
    if (frameLen >= 13)
    {
      int16_t realtimeWeight = le_i16(&frame[7]);
      uint32_t adc_raw =
          (uint32_t)frame[9] |
          ((uint32_t)frame[10] << 8) |
          ((uint32_t)frame[11] << 16) |
          ((uint32_t)frame[12] << 24);

      Serial.printf("Weight raw=%.1f kg | ADC raw=%lu\n",
                    realtimeWeight / 10.0, adc_raw);

      // Handle TARE_WEIGHT state
      if (state == TARE_WEIGHT && !mData.tare_completed)
      {
        if (mData.tare_sample_count < TARE_SAMPLES)
        {
          mData.tare_sum += (long)adc_raw;
          mData.tare_sample_count++;
          Serial.printf("Tare sample %d/%d collected\n", mData.tare_sample_count, TARE_SAMPLES);
          
          if (mData.tare_sample_count >= TARE_SAMPLES)
          {
            mData.tare_completed = true;
          }
        }
        return;
      }

      // Calculate weight
      float delta = (float)((int32_t)adc_raw - (int32_t)calib.offset - (int32_t)mData.tare_offset);
      float weight_kg = delta * calib.scale_factor;
      long usedValueForStability = (long)round(weight_kg * 10.0f);

      // Handle WAIT_SCALE_EMPTY state
      if (state == WAIT_SCALE_EMPTY)
      {
        if (weight_kg < MAX_WEIGHT_EMPTY)
        {
          Serial.printf(">>> Scale is empty (%.2f kg < %.1f kg)\n", weight_kg, MAX_WEIGHT_EMPTY);
          Serial.println("=== Ready for next measurement ===");
          Serial.println("=== Transitioning to WAIT_JSON state ===");
          Serial.println("Paste JSON to start new measurement...");
          state = WAIT_JSON;
          resetMeasurementData(mData);
          userInfo.valid = false;
        }
        else
        {
          static unsigned long lastPrintTime = 0;
          unsigned long currentTime = millis();
          if (currentTime - lastPrintTime >= 2000)
          {
            Serial.printf("Waiting for scale to be empty... current: %.2f kg\n", weight_kg);
            lastPrintTime = currentTime;
          }
        }
        return;
      }

      // Handle WAIT_FOR_WEIGHT state
      if (state == WAIT_FOR_WEIGHT && weight_kg >= MIN_WEIGHT_TO_START)
      {
        Serial.printf(">>> Weight detected: %.2f kg (threshold reached!)\n", weight_kg);
        Serial.println("=== Transitioning to SEND_A1_LOOP state ===");
        Serial.println("Now measuring weight, please stay still...");
        state = SEND_A1_LOOP;
        mData.weightHasInitial = false;
        mData.weightStableCount = 0;
        return;
      }

      if (state == WAIT_FOR_WEIGHT)
      {
        if ((int)weight_kg > 0)
        {
          Serial.printf("Waiting... current weight: %.2f kg\n", weight_kg);
        }
        return;
      }

      // Stability logic
      if (!mData.weightHasInitial)
      {
        mData.weightHasInitial = true;
        mData.lastWeightValue = usedValueForStability;
        mData.weightStableCount = 0;
      }
      else
      {
        long diff = labs(usedValueForStability - mData.lastWeightValue);
        if (diff <= STABLE_DELTA)
          mData.weightStableCount++;
        else
        {
          mData.weightStableCount = 0;
          mData.lastWeightValue = usedValueForStability;
        }
      }

      Serial.printf("ADC-based Weight=%.3f kg | stable=%d\n",
                    weight_kg, mData.weightStableCount);

      if (mData.weightStableCount >= STABLE_REQUIRED_CNT)
      {
        mData.weight_final = (long)round(weight_kg * 10.0f);
        mData.weight_final_valid = true;
        Serial.printf(">>> Weight Locked = %.2f kg\n",
                      (float)mData.weight_final / 10.0f);
      }
    }
  }
  else if (order == 0xB1)
  {
    if (frameLen >= 26)
    {
      uint8_t impState = frame[4];
      uint32_t rh = le_u32(&frame[6]);
      uint32_t lh = le_u32(&frame[10]);
      uint32_t tr = le_u32(&frame[14]);
      uint32_t rf = le_u32(&frame[18]);
      uint32_t lf = le_u32(&frame[22]);
      Serial.printf("Impedance raw: State=%02X RH=%lu LH=%lu TR=%lu RF=%lu LF=%lu\r\n", 
                    impState, rh, lh, tr, rf, lf);

      if (impState == 0x03)
      {
        if (!mData.impHasInitial)
        {
          mData.lastImpRH = rh;
          mData.lastImpLH = lh;
          mData.lastImpT = tr;
          mData.lastImpRF = rf;
          mData.lastImpLF = lf;
          mData.impHasInitial = true;
          mData.impStableCount = 0;
        }
        else
        {
          long d1 = (long)labs((long)rh - (long)mData.lastImpRH);
          long d2 = (long)labs((long)lh - (long)mData.lastImpLH);
          long d3 = (long)labs((long)tr - (long)mData.lastImpT);
          long d4 = (long)labs((long)rf - (long)mData.lastImpRF);
          long d5 = (long)labs((long)lf - (long)mData.lastImpLF);

          if (d1 <= STABLE_DELTA && d2 <= STABLE_DELTA && d3 <= STABLE_DELTA && 
              d4 <= STABLE_DELTA && d5 <= STABLE_DELTA)
          {
            mData.impStableCount++;
          }
          else
          {
            mData.impStableCount = 0;
            mData.lastImpRH = rh;
            mData.lastImpLH = lh;
            mData.lastImpT = tr;
            mData.lastImpRF = rf;
            mData.lastImpLF = lf;
          }
        }

        Serial.printf("Imp stableCount=%d\r\n", mData.impStableCount);

        if (mData.impStableCount >= STABLE_REQUIRED_CNT)
        {
          mData.imp_right_hand = rh;
          mData.imp_left_hand = lh;
          mData.imp_trunk = tr;
          mData.imp_right_foot = rf;
          mData.imp_left_foot = lf;
          mData.impedance_final_valid = true;
          Serial.println("Impedance_final locked.");
        }
      }
      else
      {
        Serial.printf("Impedance not ready (state=%02X), waiting...\r\n", impState);
      }
    }
  }
  else if (order == 0xA0)
  {
    Serial.println("Received A0 ACK frame (handshake).");
  }
  else if (order == 0xB0)
  {
    Serial.println("Received B0 ACK frame (mode ack).");
  }
  else if (order == 0xD0)
  {
    // D0 Result packets - ตรวจสอบ PackageNo ที่ byte 3
    if (frameLen < 5)
    {
      Serial.println("D0 frame too short");
      return;
    }
    
    uint8_t packageNo = frame[3];  // 0x51..0x55
    uint8_t errorType = frame[4];  // Error type
    
    Serial.printf("Received D0 result packet: PackageNo=0x%02X, Error=0x%02X\n", packageNo, errorType);
    
    // Store error type from first packet
    if (packageNo == 0x51)
    {
      mData.resultPackets.error_type = (ErrorType)errorType;
      if (errorType != 0x00)
      {
        Serial.printf("*** ERROR DETECTED: 0x%02X ***\n", errorType);
      }
    }
    
    // Validate packet length based on PackageNo
    size_t expectedLen = 0;
    switch(packageNo)
    {
      case 0x51: expectedLen = 0x50; break;  // 80 bytes
      case 0x52: expectedLen = 0x2E; break;  // 46 bytes
      case 0x53: expectedLen = 0x3A; break;  // 58 bytes
      case 0x54: expectedLen = 0x16; break;  // 22 bytes
      case 0x55: expectedLen = 0x16; break;  // 22 bytes
      default:
        Serial.printf("Unknown PackageNo: 0x%02X\n", packageNo);
        return;
    }
    
    if (frameLen != expectedLen)
    {
      Serial.printf("Length mismatch: expected %d, got %d\n", expectedLen, frameLen);
      return;
    }
    
    // เก็บ packet ตาม PackageNo
    switch(packageNo)
    {
      case 0x51:
        if (!mData.resultPackets.received1) {
          memcpy(mData.resultPackets.packet1, frame, frameLen);
          mData.resultPackets.len1 = frameLen;
          mData.resultPackets.received1 = true;
          mData.resultPackets.received_count++;
        }
        break;
      case 0x52:
        if (!mData.resultPackets.received2) {
          memcpy(mData.resultPackets.packet2, frame, frameLen);
          mData.resultPackets.len2 = frameLen;
          mData.resultPackets.received2 = true;
          mData.resultPackets.received_count++;
        }
        break;
      case 0x53:
        if (!mData.resultPackets.received3) {
          memcpy(mData.resultPackets.packet3, frame, frameLen);
          mData.resultPackets.len3 = frameLen;
          mData.resultPackets.received3 = true;
          mData.resultPackets.received_count++;
        }
        break;
      case 0x54:
        if (!mData.resultPackets.received4) {
          memcpy(mData.resultPackets.packet4, frame, frameLen);
          mData.resultPackets.len4 = frameLen;
          mData.resultPackets.received4 = true;
          mData.resultPackets.received_count++;
        }
        break;
      case 0x55:
        if (!mData.resultPackets.received5) {
          memcpy(mData.resultPackets.packet5, frame, frameLen);
          mData.resultPackets.len5 = frameLen;
          mData.resultPackets.received5 = true;
          mData.resultPackets.received_count++;
        }
        break;
    }
    
    Serial.printf("Progress: %d/%d packets received\n", 
                  mData.resultPackets.received_count, 
                  mData.resultPackets.total_packets);
  }
  else
  {
    Serial.printf("Unknown order: %02X\n", order);
  }
}

void buildAndSendFinalPacket(const UserInfo &userInfo, const MeasurementData &mData)
{
  if (!userInfo.valid)
  {
    Serial.println("User info not valid - cannot build final packet.");
    return;
  }
  if (!mData.weight_final_valid)
  {
    Serial.println("Weight measurement not ready.");
    return;
  }

  uint8_t body[29];
  memset(body, 0, sizeof(body));

  body[0] = 0x55;
  body[1] = 0x1E;
  body[2] = 0xD0;
  body[3] = userInfo.gender;
  body[4] = userInfo.product_id;
  body[5] = (uint8_t)userInfo.height;
  body[6] = userInfo.age;

  int16_t w = (int16_t)mData.weight_final;
  body[7] = (uint8_t)(w & 0xFF);
  body[8] = (uint8_t)((w >> 8) & 0xFF);

  uint16_t all_imps[10] = {
      (uint16_t)mData.imp_20k.rh,
      (uint16_t)mData.imp_20k.lh,
      (uint16_t)mData.imp_20k.trunk,
      (uint16_t)mData.imp_20k.rf,
      (uint16_t)mData.imp_20k.lf,
      (uint16_t)mData.imp_100k.rh,
      (uint16_t)mData.imp_100k.lh,
      (uint16_t)mData.imp_100k.trunk,
      (uint16_t)mData.imp_100k.rf,
      (uint16_t)mData.imp_100k.lf};

  int pos = 9;
  for (int i = 0; i < 10; ++i)
  {
    uint16_t v = all_imps[i];
    body[pos++] = (uint8_t)(v & 0xFF);
    body[pos++] = (uint8_t)((v >> 8) & 0xFF);
  }

  buildAndSend(body, sizeof(body));
  Serial.println("Final 8-Electrode packet (D0) sent. Waiting for result...");
}

// Helper function to get body type string
const char* getBodyTypeString(uint8_t typeCode)
{
  switch(typeCode)
  {
    case 0x01: return "Thin type";
    case 0x02: return "Lean muscular type";
    case 0x03: return "Muscular type";
    case 0x04: return "Bloated obesity type";
    case 0x05: return "Fat muscular type";
    case 0x06: return "Muscular fat type";
    case 0x07: return "Not athletic";
    case 0x08: return "Standard type";
    case 0x09: return "Standard muscle type";
    default: return "Unknown";
  }
}

// Helper function to get error type string
const char* getErrorTypeString(uint8_t errorCode)
{
  switch(errorCode)
  {
    case 0x00: return "No errors";
    case 0x01: return "Wrong age";
    case 0x02: return "Wrong height";
    case 0x03: return "Wrong weight";
    case 0x04: return "Wrong gender";
    case 0x05: return "User type error";
    case 0x06: return "Wrong impedance of both feet";
    case 0x07: return "Hand impedance error";
    case 0x08: return "Left whole body impedance error";
    case 0x09: return "Left hand impedance error";
    case 0x0A: return "Right hand impedance error";
    case 0x0B: return "Left foot impedance error";
    case 0x0C: return "Right foot impedance error";
    case 0x0D: return "Torso impedance error";
    default: return "Unknown error";
  }
}

// Helper to get standard level string
const char* getStdLevelString(uint8_t level)
{
  switch(level)
  {
    case 0: return "low";
    case 1: return "normal";
    case 2: return "high";
    default: return "unknown";
  }
}

void parseAndDisplayResultJSON(const ResultPackets &packets)
{
  Serial.println("\n=== MEASUREMENT RESULTS ===");
  
  // Check for errors first
  if (packets.hasError())
  {
    Serial.println("{");
    Serial.println("  \"status\": \"error\",");
    Serial.printf("  \"error_code\": \"0x%02X\",\n", packets.error_type);
    Serial.printf("  \"error_message\": \"%s\"\n", getErrorTypeString(packets.error_type));
    Serial.println("}");
    Serial.println("=========================\n");
    return;
  }
  
  Serial.println("{");
  Serial.println("  \"status\": \"success\",");
  Serial.printf("  \"total_packets\": %d,\n", packets.total_packets);
  Serial.printf("  \"received_packets\": %d,\n", packets.received_count);
  
  // ===== PACKET 1 (0x51) - Main body composition =====
  if (packets.received1 && packets.len1 == 0x50)
  {
    const uint8_t *p = packets.packet1;
    Serial.println("  \"body_composition\": {");
    
    // Weight measurements (bytes 5-40)
    Serial.printf("    \"weight_kg\": %.1f,\n", le_u16(&p[5]) / 10.0);
    Serial.printf("    \"weight_std_min_kg\": %.1f,\n", le_u16(&p[7]) / 10.0);
    Serial.printf("    \"weight_std_max_kg\": %.1f,\n", le_u16(&p[9]) / 10.0);
    
    Serial.printf("    \"moisture_kg\": %.1f,\n", le_u16(&p[11]) / 10.0);
    Serial.printf("    \"moisture_std_min_kg\": %.1f,\n", le_u16(&p[13]) / 10.0);
    Serial.printf("    \"moisture_std_max_kg\": %.1f,\n", le_u16(&p[15]) / 10.0);
    
    Serial.printf("    \"body_fat_mass_kg\": %.1f,\n", le_u16(&p[17]) / 10.0);
    Serial.printf("    \"body_fat_std_min_kg\": %.1f,\n", le_u16(&p[19]) / 10.0);
    Serial.printf("    \"body_fat_std_max_kg\": %.1f,\n", le_u16(&p[21]) / 10.0);
    
    Serial.printf("    \"protein_mass_kg\": %.1f,\n", le_u16(&p[23]) / 10.0);
    Serial.printf("    \"protein_std_min_kg\": %.1f,\n", le_u16(&p[25]) / 10.0);
    Serial.printf("    \"protein_std_max_kg\": %.1f,\n", le_u16(&p[27]) / 10.0);
    
    Serial.printf("    \"inorganic_salt_kg\": %.1f,\n", le_u16(&p[29]) / 10.0);
    Serial.printf("    \"inorganic_std_min_kg\": %.1f,\n", le_u16(&p[31]) / 10.0);
    Serial.printf("    \"inorganic_std_max_kg\": %.1f,\n", le_u16(&p[33]) / 10.0);
    
    Serial.printf("    \"lean_body_weight_kg\": %.1f,\n", le_u16(&p[35]) / 10.0);
    Serial.printf("    \"lean_body_std_min_kg\": %.1f,\n", le_u16(&p[37]) / 10.0);
    Serial.printf("    \"lean_body_std_max_kg\": %.1f,\n", le_u16(&p[39]) / 10.0);
    
    Serial.printf("    \"muscle_mass_kg\": %.1f,\n", le_u16(&p[41]) / 10.0);
    Serial.printf("    \"muscle_std_min_kg\": %.1f,\n", le_u16(&p[43]) / 10.0);
    Serial.printf("    \"muscle_std_max_kg\": %.1f,\n", le_u16(&p[45]) / 10.0);
    
    Serial.printf("    \"bone_mass_kg\": %.1f,\n", le_u16(&p[47]) / 10.0);
    Serial.printf("    \"bone_std_min_kg\": %.1f,\n", le_u16(&p[49]) / 10.0);
    Serial.printf("    \"bone_std_max_kg\": %.1f,\n", le_u16(&p[51]) / 10.0);
    
    Serial.printf("    \"skeletal_muscle_kg\": %.1f,\n", le_u16(&p[53]) / 10.0);
    Serial.printf("    \"skeletal_std_min_kg\": %.1f,\n", le_u16(&p[55]) / 10.0);
    Serial.printf("    \"skeletal_std_max_kg\": %.1f,\n", le_u16(&p[57]) / 10.0);
    
    Serial.printf("    \"intracellular_water_kg\": %.1f,\n", le_u16(&p[59]) / 10.0);
    Serial.printf("    \"ic_water_std_min_kg\": %.1f,\n", le_u16(&p[61]) / 10.0);
    Serial.printf("    \"ic_water_std_max_kg\": %.1f,\n", le_u16(&p[63]) / 10.0);
    
    Serial.printf("    \"extracellular_water_kg\": %.1f,\n", le_u16(&p[65]) / 10.0);
    Serial.printf("    \"ec_water_std_min_kg\": %.1f,\n", le_u16(&p[67]) / 10.0);
    Serial.printf("    \"ec_water_std_max_kg\": %.1f,\n", le_u16(&p[69]) / 10.0);
    
    Serial.printf("    \"body_cell_mass_kg\": %.1f,\n", le_u16(&p[71]) / 10.0);
    Serial.printf("    \"bcm_std_min_kg\": %.1f,\n", le_u16(&p[73]) / 10.0);
    Serial.printf("    \"bcm_std_max_kg\": %.1f,\n", le_u16(&p[75]) / 10.0);
    
    Serial.printf("    \"subcutaneous_fat_mass_kg\": %.1f\n", le_u16(&p[77]) / 10.0);
    Serial.println("  },");
  }
  
  // ===== PACKET 2 (0x52) - Segmental analysis =====
  if (packets.received2 && packets.len2 == 0x2E)
  {
    const uint8_t *p = packets.packet2;
    Serial.println("  \"segmental_analysis\": {");
    
    // Fat mass by segment (kg)
    Serial.println("    \"fat_mass_kg\": {");
    Serial.printf("      \"right_hand\": %.1f,\n", le_u16(&p[5]) / 10.0);
    Serial.printf("      \"left_hand\": %.1f,\n", le_u16(&p[7]) / 10.0);
    Serial.printf("      \"trunk\": %.1f,\n", le_u16(&p[9]) / 10.0);
    Serial.printf("      \"right_foot\": %.1f,\n", le_u16(&p[11]) / 10.0);
    Serial.printf("      \"left_foot\": %.1f\n", le_u16(&p[13]) / 10.0);
    Serial.println("    },");
    
    // Fat percentage by segment
    Serial.println("    \"fat_percent\": {");
    Serial.printf("      \"right_hand\": %.1f,\n", le_u16(&p[15]) / 10.0);
    Serial.printf("      \"left_hand\": %.1f,\n", le_u16(&p[17]) / 10.0);
    Serial.printf("      \"trunk\": %.1f,\n", le_u16(&p[19]) / 10.0);
    Serial.printf("      \"right_foot\": %.1f,\n", le_u16(&p[21]) / 10.0);
    Serial.printf("      \"left_foot\": %.1f\n", le_u16(&p[23]) / 10.0);
    Serial.println("    },");
    
    // Muscle mass by segment (kg)
    Serial.println("    \"muscle_mass_kg\": {");
    Serial.printf("      \"right_hand\": %.1f,\n", le_u16(&p[25]) / 10.0);
    Serial.printf("      \"left_hand\": %.1f,\n", le_u16(&p[27]) / 10.0);
    Serial.printf("      \"trunk\": %.1f,\n", le_u16(&p[29]) / 10.0);
    Serial.printf("      \"right_foot\": %.1f,\n", le_u16(&p[31]) / 10.0);
    Serial.printf("      \"left_foot\": %.1f\n", le_u16(&p[33]) / 10.0);
    Serial.println("    },");
    
    // Muscle ratio by segment
    Serial.println("    \"muscle_ratio_percent\": {");
    Serial.printf("      \"right_hand\": %.1f,\n", le_u16(&p[35]) / 10.0);
    Serial.printf("      \"left_hand\": %.1f,\n", le_u16(&p[37]) / 10.0);
    Serial.printf("      \"trunk\": %.1f,\n", le_u16(&p[39]) / 10.0);
    Serial.printf("      \"right_foot\": %.1f,\n", le_u16(&p[41]) / 10.0);
    Serial.printf("      \"left_foot\": %.1f\n", le_u16(&p[43]) / 10.0);
    Serial.println("    }");
    Serial.println("  },");
  }
  
  // ===== PACKET 3 (0x53) - Health metrics =====
  if (packets.received3 && packets.len3 == 0x3A)
  {
    const uint8_t *p = packets.packet3;
    Serial.println("  \"health_metrics\": {");
    
    Serial.printf("    \"body_score\": %d,\n", p[5]);
    Serial.printf("    \"physical_age\": %d,\n", p[6]);
    Serial.printf("    \"body_type\": %d,\n", p[7]);
    Serial.printf("    \"body_type_name\": \"%s\",\n", getBodyTypeString(p[7]));
    Serial.printf("    \"smi\": %.1f,\n", p[8] / 10.0);
    
    Serial.printf("    \"whr\": %.2f,\n", p[9] * 0.01);
    Serial.printf("    \"whr_std_min\": %.2f,\n", p[10] * 0.01);
    Serial.printf("    \"whr_std_max\": %.2f,\n", p[11] * 0.01);
    
    Serial.printf("    \"visceral_fat\": %d,\n", p[12]);
    Serial.printf("    \"vf_std_min\": %d,\n", p[13]);
    Serial.printf("    \"vf_std_max\": %d,\n", p[14]);
    
    Serial.printf("    \"obesity_percent\": %.1f,\n", le_u16(&p[15]) / 10.0);
    Serial.printf("    \"obesity_std_min\": %.1f,\n", le_u16(&p[17]) / 10.0);
    Serial.printf("    \"obesity_std_max\": %.1f,\n", le_u16(&p[19]) / 10.0);
    
    Serial.printf("    \"bmi\": %.1f,\n", le_u16(&p[21]) / 10.0);
    Serial.printf("    \"bmi_std_min\": %.1f,\n", le_u16(&p[23]) / 10.0);
    Serial.printf("    \"bmi_std_max\": %.1f,\n", le_u16(&p[25]) / 10.0);
    
    Serial.printf("    \"body_fat_percent\": %.1f,\n", le_u16(&p[27]) / 10.0);
    Serial.printf("    \"body_fat_std_min\": %.1f,\n", le_u16(&p[29]) / 10.0);
    Serial.printf("    \"body_fat_std_max\": %.1f,\n", le_u16(&p[31]) / 10.0);
    
    Serial.printf("    \"bmr_kcal\": %d,\n", le_u16(&p[33]));
    Serial.printf("    \"bmr_std_min_kcal\": %d,\n", le_u16(&p[35]));
    Serial.printf("    \"bmr_std_max_kcal\": %d,\n", le_u16(&p[37]));
    
    Serial.printf("    \"recommended_intake_kcal\": %d,\n", le_u16(&p[39]));
    Serial.printf("    \"ideal_weight_kg\": %.1f,\n", le_u16(&p[41]) / 10.0);
    Serial.printf("    \"target_weight_kg\": %.1f,\n", le_u16(&p[43]) / 10.0);
    
    // Control values (can be negative)
    int16_t weight_ctrl = (int16_t)le_u16(&p[45]);
    int16_t muscle_ctrl = (int16_t)le_u16(&p[47]);
    int16_t fat_ctrl = (int16_t)le_u16(&p[49]);
    Serial.printf("    \"weight_control_kg\": %.1f,\n", weight_ctrl / 10.0);
    Serial.printf("    \"muscle_control_kg\": %.1f,\n", muscle_ctrl / 10.0);
    Serial.printf("    \"fat_control_kg\": %.1f,\n", fat_ctrl / 10.0);
    
    Serial.printf("    \"subcutaneous_fat_percent\": %.1f,\n", le_u16(&p[51]) / 10.0);
    Serial.printf("    \"subq_std_min\": %.1f,\n", le_u16(&p[53]) / 10.0);
    Serial.printf("    \"subq_std_max\": %.1f\n", le_u16(&p[55]) / 10.0);
    Serial.println("  },");
  }
  
  // ===== PACKET 4 (0x54) - Energy consumption =====
  if (packets.received4 && packets.len4 == 0x16)
  {
    const uint8_t *p = packets.packet4;
    Serial.println("  \"energy_consumption_kcal_per_30min\": {");
    
    Serial.printf("    \"walk\": %d,\n", le_u16(&p[5]));
    Serial.printf("    \"golf\": %d,\n", le_u16(&p[7]));
    Serial.printf("    \"croquet\": %d,\n", le_u16(&p[9]));
    Serial.printf("    \"tennis_cycling_basketball\": %d,\n", le_u16(&p[11]));
    Serial.printf("    \"squash_tkd_fencing\": %d,\n", le_u16(&p[13]));
    Serial.printf("    \"mountain_climbing\": %d,\n", le_u16(&p[15]));
    Serial.printf("    \"swimming_aerobic_jog\": %d,\n", le_u16(&p[17]));
    Serial.printf("    \"badminton_table_tennis\": %d\n", le_u16(&p[19]));
    Serial.println("  },");
  }
  
  // ===== PACKET 5 (0x55) - Standard classifications =====
  if (packets.received5 && packets.len5 == 0x16)
  {
    const uint8_t *p = packets.packet5;
    Serial.println("  \"segmental_standards\": {");
    
    Serial.println("    \"fat_standard\": {");
    Serial.printf("      \"right_hand\": \"%s\",\n", getStdLevelString(p[5]));
    Serial.printf("      \"left_hand\": \"%s\",\n", getStdLevelString(p[6]));
    Serial.printf("      \"trunk\": \"%s\",\n", getStdLevelString(p[7]));
    Serial.printf("      \"right_foot\": \"%s\",\n", getStdLevelString(p[8]));
    Serial.printf("      \"left_foot\": \"%s\"\n", getStdLevelString(p[9]));
    Serial.println("    },");
    
    Serial.println("    \"muscle_standard\": {");
    Serial.printf("      \"right_hand\": \"%s\",\n", getStdLevelString(p[10]));
    Serial.printf("      \"left_hand\": \"%s\",\n", getStdLevelString(p[11]));
    Serial.printf("      \"trunk\": \"%s\",\n", getStdLevelString(p[12]));
    Serial.printf("      \"right_foot\": \"%s\",\n", getStdLevelString(p[13]));
    Serial.printf("      \"left_foot\": \"%s\"\n", getStdLevelString(p[14]));
    Serial.println("    }");
    Serial.println("  }");
  }
  
  Serial.println("}");
  Serial.println("=========================\n");
}

String generateResultJSON(const ResultPackets &packets)
{
  String json = "";
  
  // Check for errors first
  if (packets.hasError())
  {
    json += "{\n";
    json += "  \"status\": \"error\",\n";
    json += "  \"error_code\": \"0x";
    json += String(packets.error_type, HEX);
    json += "\",\n";
    json += "  \"error_message\": \"";
    json += getErrorTypeString(packets.error_type);
    json += "\"\n";
    json += "}";
    return json;
  }
  
  json += "{\n";
  json += "  \"status\": \"success\",\n";
  json += "  \"total_packets\": " + String(packets.total_packets) + ",\n";
  json += "  \"received_packets\": " + String(packets.received_count) + ",\n";
  
  // ===== PACKET 1 (0x51) - Main body composition =====
  if (packets.received1 && packets.len1 == 0x50)
  {
    const uint8_t *p = packets.packet1;
    json += "  \"body_composition\": {\n";
    
    json += "    \"weight_kg\": " + String(le_u16(&p[5]) / 10.0, 1) + ",\n";
    json += "    \"weight_std_min_kg\": " + String(le_u16(&p[7]) / 10.0, 1) + ",\n";
    json += "    \"weight_std_max_kg\": " + String(le_u16(&p[9]) / 10.0, 1) + ",\n";
    
    json += "    \"moisture_kg\": " + String(le_u16(&p[11]) / 10.0, 1) + ",\n";
    json += "    \"moisture_std_min_kg\": " + String(le_u16(&p[13]) / 10.0, 1) + ",\n";
    json += "    \"moisture_std_max_kg\": " + String(le_u16(&p[15]) / 10.0, 1) + ",\n";
    
    json += "    \"body_fat_mass_kg\": " + String(le_u16(&p[17]) / 10.0, 1) + ",\n";
    json += "    \"body_fat_std_min_kg\": " + String(le_u16(&p[19]) / 10.0, 1) + ",\n";
    json += "    \"body_fat_std_max_kg\": " + String(le_u16(&p[21]) / 10.0, 1) + ",\n";
    
    json += "    \"protein_mass_kg\": " + String(le_u16(&p[23]) / 10.0, 1) + ",\n";
    json += "    \"protein_std_min_kg\": " + String(le_u16(&p[25]) / 10.0, 1) + ",\n";
    json += "    \"protein_std_max_kg\": " + String(le_u16(&p[27]) / 10.0, 1) + ",\n";
    
    json += "    \"inorganic_salt_kg\": " + String(le_u16(&p[29]) / 10.0, 1) + ",\n";
    json += "    \"inorganic_std_min_kg\": " + String(le_u16(&p[31]) / 10.0, 1) + ",\n";
    json += "    \"inorganic_std_max_kg\": " + String(le_u16(&p[33]) / 10.0, 1) + ",\n";
    
    json += "    \"lean_body_weight_kg\": " + String(le_u16(&p[35]) / 10.0, 1) + ",\n";
    json += "    \"lean_body_std_min_kg\": " + String(le_u16(&p[37]) / 10.0, 1) + ",\n";
    json += "    \"lean_body_std_max_kg\": " + String(le_u16(&p[39]) / 10.0, 1) + ",\n";
    
    json += "    \"muscle_mass_kg\": " + String(le_u16(&p[41]) / 10.0, 1) + ",\n";
    json += "    \"muscle_std_min_kg\": " + String(le_u16(&p[43]) / 10.0, 1) + ",\n";
    json += "    \"muscle_std_max_kg\": " + String(le_u16(&p[45]) / 10.0, 1) + ",\n";
    
    json += "    \"bone_mass_kg\": " + String(le_u16(&p[47]) / 10.0, 1) + ",\n";
    json += "    \"bone_std_min_kg\": " + String(le_u16(&p[49]) / 10.0, 1) + ",\n";
    json += "    \"bone_std_max_kg\": " + String(le_u16(&p[51]) / 10.0, 1) + ",\n";
    
    json += "    \"skeletal_muscle_kg\": " + String(le_u16(&p[53]) / 10.0, 1) + ",\n";
    json += "    \"skeletal_std_min_kg\": " + String(le_u16(&p[55]) / 10.0, 1) + ",\n";
    json += "    \"skeletal_std_max_kg\": " + String(le_u16(&p[57]) / 10.0, 1) + ",\n";
    
    json += "    \"intracellular_water_kg\": " + String(le_u16(&p[59]) / 10.0, 1) + ",\n";
    json += "    \"ic_water_std_min_kg\": " + String(le_u16(&p[61]) / 10.0, 1) + ",\n";
    json += "    \"ic_water_std_max_kg\": " + String(le_u16(&p[63]) / 10.0, 1) + ",\n";
    
    json += "    \"extracellular_water_kg\": " + String(le_u16(&p[65]) / 10.0, 1) + ",\n";
    json += "    \"ec_water_std_min_kg\": " + String(le_u16(&p[67]) / 10.0, 1) + ",\n";
    json += "    \"ec_water_std_max_kg\": " + String(le_u16(&p[69]) / 10.0, 1) + ",\n";
    
    json += "    \"body_cell_mass_kg\": " + String(le_u16(&p[71]) / 10.0, 1) + ",\n";
    json += "    \"bcm_std_min_kg\": " + String(le_u16(&p[73]) / 10.0, 1) + ",\n";
    json += "    \"bcm_std_max_kg\": " + String(le_u16(&p[75]) / 10.0, 1) + ",\n";
    
    json += "    \"subcutaneous_fat_mass_kg\": " + String(le_u16(&p[77]) / 10.0, 1) + "\n";
    json += "  },\n";
  }
  
  // ===== PACKET 2 (0x52) - Segmental analysis =====
  if (packets.received2 && packets.len2 == 0x2E)
  {
    const uint8_t *p = packets.packet2;
    json += "  \"segmental_analysis\": {\n";
    
    json += "    \"fat_mass_kg\": {\n";
    json += "      \"right_hand\": " + String(le_u16(&p[5]) / 10.0, 1) + ",\n";
    json += "      \"left_hand\": " + String(le_u16(&p[7]) / 10.0, 1) + ",\n";
    json += "      \"trunk\": " + String(le_u16(&p[9]) / 10.0, 1) + ",\n";
    json += "      \"right_foot\": " + String(le_u16(&p[11]) / 10.0, 1) + ",\n";
    json += "      \"left_foot\": " + String(le_u16(&p[13]) / 10.0, 1) + "\n";
    json += "    },\n";
    
    json += "    \"fat_percent\": {\n";
    json += "      \"right_hand\": " + String(le_u16(&p[15]) / 10.0, 1) + ",\n";
    json += "      \"left_hand\": " + String(le_u16(&p[17]) / 10.0, 1) + ",\n";
    json += "      \"trunk\": " + String(le_u16(&p[19]) / 10.0, 1) + ",\n";
    json += "      \"right_foot\": " + String(le_u16(&p[21]) / 10.0, 1) + ",\n";
    json += "      \"left_foot\": " + String(le_u16(&p[23]) / 10.0, 1) + "\n";
    json += "    },\n";
    
    json += "    \"muscle_mass_kg\": {\n";
    json += "      \"right_hand\": " + String(le_u16(&p[25]) / 10.0, 1) + ",\n";
    json += "      \"left_hand\": " + String(le_u16(&p[27]) / 10.0, 1) + ",\n";
    json += "      \"trunk\": " + String(le_u16(&p[29]) / 10.0, 1) + ",\n";
    json += "      \"right_foot\": " + String(le_u16(&p[31]) / 10.0, 1) + ",\n";
    json += "      \"left_foot\": " + String(le_u16(&p[33]) / 10.0, 1) + "\n";
    json += "    },\n";
    
    json += "    \"muscle_ratio_percent\": {\n";
    json += "      \"right_hand\": " + String(le_u16(&p[35]) / 10.0, 1) + ",\n";
    json += "      \"left_hand\": " + String(le_u16(&p[37]) / 10.0, 1) + ",\n";
    json += "      \"trunk\": " + String(le_u16(&p[39]) / 10.0, 1) + ",\n";
    json += "      \"right_foot\": " + String(le_u16(&p[41]) / 10.0, 1) + ",\n";
    json += "      \"left_foot\": " + String(le_u16(&p[43]) / 10.0, 1) + "\n";
    json += "    }\n";
    json += "  },\n";
  }
  
  // ===== PACKET 3 (0x53) - Health metrics =====
  if (packets.received3 && packets.len3 == 0x3A)
  {
    const uint8_t *p = packets.packet3;
    json += "  \"health_metrics\": {\n";
    
    json += "    \"body_score\": " + String(p[5]) + ",\n";
    json += "    \"physical_age\": " + String(p[6]) + ",\n";
    json += "    \"body_type\": " + String(p[7]) + ",\n";
    json += "    \"body_type_name\": \"" + String(getBodyTypeString(p[7])) + "\",\n";
    json += "    \"smi\": " + String(p[8] / 10.0, 1) + ",\n";
    
    json += "    \"whr\": " + String(p[9] * 0.01, 2) + ",\n";
    json += "    \"whr_std_min\": " + String(p[10] * 0.01, 2) + ",\n";
    json += "    \"whr_std_max\": " + String(p[11] * 0.01, 2) + ",\n";
    
    json += "    \"visceral_fat\": " + String(p[12]) + ",\n";
    json += "    \"vf_std_min\": " + String(p[13]) + ",\n";
    json += "    \"vf_std_max\": " + String(p[14]) + ",\n";
    
    json += "    \"obesity_percent\": " + String(le_u16(&p[15]) / 10.0, 1) + ",\n";
    json += "    \"obesity_std_min\": " + String(le_u16(&p[17]) / 10.0, 1) + ",\n";
    json += "    \"obesity_std_max\": " + String(le_u16(&p[19]) / 10.0, 1) + ",\n";
    
    json += "    \"bmi\": " + String(le_u16(&p[21]) / 10.0, 1) + ",\n";
    json += "    \"bmi_std_min\": " + String(le_u16(&p[23]) / 10.0, 1) + ",\n";
    json += "    \"bmi_std_max\": " + String(le_u16(&p[25]) / 10.0, 1) + ",\n";
    
    json += "    \"body_fat_percent\": " + String(le_u16(&p[27]) / 10.0, 1) + ",\n";
    json += "    \"body_fat_std_min\": " + String(le_u16(&p[29]) / 10.0, 1) + ",\n";
    json += "    \"body_fat_std_max\": " + String(le_u16(&p[31]) / 10.0, 1) + ",\n";
    
    json += "    \"bmr_kcal\": " + String(le_u16(&p[33])) + ",\n";
    json += "    \"bmr_std_min_kcal\": " + String(le_u16(&p[35])) + ",\n";
    json += "    \"bmr_std_max_kcal\": " + String(le_u16(&p[37])) + ",\n";
    
    json += "    \"recommended_intake_kcal\": " + String(le_u16(&p[39])) + ",\n";
    json += "    \"ideal_weight_kg\": " + String(le_u16(&p[41]) / 10.0, 1) + ",\n";
    json += "    \"target_weight_kg\": " + String(le_u16(&p[43]) / 10.0, 1) + ",\n";
    
    int16_t weight_ctrl = (int16_t)le_u16(&p[45]);
    int16_t muscle_ctrl = (int16_t)le_u16(&p[47]);
    int16_t fat_ctrl = (int16_t)le_u16(&p[49]);
    json += "    \"weight_control_kg\": " + String(weight_ctrl / 10.0, 1) + ",\n";
    json += "    \"muscle_control_kg\": " + String(muscle_ctrl / 10.0, 1) + ",\n";
    json += "    \"fat_control_kg\": " + String(fat_ctrl / 10.0, 1) + ",\n";
    
    json += "    \"subcutaneous_fat_percent\": " + String(le_u16(&p[51]) / 10.0, 1) + ",\n";
    json += "    \"subq_std_min\": " + String(le_u16(&p[53]) / 10.0, 1) + ",\n";
    json += "    \"subq_std_max\": " + String(le_u16(&p[55]) / 10.0, 1) + "\n";
    json += "  },\n";
  }
  
  // ===== PACKET 4 (0x54) - Energy consumption =====
  if (packets.received4 && packets.len4 == 0x16)
  {
    const uint8_t *p = packets.packet4;
    json += "  \"energy_consumption_kcal_per_30min\": {\n";
    
    json += "    \"walk\": " + String(le_u16(&p[5])) + ",\n";
    json += "    \"golf\": " + String(le_u16(&p[7])) + ",\n";
    json += "    \"croquet\": " + String(le_u16(&p[9])) + ",\n";
    json += "    \"tennis_cycling_basketball\": " + String(le_u16(&p[11])) + ",\n";
    json += "    \"squash_tkd_fencing\": " + String(le_u16(&p[13])) + ",\n";
    json += "    \"mountain_climbing\": " + String(le_u16(&p[15])) + ",\n";
    json += "    \"swimming_aerobic_jog\": " + String(le_u16(&p[17])) + ",\n";
    json += "    \"badminton_table_tennis\": " + String(le_u16(&p[19])) + "\n";
    json += "  },\n";
  }
  
  // ===== PACKET 5 (0x55) - Standard classifications =====
  if (packets.received5 && packets.len5 == 0x16)
  {
    const uint8_t *p = packets.packet5;
    json += "  \"segmental_standards\": {\n";
    
    json += "    \"fat_standard\": {\n";
    json += "      \"right_hand\": \"" + String(getStdLevelString(p[5])) + "\",\n";
    json += "      \"left_hand\": \"" + String(getStdLevelString(p[6])) + "\",\n";
    json += "      \"trunk\": \"" + String(getStdLevelString(p[7])) + "\",\n";
    json += "      \"right_foot\": \"" + String(getStdLevelString(p[8])) + "\",\n";
    json += "      \"left_foot\": \"" + String(getStdLevelString(p[9])) + "\"\n";
    json += "    },\n";
    
    json += "    \"muscle_standard\": {\n";
    json += "      \"right_hand\": \"" + String(getStdLevelString(p[10])) + "\",\n";
    json += "      \"left_hand\": \"" + String(getStdLevelString(p[11])) + "\",\n";
    json += "      \"trunk\": \"" + String(getStdLevelString(p[12])) + "\",\n";
    json += "      \"right_foot\": \"" + String(getStdLevelString(p[13])) + "\",\n";
    json += "      \"left_foot\": \"" + String(getStdLevelString(p[14])) + "\"\n";
    json += "    }\n";
    json += "  }\n";
  }
  
  json += "}";
  return json;
}
