#pragma once

#include <Arduino.h>
#include "../config/config.h"
#include "../config/types.h"

extern MeasurePhase currentMeasurePhase;
extern String lastTimingResult;

void timingInit();
bool startMeasureUp();
bool startMeasureDown();
void abortMeasure(const String &why = "");
void handleTimingMeasure(unsigned long now);
const char *measurePhaseName();
