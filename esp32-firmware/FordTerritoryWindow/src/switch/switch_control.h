#pragma once

#include <Arduino.h>
#include "../config/types.h"
#include "../config/config.h"

extern int switchVoltageMv;
extern SwitchMode stableSwitchMode;
extern SwitchMode candidateSwitchMode;
extern unsigned long candidateSince;
extern bool requireReleaseForNormal;

// Khoi tao chan ADC cong tac
void switchInit();

// Doc dien ap ADC qua 4 mau de triet tieu nhieu va giai ma che do
SwitchMode readSwitchMode();

// Xu ly lenh cong tac theo che do hoat dong binh thuong (Manual / Auto)
void handleSwitchModeChange(SwitchMode mode);
