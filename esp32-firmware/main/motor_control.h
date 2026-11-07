#pragma once

#include <Arduino.h>
#include "types.h"
#include "config.h"

extern MotorState motorState;
extern RunMode currentRunMode;
extern unsigned long motorStartMs;

// Khoi tao cac chan dieu khien motor BTS7960 va hanh trinh
void motorInit();

// Dung motor an toan
void motorStop();

// Dieu khien motor nang/ha voi deadtime va kiem tra an toan
bool motorRun(bool up, RunMode requestedMode);

// Kiem tra cong tac hanh trinh (NC)
bool topReached();
bool bottomReached();
bool limitsConflict();

// Giam sat lien tuc trong loop
void monitorLimits();
void monitorRunTimeout();
