#include "calibration.h"
#include <EEPROM.h>
#include <Arduino.h>

void loadCalibration(CalibData &calib) {
  EEPROM.begin(64);
  EEPROM.get(0, calib.scale_factor); // slope
  EEPROM.get(4, calib.offset);       // offset

  Serial.printf("Loaded slope=%f offset=%f\n",
                calib.scale_factor, calib.offset);
}
