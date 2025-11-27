// ลำดับงาน
#include "state_machine.h"
#include "protocol.h"
#include "config.h"
#include <ArduinoJson.h>

void initStateMachine(StateMachineContext &ctx) {
  ctx.currentState = WAIT_JSON;
  ctx.lastPollSendMs = 0;
  ctx.ack_A0_received = false;
  ctx.ack_B0_received = false;
  ctx.ack_B0_2_received = false;
  ctx.userInfo.valid = false;
  initMeasurementData(ctx.mData);
}

void handleJsonInput(const String &jsonStr, StateMachineContext &ctx) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err)
  {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  if (doc.containsKey("gender"))
    ctx.userInfo.gender = (uint8_t)doc["gender"].as<int>();
  if (doc.containsKey("product_id"))
    ctx.userInfo.product_id = (uint8_t)doc["product_id"].as<int>();
  if (doc.containsKey("height"))
    ctx.userInfo.height = (uint16_t)doc["height"].as<int>();
  if (doc.containsKey("age"))
    ctx.userInfo.age = (uint8_t)doc["age"].as<int>();
  ctx.userInfo.valid = true;
  
  Serial.println("User JSON accepted:");
  Serial.printf(" gender=%u product_id=%u height=%u age=%u\n", 
                ctx.userInfo.gender, ctx.userInfo.product_id, 
                ctx.userInfo.height, ctx.userInfo.age);
  
  ctx.currentState = SEND_A0_WAIT_ACK;
  resetMeasurementData(ctx.mData);
  ctx.lastPollSendMs = 0;
  ctx.ack_A0_received = false;
  ctx.ack_B0_received = false;
  ctx.ack_B0_2_received = false;
}

void processStateMachine(StateMachineContext &ctx) {
  unsigned long now = millis();

  switch (ctx.currentState)
  {
  case WAIT_JSON:
    break;

  case SEND_A0_WAIT_ACK:
  {
    static bool sent = false;
    if (!sent)
    {
      Serial.println("Sending A0 (handshake)...");
      send_cmd_A0();
      sent = true;
      ctx.ack_A0_received = false;
    }
    if (ctx.ack_A0_received)
    {
      Serial.println("=== Transitioning to TARE_WEIGHT state ===");
      Serial.println("Please ensure the scale is empty for tare calibration...");
      sent = false;
      ctx.currentState = TARE_WEIGHT;
      ctx.mData.tare_completed = false;
      ctx.mData.tare_sample_count = 0;
      ctx.mData.tare_sum = 0;
      ctx.lastPollSendMs = millis() - POLL_INTERVAL_MS;
    }
    break;
  }

  case TARE_WEIGHT:
  {
    if (now - ctx.lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      Serial.println("Sending A1 for tare reading...");
      send_cmd_A1();
    }

    if (ctx.mData.tare_completed)
    {
      ctx.mData.tare_offset = ctx.mData.tare_sum / TARE_SAMPLES;
      Serial.printf(">>> Tare completed! Offset = %ld ADC units\n", ctx.mData.tare_offset);
      Serial.println("=== Transitioning to WAIT_FOR_WEIGHT state ===");
      Serial.printf("Please step on the scale (waiting for weight > %.1f kg)...\n", MIN_WEIGHT_TO_START);
      ctx.currentState = WAIT_FOR_WEIGHT;
      ctx.lastPollSendMs = millis() - POLL_INTERVAL_MS;
    }
    break;
  }

  case WAIT_FOR_WEIGHT:
  {
    if (now - ctx.lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      send_cmd_A1();
    }
    break;
  }

  case SEND_A1_LOOP:
  {
    if (now - ctx.lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      send_cmd_A1();
    }

    if (ctx.mData.weight_final_valid)
    {
      Serial.println("Weight stabilized. Proceeding to B0 start.");
      ctx.currentState = SEND_B0_WAIT_ACK;
    }
    break;
  }

  case SEND_B0_WAIT_ACK:
  {
    static bool sent = false;
    if (!sent)
    {
      Serial.println("Sending B0 (mode start) 03...");
      send_cmd_B0_len6_0303();
      sent = true;
      ctx.ack_B0_received = false;
    }
    if (ctx.ack_B0_received)
    {
      Serial.println("=== Transitioning to SEND_B1_LOOP state ===");
      sent = false;
      ctx.currentState = SEND_B1_LOOP;
      ctx.lastPollSendMs = millis() - POLL_INTERVAL_MS;
    }
    break;
  }

  case SEND_B1_LOOP:
  {
    if (now - ctx.lastPollSendMs >= POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      Serial.println("Sending B1 (impedance read)...");
      send_cmd_B1();
    }
    if (ctx.mData.impedance_final_valid)
    {
      ctx.mData.imp_20k.rh = ctx.mData.imp_right_hand;
      ctx.mData.imp_20k.lh = ctx.mData.imp_left_hand;
      ctx.mData.imp_20k.trunk = ctx.mData.imp_trunk;
      ctx.mData.imp_20k.rf = ctx.mData.imp_right_foot;
      ctx.mData.imp_20k.lf = ctx.mData.imp_left_foot;
      Serial.println("Impedance first-round stabilized. Sending B0 second-phase.");
      ctx.currentState = SEND_B0_2_WAIT_ACK;
    }
    break;
  }

  case SEND_B0_2_WAIT_ACK:
  {
    static bool sent = false;
    if (!sent)
    {
      Serial.println("Sending B0 second phase (01 06) ...");
      send_cmd_B0_len6_0106();
      sent = true;
      ctx.ack_B0_2_received = false;
    }
    if (ctx.ack_B0_2_received)
    {
      Serial.println("=== Transitioning to SEND_B1_LOOP2 state ===");
      sent = false;
      ctx.currentState = SEND_B1_LOOP2;
      ctx.lastPollSendMs = millis() - POLL_INTERVAL_MS;
      ctx.mData.impHasInitial = false;
      ctx.mData.impStableCount = 0;
      ctx.mData.impedance_final_valid = false;
    }
    break;
  }

  case SEND_B1_LOOP2:
  {
    if (now - ctx.lastPollSendMs >= POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      Serial.println("Sending B1 (impedance read) second round ...");
      send_cmd_B1();
    }
    if (ctx.mData.impedance_final_valid)
    {
      ctx.mData.imp_100k.rh = ctx.mData.imp_right_hand;
      ctx.mData.imp_100k.lh = ctx.mData.imp_left_hand;
      ctx.mData.imp_100k.trunk = ctx.mData.imp_trunk;
      ctx.mData.imp_100k.rf = ctx.mData.imp_right_foot;
      ctx.mData.imp_100k.lf = ctx.mData.imp_left_foot;
      Serial.println("Impedance stabilized second round. Building final packet.");
      ctx.currentState = BUILD_AND_SEND_FINAL;
    }
    break;
  }

  case BUILD_AND_SEND_FINAL:
  {
    buildAndSendFinalPacket(ctx.userInfo, ctx.mData);
    Serial.println("\n*** D0 packet sent! ***");
    Serial.println("Waiting for calculation results (5 packets)...");
    ctx.mData.resultPackets.reset();
    ctx.currentState = WAIT_RESULT_PACKETS;
    ctx.lastPollSendMs = millis();
    break;
  }

  case WAIT_RESULT_PACKETS:
  {
    // Check if all result packets received
    if (ctx.mData.resultPackets.isComplete())
    {
      Serial.println("\n*** All result packets received! ***");
      parseAndDisplayResultJSON(ctx.mData.resultPackets);
      Serial.println("Please step off the scale...");
      ctx.currentState = DONE;
      ctx.lastPollSendMs = millis();
    }
    else
    {
      // Timeout check (30 seconds)
      if (now - ctx.lastPollSendMs >= 30000)
      {
        Serial.println("\n*** Timeout waiting for result packets! ***");
        Serial.printf("Received only %d/%d packets\n", 
                     ctx.mData.resultPackets.received_count,
                     ctx.mData.resultPackets.total_packets);
        ctx.currentState = DONE;
        ctx.lastPollSendMs = millis();
      }
    }
    break;
  }

  case DONE:
  {
    if (now - ctx.lastPollSendMs >= 3000)
    {
      Serial.println("=== Transitioning to WAIT_SCALE_EMPTY state ===");
      ctx.currentState = WAIT_SCALE_EMPTY;
      ctx.lastPollSendMs = millis();
    }
    break;
  }

  case WAIT_SCALE_EMPTY:
  {
    if (now - ctx.lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      ctx.lastPollSendMs = now;
      send_cmd_A1();
    }
    break;
  }
  }
}
