#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../config/types.h"
#include "../config/config.h"

extern CalibrationRecord savedCal;
extern bool baselineValid;

extern unsigned long travelTimeUpMs;
extern unsigned long travelTimeDownMs;
extern bool travelTimeValid;

uint32_t checksumCal(const CalibrationRecord &r);
void loadCalibration();
bool saveCalibration(int baseline, int idle, int p10, int p90, int count);

void loadTravelTimes();
bool saveTravelTimeUp(unsigned long ms);
bool saveTravelTimeDown(unsigned long ms);
