/*
  BMH UART state machine for:
   - JSON input via Serial Monitor: {"gender":1,"product_id":0,"height":168,"age":23}
   - Handshake and data flow:
     1) Send 55 05 A0 01 05  -> wait for AA 05 A0 00 B1
     2) Send periodically 55 05 A1 00 05 (every 500ms) -> parse weight frames,
        detect stability (±10 units, consecutive 10 samples)
     3) When stable -> send 55 06 B0 01 03 F1 -> wait AA 05 B0 00 A1
     4) Send periodically 55 05 B1 01 F4 (every 500ms) -> parse impedance frames,
        detect stability (±10 units, consecutive 10 samples)
     5) Send 55 06 B0 01 06 EE -> wait AA 05 B0 00 A1 -> final B1 loop until stable
     6) Build final 29-byte packet (includes gender/product/height/age + weight + impedances)
        and send (checksum auto)
*/

#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h> // install ArduinoJson v6+
#include <EEPROM.h>

struct CalibData {
  float scale_factor;  // slope
  float offset;        // offset
};

CalibData calib;


HardwareSerial BMH(2); // UART2

// Serial settings
#define SERIAL_BAUD 115200
#define BMH_BAUD 115200
#define BMH_RX_PIN 16
#define BMH_TX_PIN 17

// Timings
const unsigned long POLL_INTERVAL_MS = 200; // 200 ms
const unsigned long WEIGHT_POLL_INTERVAL_MS = 200; // 200 ms for faster weight reading

// Stability thresholds
const int STABLE_DELTA = 150;       // ±150 units (increased tolerance for faster lock)
const int STABLE_REQUIRED_CNT = 30;  // consecutive samples (reduced from 10)
const float MIN_WEIGHT_TO_START = 20.0; // minimum weight in kg to start measuring
const float MAX_WEIGHT_EMPTY = 5.0;     // maximum weight in kg to consider scale empty

// Enums: states
enum State
{
  WAIT_JSON,
  SEND_A0_WAIT_ACK,
  TARE_WEIGHT,
  WAIT_FOR_WEIGHT,
  SEND_A1_LOOP,
  SEND_B0_WAIT_ACK,
  SEND_B1_LOOP,
  SEND_B0_2_WAIT_ACK,
  SEND_B1_LOOP2,
  BUILD_AND_SEND_FINAL,
  DONE,
  WAIT_SCALE_EMPTY
};

State state = WAIT_JSON;

// User JSON storage
struct UserInfo
{
  uint8_t gender = 0;
  uint8_t product_id = 0;
  uint16_t height = 0;
  uint8_t age = 0;
  bool valid = false;
} userInfo;

// Measurements
long weight_final = 0; // stored magnified (e.g. weight*10 as int)
bool weight_final_valid = false;

// Tare variables
long tare_offset = 0;  // ADC offset from tare
bool tare_completed = false;
int tare_sample_count = 0;
long tare_sum = 0;
const int TARE_SAMPLES = 20;  // จำนวนตัวอย่างที่ใช้ในการ tare
bool weight_threshold_reached = false; // flag when weight exceeds minimum threshold

// เปลี่ยนจากชุดเดียว เป็น 2 ชุด
struct ImpedanceData
{
  uint32_t rh, lh, trunk, rf, lf;
};

ImpedanceData imp_20k;  // เก็บผลรอบแรก
ImpedanceData imp_100k; // เก็บผลรอบสอง

uint32_t imp_right_hand = 0;
uint32_t imp_left_hand = 0;
uint32_t imp_trunk = 0;
uint32_t imp_right_foot = 0;
uint32_t imp_left_foot = 0;
bool impedance_final_valid = false;

// ACK flags
bool ack_A0_received = false;
bool ack_B0_received = false;
bool ack_B0_2_received = false;

// For periodic sends
unsigned long lastPollSendMs = 0;

// For stability checking (weight)
long lastWeightValue = 0;
int weightStableCount = 0;
bool weightHasInitial = false;

// For stability checking (impedance) - check each channel individually and combine
uint32_t lastImpRH = 0, lastImpLH = 0, lastImpT = 0, lastImpRF = 0, lastImpLF = 0;
int impStableCount = 0;
bool impHasInitial = false;

// RX buffer for BMH
#define RX_BUF_SIZE 512
uint8_t rxBuf[RX_BUF_SIZE];
size_t rxHead = 0; // next write index
size_t rxTail = 0; // next read index

void loadCalibration() {
  EEPROM.begin(64);
  EEPROM.get(0, calib.scale_factor); // slope
  EEPROM.get(4, calib.offset);       // offset

  Serial.printf("Loaded slope=%f offset=%f\n",
                calib.scale_factor, calib.offset);
}



// Utility: push incoming byte into circular buffer
void pushRxByte(uint8_t b)
{
  rxBuf[rxHead] = b;
  rxHead = (rxHead + 1) % RX_BUF_SIZE;
  // simple overflow handling: advance tail if full
  if (rxHead == rxTail)
  {
    rxTail = (rxTail + 1) % RX_BUF_SIZE;
  }
}

// Utility: available bytes in buffer
size_t rxAvailable()
{
  if (rxHead >= rxTail)
    return rxHead - rxTail;
  return RX_BUF_SIZE - (rxTail - rxHead);
}

// Utility: peek i-th byte (0-based) from tail (without consuming)
bool rxPeek(size_t i, uint8_t &out)
{
  if (i >= rxAvailable())
    return false;
  size_t idx = (rxTail + i) % RX_BUF_SIZE;
  out = rxBuf[idx];
  return true;
}

// Utility: read and consume one byte
bool rxRead(uint8_t &out)
{
  if (rxAvailable() == 0)
    return false;
  out = rxBuf[rxTail];
  rxTail = (rxTail + 1) % RX_BUF_SIZE;
  return true;
}

// Compute checksum like in datasheet examples:
// checksum = (~(sum of bytes before checksum & 0xFF) + 1) & 0xFF
uint8_t computeChecksum(const uint8_t *buf, size_t lenWithoutChecksum)
{
  uint32_t s = 0;
  for (size_t i = 0; i < lenWithoutChecksum; ++i)
    s += buf[i];
  uint8_t sum8 = (uint8_t)(s & 0xFF);
  uint8_t ch = (uint8_t)((~sum8 + 1) & 0xFF);
  return ch;
}

// Send raw bytes to BMH
void sendRaw(const uint8_t *data, size_t len)
{
  BMH.write(data, len);
  // also print to Serial monitor for debug
  Serial.print("TX -> ");
  for (size_t i = 0; i < len; ++i)
  {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
}

// Build command with provided bytes (without checksum), compute checksum, append and send
void buildAndSend(const uint8_t *body, size_t bodyLen)
{
  // body already includes header, length, order, data fields (but NOT checksum)
  uint8_t buf[64];
  if (bodyLen + 1 > sizeof(buf))
    return;
  memcpy(buf, body, bodyLen);
  uint8_t ch = computeChecksum(buf, bodyLen);
  buf[bodyLen] = ch;
  sendRaw(buf, bodyLen + 1);
}

// Helper: send hex string like "55 05 A1 00 05" - (we will use buildAndSend directly for correctness)
// kept for reference if needed
void sendHexString(const char *hexstr)
{
  // Not used in main flow; kept for debugging
  const char *p = hexstr;
  uint8_t outBuf[32];
  size_t outLen = 0;
  while (*p)
  {
    while (*p == ' ')
      ++p;
    if (!isxdigit(*p))
      break;
    char tmp[3] = {0, 0, 0};
    tmp[0] = *p++;
    if (isxdigit(*p))
      tmp[1] = *p++;
    else
      break;
    uint8_t v = (uint8_t)strtol(tmp, NULL, 16);
    outBuf[outLen++] = v;
  }
  if (outLen)
    sendRaw(outBuf, outLen);
}

// Function to parse a complete frame starting at rxTail if available.
// Returns true if a valid complete frame was consumed and parsed into buffer `frame`.
// frameBuf will contain the full frame (including checksum). frameLen set to length.
bool tryParseFrame(uint8_t *frameBuf, size_t &frameLen)
{
  // Need at least header + length + order + checksum minimal
  if (rxAvailable() < 3)
    return false;

  // Look for 0xAA header (answer frames from device start with 0xAA)
  // Keep advancing tail until we find 0xAA or run out
  bool found = false;
  size_t avail = rxAvailable();
  for (size_t i = 0; i < avail; ++i)
  {
    uint8_t b;
    rxPeek(i, b);
    if (b == 0xAA)
    {
      // consume preceding bytes
      for (size_t j = 0; j < i; ++j)
      {
        uint8_t tmp;
        rxRead(tmp); // drop
      }
      found = true;
      break;
    }
  }
  if (!found)
  {
    // drop all
    uint8_t tmp;
    while (rxRead(tmp))
      ;
    return false;
  }

  // Now rxTail points to header 0xAA
  if (rxAvailable() < 2)
    return false; // need length byte
  uint8_t hdr, lengthByte;
  rxPeek(0, hdr);
  rxPeek(1, lengthByte);
  // full frame length = lengthByte + 2? (depends on protocol). From examples:
  // For AA frames, lengthByte seems to be payload length (e.g. 0x0E) and the full frame size = lengthByte + 2 (header + length) ? But examples show answer length 0x0E and total bytes maybe length+2? We'll assume fullFrameLen = lengthByte + 2 (header+length) + 1 checksum = lengthByte+3
  // However in provided examples: "answer" table shows Byte 0 header, Byte1 length (0x0E), Order number at Byte2 ... Check Digit at Byte13 -> That suggests total bytes = lengthByte + 2 (header+length) + 1 checksum? Let's compute: lengthByte=0x0E (14 decimal). They list up to Byte13 (0..13) = 14 bytes total. So fullFrameLen = lengthByte (as total length) ??? To match example: lengthByte = 0x0E and total frame length is 14 bytes (0..13), meaning lengthByte equals total frame length. So in that protocol the lengthByte == fullFrameLength. Therefore the number of bytes to read = lengthByte.
  // So we'll interpret: lengthByte == total frame length; thus we must have at least lengthByte bytes available.
  size_t totalFrameLen = (size_t)lengthByte;
  // Safety: min frame length should be >= 5 or so
  if (totalFrameLen < 5)
  {
    // consume one byte to avoid infinite loop
    uint8_t tmp;
    rxRead(tmp);
    return false;
  }
  if (rxAvailable() < totalFrameLen)
    return false; // wait for more data

  // Read complete frame into frameBuf
  for (size_t i = 0; i < totalFrameLen; ++i)
  {
    rxRead(frameBuf[i]);
  }
  frameLen = totalFrameLen;

  // Verify checksum: check last byte is checksum
  if (frameLen < 1)
    return false;
  uint8_t checksum = frameBuf[frameLen - 1];
  uint8_t calc = computeChecksum(frameBuf, frameLen - 1);
  if (checksum != calc)
  {
    Serial.printf("Frame checksum mismatch: got %02X calc %02X\r\n", checksum, calc);
    return false;
  }
  // Valid frame returned
  return true;
}

// Helpers to parse little-endian values from frame
uint16_t le_u16(const uint8_t *buf)
{
  return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}
int16_t le_i16(const uint8_t *buf)
{
  return (int16_t)(buf[0] | (buf[1] << 8));
}
uint32_t le_u32(const uint8_t *buf)
{
  return (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

// Process a device frame (AA ... ) and extract weight or impedance depending on order byte
void processDeviceFrame(const uint8_t *frame, size_t frameLen)
{
  if (frameLen < 3)
    return;
  uint8_t header = frame[0];     // 0xAA
  uint8_t lengthByte = frame[1]; // total length
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
      // 1) realtimeWeight จากโมดูล (หน่วย 0.1kg)
      int16_t realtimeWeight = le_i16(&frame[7]);

      // 2) อ่านค่า ADC ดิบ (Byte 9..12)
      uint32_t adc_raw =
          (uint32_t)frame[9] |
          ((uint32_t)frame[10] << 8) |
          ((uint32_t)frame[11] << 16) |
          ((uint32_t)frame[12] << 24);

      Serial.printf("Weight raw=%.1f kg | ADC raw=%lu\n",
                    realtimeWeight / 10.0, adc_raw);

      // ถ้าอยู่ใน state TARE_WEIGHT ให้เก็บค่า ADC เพื่อคำนวณ tare
      if (state == TARE_WEIGHT && !tare_completed)
      {
        if (tare_sample_count < TARE_SAMPLES)
        {
          tare_sum += (long)adc_raw;
          tare_sample_count++;
          Serial.printf("Tare sample %d/%d collected\n", tare_sample_count, TARE_SAMPLES);
          
          if (tare_sample_count >= TARE_SAMPLES)
          {
            tare_completed = true;
          }
        }
        return; // ไม่ต้องคำนวณน้ำหนักในขั้นตอน tare
      }

      // 3) ใช้ ADC ดิบ + ค่าคาลิเบรต และหัก tare offset
      // ------------------------------
      // scale_factor = kg / ADC_count
      // adc_zero = ADC at zero load
      float delta = (float)((int32_t)adc_raw - (int32_t)calib.offset - (int32_t)tare_offset);
      float weight_kg = delta * calib.scale_factor;

      // ใช้ค่านี้เพื่อตรวจความนิ่ง (แปลงเป็นหน่วย x10)
      long usedValueForStability = (long)round(weight_kg * 10.0f);

      // Check if in WAIT_SCALE_EMPTY state and weight is below threshold
      if (state == WAIT_SCALE_EMPTY)
      {
        if (weight_kg < MAX_WEIGHT_EMPTY)
        {
          Serial.printf(">>> Scale is empty (%.2f kg < %.1f kg)\n", weight_kg, MAX_WEIGHT_EMPTY);
          Serial.println("=== Ready for next measurement ===");
          Serial.println("=== Transitioning to WAIT_JSON state ===");
          Serial.println("Paste JSON to start new measurement...");
          state = WAIT_JSON;
          // Reset all flags for new measurement
          weight_final_valid = false;
          impedance_final_valid = false;
          weightHasInitial = false;
          impHasInitial = false;
          weightStableCount = 0;
          impStableCount = 0;
          ack_A0_received = false;
          ack_B0_received = false;
          ack_B0_2_received = false;
          tare_offset = 0;
          tare_completed = false;
          tare_sample_count = 0;
          tare_sum = 0;
          weight_threshold_reached = false;
          userInfo.valid = false;
        }
        else
        {
          static unsigned long lastPrintTime = 0;
          unsigned long currentTime = millis();
          if (currentTime - lastPrintTime >= 2000) // Print every 2 seconds
          {
            Serial.printf("Waiting for scale to be empty... current: %.2f kg\n", weight_kg);
            lastPrintTime = currentTime;
          }
        }
        return;
      }

      // Check if in WAIT_FOR_WEIGHT state and weight exceeds threshold
      if (state == WAIT_FOR_WEIGHT && weight_kg >= MIN_WEIGHT_TO_START)
      {
        Serial.printf(">>> Weight detected: %.2f kg (threshold reached!)\n", weight_kg);
        Serial.println("=== Transitioning to SEND_A1_LOOP state ===");
        Serial.println("Now measuring weight, please stay still...");
        state = SEND_A1_LOOP;
        // Reset stability counters for fresh measurement
        weightHasInitial = false;
        weightStableCount = 0;
        return;
      }

      // If still in WAIT_FOR_WEIGHT, just monitor and return
      if (state == WAIT_FOR_WEIGHT)
      {
        if ((int)weight_kg > 0) // Only print if there's some weight
        {
          Serial.printf("Waiting... current weight: %.2f kg\n", weight_kg);
        }
        return;
      }

      // 4) stability logic
      if (!weightHasInitial)
      {
        weightHasInitial = true;
        lastWeightValue = usedValueForStability;
        weightStableCount = 0;
      }
      else
      {
        long diff = labs(usedValueForStability - lastWeightValue);
        if (diff <= STABLE_DELTA)
          weightStableCount++;
        else
        {
          weightStableCount = 0;
          lastWeightValue = usedValueForStability;
        }
      }

      Serial.printf("ADC-based Weight=%.3f kg | stable=%d\n",
                    weight_kg, weightStableCount);

      // 5) ถ้านิ่งพอ
      if (weightStableCount >= STABLE_REQUIRED_CNT)
      {
        weight_final = (long)round(weight_kg * 10.0f); // เก็บแบบ x10
        weight_final_valid = true;
        Serial.printf(">>> Weight Locked = %.2f kg\n",
                      (float)weight_final / 10.0f);
      }
    }
  }

  else if (order == 0xB1)
  {
    // Impedance measurement frame (eight-electrode image)
    // Byte layout from your image:
    // 0 header AA
    // 1 length
    // 2 order (B1)
    // 3 measurement frequency
    // 4 impedance state (01=initializing, 02=warming up, 03=ready/measuring)
    // 5 type of data
    // 6-9 right hand  (uint32)
    // 10-13 left hand
    // 14-17 trunk
    // 18-21 right foot
    // 22-25 left foot
    // 26 checksum
    if (frameLen >= 26)
    {
      uint8_t impState = frame[4];
      uint32_t rh = le_u32(&frame[6]);
      uint32_t lh = le_u32(&frame[10]);
      uint32_t tr = le_u32(&frame[14]);
      uint32_t rf = le_u32(&frame[18]);
      uint32_t lf = le_u32(&frame[22]);
      Serial.printf("Impedance raw: State=%02X RH=%lu LH=%lu TR=%lu RF=%lu LF=%lu\r\n", impState, rh, lh, tr, rf, lf);

      // Only process if impedance state is ready (03 = measuring)
      if (impState == 0x03)
      {
        if (!impHasInitial)
        {
          lastImpRH = rh;
          lastImpLH = lh;
          lastImpT = tr;
          lastImpRF = rf;
          lastImpLF = lf;
          impHasInitial = true;
          impStableCount = 0;
        }
        else
        {
          // determine if all channels changed <= STABLE_DELTA
          long d1 = (long)labs((long)rh - (long)lastImpRH);
          long d2 = (long)labs((long)lh - (long)lastImpLH);
          long d3 = (long)labs((long)tr - (long)lastImpT);
          long d4 = (long)labs((long)rf - (long)lastImpRF);
          long d5 = (long)labs((long)lf - (long)lastImpLF);

          if (d1 <= STABLE_DELTA && d2 <= STABLE_DELTA && d3 <= STABLE_DELTA && d4 <= STABLE_DELTA && d5 <= STABLE_DELTA)
          {
            impStableCount++;
          }
          else
          {
            impStableCount = 0;
            lastImpRH = rh;
            lastImpLH = lh;
            lastImpT = tr;
            lastImpRF = rf;
            lastImpLF = lf;
          }
        }

        Serial.printf("Imp stableCount=%d\r\n", impStableCount);

        if (impStableCount >= STABLE_REQUIRED_CNT)
        {
          imp_right_hand = rh;
          imp_left_hand = lh;
          imp_trunk = tr;
          imp_right_foot = rf;
          imp_left_foot = lf;
          impedance_final_valid = true;
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
    // ACK frame for A0 handshake; check bytes equals AA 05 A0 00 B1
    Serial.println("Received A0 ACK frame (handshake).");
    if (frameLen >= 5 && frame[3] == 0x00 && frame[4] == 0xB1)
    {
      ack_A0_received = true;
      Serial.println(">>> A0 handshake confirmed! Ready to send A1.");
    }
  }
  else if (order == 0xB0)
  {
    Serial.println("Received B0 ACK frame (mode ack).");
    if (frameLen >= 5 && frame[3] == 0x00 && frame[4] == 0xA1)
    {
      if (!ack_B0_received)
      {
        ack_B0_received = true;
        Serial.println(">>> B0 first phase confirmed!");
      }
      else
      {
        ack_B0_2_received = true;
        Serial.println(">>> B0 second phase confirmed!");
      }
    }
  }
  else
  {
    Serial.printf("Unknown order: %02X\n", order);
  }
}

// High-level send commands (body excludes checksum).
void send_cmd_A0()
{
  uint8_t body[] = {0x55, 0x05, 0xA0, 0x01, 0x05}; // note: last byte will be replaced by checksum when using buildAndSend, but body currently matches example without checksum
  // In our protocol buildAndSend expects body without checksum and will append checksum. The example already included checksum as last byte.
  // To avoid double-checksum, we'll construct the body excluding the checksum field:
  uint8_t packet[] = {0x55, 0x05, 0xA0, 0x01}; // data only, checksum appended by function
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_A1()
{
  uint8_t packet[] = {0x55, 0x05, 0xA1, 0x00};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B0_len6_0303()
{ // 55 06 B0 01 03 F1
  uint8_t packet[] = {0x55, 0x06, 0xB0, 0x01, 0x03};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B0_len6_0106()
{ // 55 06 B0 01 06 EE
  uint8_t packet[] = {0x55, 0x06, 0xB0, 0x01, 0x06};
  buildAndSend(packet, sizeof(packet));
}

void send_cmd_B1()
{ // 55 05 B1 01 F4  (without checksum version: 55 05 B1 01)
  uint8_t packet[] = {0x55, 0x05, 0xB1, 0x01};
  buildAndSend(packet, sizeof(packet));
}

// Build final packet (29 bytes) using user info, weight_final, impedance_final.
// The exact field order may need to be adjusted to your final spec image; here we follow a reasonable layout:
// [0] header 0x55
// [1] length 0x1D (29)
// [2] order e.g. 0xD1 (choose an order value; adjust if your device expects a specific one)
// Fields after: gender(1), product_id(1), height(2), age(1), weight(2, magnified), impRH(4), impLH(4), impTR(4), impRF(4), impLF(4) -> total fits into 29 bytes including checksum
void buildAndSendFinalPacket()
{
  // 1. ตรวจสอบความพร้อมของข้อมูล
  if (!userInfo.valid)
  {
    Serial.println("User info not valid - cannot build final packet.");
    return;
  }
  // เช็คว่ามีข้อมูลน้ำหนักและค่า Impedance ครบไหม
  if (!weight_final_valid)
  {
    Serial.println("Weight measurement not ready.");
    return;
  }

  // เนื่องจากเราเก็บค่าลง imp_20k/imp_100k แล้ว ตัวแปร impedance_final_valid อาจจะเป็น true แค่ของรอบล่าสุด
  // แต่ถ้า flow การทำงานมาถึงจุดนี้ได้ แสดงว่าวัดครบ 2 รอบแล้ว (เพราะผ่าน state SEND_B1_LOOP2 มาแล้ว)

  // 2. เตรียม Buffer สำหรับส่งข้อมูล
  // ความยาว 30 Bytes (Header...Checksum)
  // เราต้องเตรียมข้อมูล 29 Bytes (Header...Data) แล้วให้ฟังก์ชัน buildAndSend เติม Checksum ตัวที่ 30
  uint8_t body[29];
  memset(body, 0, sizeof(body));

  // --- HEADER ---
  body[0] = 0x55;
  body[1] = 0x1E; // Length = 30 (รวม Checksum)
  body[2] = 0xD0; // Command: 8-Electrode Algorithm Input (0xD0)

  // --- USER PROFILE ---
  body[3] = userInfo.gender;          // 1 byte
  body[4] = userInfo.product_id;      // 1 byte
  body[5] = (uint8_t)userInfo.height; // 1 byte (Low byte ของส่วนสูง)
  body[6] = userInfo.age;             // 1 byte

  // --- WEIGHT ---
  // ส่งเป็น uint16_t Little Endian (Low byte first)
  int16_t w = (int16_t)weight_final;
  body[7] = (uint8_t)(w & 0xFF);
  body[8] = (uint8_t)((w >> 8) & 0xFF);

  // --- IMPEDANCE (10 Values) ---
  // ต้องส่งทั้ง 20kHz และ 100kHz (รวม 10 ค่า)
  // ค่าที่ส่งต้องลดขนาดจาก uint32 เป็น uint16 (Low byte first)

  // สร้าง Array ชั่วคราวเก็บค่าเรียงตามลำดับสำหรับส่ง D0:
  // [RH_20k, LH_20k, Trunk_20k, RF_20k, LF_20k, RH_100k, LH_100k, Trunk_100k, RF_100k, LF_100k]
  // ดึงค่ามาจาก struct imp_20k และ imp_100k ที่คุณสร้างไว้
  uint16_t all_imps[10] = {
      (uint16_t)imp_20k.rh,
      (uint16_t)imp_20k.lh,
      (uint16_t)imp_20k.trunk,
      (uint16_t)imp_20k.rf,
      (uint16_t)imp_20k.lf,

      (uint16_t)imp_100k.rh,
      (uint16_t)imp_100k.lh,
      (uint16_t)imp_100k.trunk,
      (uint16_t)imp_100k.rf,
      (uint16_t)imp_100k.lf};

  // Loop ใส่ค่าลงใน body ตั้งแต่ index 9 ถึง 28
  int pos = 9;
  for (int i = 0; i < 10; ++i)
  {
    uint16_t v = all_imps[i];
    body[pos++] = (uint8_t)(v & 0xFF);        // Low Byte
    body[pos++] = (uint8_t)((v >> 8) & 0xFF); // High Byte
  }

  // ตอนนี้ pos ควรจะเป็น 29 พอดี (9 + 20 bytes)

  // 3. ส่งข้อมูล (ฟังก์ชันนี้จะไปเติม Checksum ที่ byte 29 ให้ครบ 30 เอง)
  buildAndSend(body, sizeof(body));
  Serial.println("Final 8-Electrode packet (D0) sent. Waiting for result...");
}

// Try processing BMH incoming bytes into frames
void pollBMHReceive()
{
  while (BMH.available())
  {
    uint8_t b = BMH.read();
    pushRxByte(b);
  }
  // Try to parse frames while possible
  uint8_t frameBuf[256];
  size_t frameLen = 0;
  while (tryParseFrame(frameBuf, frameLen))
  {
    // handle
    processDeviceFrame(frameBuf, frameLen);
    // continue; maybe more frames in buffer
  }
}

// Check for specific ack frames in buffer (AA 05 A0 00 B1) etc.
// We'll scan rxBuf for that specific pattern (non-destructively)
bool findAckPattern(const uint8_t *pattern, size_t plen)
{
  size_t avail = rxAvailable();
  for (size_t i = 0; i + plen <= avail; ++i)
  {
    bool ok = true;
    for (size_t j = 0; j < plen; ++j)
    {
      uint8_t b;
      rxPeek(i + j, b);
      if (b != pattern[j])
      {
        ok = false;
        break;
      }
    }
    if (ok)
      return true;
  }
  return false;
}

// Consumptive check: if pattern found, consume bytes up to and including the pattern's end (so we don't re-detect)
bool consumeAckPattern(const uint8_t *pattern, size_t plen)
{
  size_t avail = rxAvailable();
  for (size_t i = 0; i + plen <= avail; ++i)
  {
    bool ok = true;
    for (size_t j = 0; j < plen; ++j)
    {
      uint8_t b;
      rxPeek(i + j, b);
      if (b != pattern[j])
      {
        ok = false;
        break;
      }
    }
    if (ok)
    {
      // consume i+plen bytes
      uint8_t tmp;
      for (size_t k = 0; k < i + plen; ++k)
        rxRead(tmp);
      return true;
    }
  }
  return false;
}

void setup()
{
  Serial.begin(SERIAL_BAUD);
  delay(50);
  BMH.begin(BMH_BAUD, SERIAL_8N1, BMH_RX_PIN, BMH_TX_PIN);

  Serial.println();
  Serial.println("=== BMH05108 UART StateMachine Ready ===");
  loadCalibration();
  Serial.println("Paste JSON like: {\"gender\":1,\"product_id\":0,\"height\":168,\"age\":23}");
  Serial.println();

  // init variables
  state = WAIT_JSON;
}

String jsonBuffer = "";

void loop()
{
  // 1) read Serial Monitor for JSON input
  if (Serial.available())
  {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.length() > 0)
    {
      // try parse JSON
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, s);
      if (err)
      {
        Serial.print("JSON parse error: ");
        Serial.println(err.c_str());
      }
      else
      {
        // get fields
        if (doc.containsKey("gender"))
          userInfo.gender = (uint8_t)doc["gender"].as<int>();
        if (doc.containsKey("product_id"))
          userInfo.product_id = (uint8_t)doc["product_id"].as<int>();
        if (doc.containsKey("height"))
          userInfo.height = (uint16_t)doc["height"].as<int>();
        if (doc.containsKey("age"))
          userInfo.age = (uint8_t)doc["age"].as<int>();
        userInfo.valid = true;
        Serial.println("User JSON accepted:");
        Serial.printf(" gender=%u product_id=%u height=%u age=%u\n", userInfo.gender, userInfo.product_id, userInfo.height, userInfo.age);
        // Advance state to start handshake
        state = SEND_A0_WAIT_ACK;
        // Reset measurement flags
        weight_final_valid = false;
        impedance_final_valid = false;
        weightHasInitial = false;
        impHasInitial = false;
        weightStableCount = 0;
        impStableCount = 0;
        lastPollSendMs = 0;
        // Reset ACK flags
        ack_A0_received = false;
        ack_B0_received = false;
        ack_B0_2_received = false;
        // Reset tare flags
        tare_offset = 0;
        tare_completed = false;
        tare_sample_count = 0;
        tare_sum = 0;
        weight_threshold_reached = false;
      }
    }
  }

  // Poll incoming bytes from BMH and try parse frames
  pollBMHReceive();

  // High-level state machine
  unsigned long now = millis();

  switch (state)
  {
  case WAIT_JSON:
    // nothing to do
    break;

  case SEND_A0_WAIT_ACK:
  {
    // send A0 once then wait for ack flag
    static bool sent = false;
    if (!sent)
    {
      Serial.println("Sending A0 (handshake)...");
      // send body excluding checksum -> buildAndSend will append checksum
      uint8_t pkt[] = {0x55, 0x05, 0xA0, 0x01};
      buildAndSend(pkt, sizeof(pkt));
      sent = true;
      ack_A0_received = false;
    }
    // check ack flag
    if (ack_A0_received)
    {
      Serial.println("=== Transitioning to TARE_WEIGHT state ===");
      Serial.println("Please ensure the scale is empty for tare calibration...");
      sent = false;
      state = TARE_WEIGHT;
      tare_completed = false;
      tare_sample_count = 0;
      tare_sum = 0;
      lastPollSendMs = millis() - POLL_INTERVAL_MS; // trigger immediate send
    }
    break;
  }

  case TARE_WEIGHT:
  {
    // Send A1 to read weight for tare calibration
    if (now - lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      Serial.println("Sending A1 for tare reading...");
      send_cmd_A1();
    }

    // processDeviceFrame จะอัพเดทค่า weight มาให้เราเรื่อยๆ
    // เราต้องอ่านค่า ADC จาก frame A1 เพื่อหาค่าเฉลี่ยสำหรับ tare
    // Note: ต้องแก้ไข processDeviceFrame ให้สามารถเข้าถึงค่า adc_raw ได้
    // หรือใช้วิธีเก็บค่าจาก global variable
    
    // ถ้าได้ครบจำนวนตัวอย่างแล้ว คำนวณค่าเฉลี่ยและเก็บเป็น tare_offset
    if (tare_completed)
    {
      tare_offset = tare_sum / TARE_SAMPLES;
      Serial.printf(">>> Tare completed! Offset = %ld ADC units\n", tare_offset);
      Serial.println("=== Transitioning to WAIT_FOR_WEIGHT state ===");
      Serial.printf("Please step on the scale (waiting for weight > %.1f kg)...\n", MIN_WEIGHT_TO_START);
      state = WAIT_FOR_WEIGHT;
      lastPollSendMs = millis() - POLL_INTERVAL_MS;
    }
    break;
  }

  case WAIT_FOR_WEIGHT:
  {
    // Send A1 to monitor weight until it exceeds minimum threshold
    if (now - lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      send_cmd_A1();
    }

    // The weight calculation is done in processDeviceFrame
    // We check if current weight (after tare) exceeds MIN_WEIGHT_TO_START
    // Note: We need to access the calculated weight from processDeviceFrame
    // For now, we'll rely on a flag or check weight value
    // Let's add logic in processDeviceFrame to set a flag when weight > threshold
    
    break;
  }

  case SEND_A1_LOOP:
  {
    // Device may auto-send A1 frames or require polling
    // Send A1 frequently for faster weight reading (every 200ms)
    if (now - lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      send_cmd_A1();
    }

    // if weight_final_valid detected by processDeviceFrame -> move on
    if (weight_final_valid)
    {
      Serial.println("Weight stabilized. Proceeding to B0 start.");
      state = SEND_B0_WAIT_ACK;
    }
    break;
  }

  case SEND_B0_WAIT_ACK:
  {
    // send 55 06 B0 01 03 F1 and wait for ack flag
    static bool sent = false;
    if (!sent)
    {
      Serial.println("Sending B0 (mode start) 03...");
      send_cmd_B0_len6_0303();
      sent = true;
      ack_B0_received = false;
    }
    if (ack_B0_received)
    {
      Serial.println("=== Transitioning to SEND_B1_LOOP state ===");
      sent = false;
      state = SEND_B1_LOOP;
      lastPollSendMs = millis() - POLL_INTERVAL_MS;
    }
    break;
  }

  case SEND_B1_LOOP:
  {
    if (now - lastPollSendMs >= POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      Serial.println("Sending B1 (impedance read)...");
      send_cmd_B1();
    }
    if (impedance_final_valid)
    {
      imp_20k.rh = imp_right_hand;
      imp_20k.lh = imp_left_hand;
      imp_20k.trunk = imp_trunk;
      imp_20k.rf = imp_right_foot;
      imp_20k.lf = imp_left_foot;
      Serial.println("Impedance first-round stabilized. Sending B0 second-phase.");
      state = SEND_B0_2_WAIT_ACK;
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
      ack_B0_2_received = false;
    }
    if (ack_B0_2_received)
    {
      Serial.println("=== Transitioning to SEND_B1_LOOP2 state ===");
      sent = false;
      state = SEND_B1_LOOP2;
      lastPollSendMs = millis() - POLL_INTERVAL_MS;
      // reset impedance stability counters to require another stability check if you want
      impHasInitial = false;
      impStableCount = 0;
      impedance_final_valid = false;
    }
    break;
  }

  case SEND_B1_LOOP2:
  {
    if (now - lastPollSendMs >= POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      Serial.println("Sending B1 (impedance read) second round ...");
      send_cmd_B1();
    }
    if (impedance_final_valid)
    {
      imp_100k.rh = imp_right_hand;
      imp_100k.lh = imp_left_hand;
      imp_100k.trunk = imp_trunk;
      imp_100k.rf = imp_right_foot;
      imp_100k.lf = imp_left_foot;
      Serial.println("Impedance stabilized second round. Building final packet.");
      state = BUILD_AND_SEND_FINAL;
    }
    break;
  }

  case BUILD_AND_SEND_FINAL:
  {
    buildAndSendFinalPacket();
    Serial.println("\n*** Measurement complete! ***");
    Serial.println("Please step off the scale...");
    state = DONE;
    lastPollSendMs = millis();
    break;
  }

  case DONE:
  {
    // Show result for a moment, then transition to wait for empty scale
    if (now - lastPollSendMs >= 3000) // wait 3 seconds to show result
    {
      Serial.println("=== Transitioning to WAIT_SCALE_EMPTY state ===");
      state = WAIT_SCALE_EMPTY;
      lastPollSendMs = millis();
    }
    break;
  }

  case WAIT_SCALE_EMPTY:
  {
    // Monitor weight until scale is empty
    if (now - lastPollSendMs >= WEIGHT_POLL_INTERVAL_MS)
    {
      lastPollSendMs = now;
      send_cmd_A1();
    }
    
    // Weight check is done in processDeviceFrame
    // When weight < MAX_WEIGHT_EMPTY, we'll transition back to WAIT_JSON
    break;
  }
  } // switch

  // small delay to avoid busy loop
  delay(5);
}
