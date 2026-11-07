#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../config/types.h"
#include "../config/config.h"

extern CalibrationRecord savedCal;
extern bool baselineValid;

uint32_t checksumCal(const CalibrationRecord &r);
void loadCalibration();
bool saveCalibration(int baseline, int idle, int p10, int p90, int count);
