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
