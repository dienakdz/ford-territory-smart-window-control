#pragma once

#include <Arduino.h>
#include "../config/types.h"
#include "../config/config.h"

extern MotorState motorState;
extern RunMode currentRunMode;
extern unsigned long motorStartMs;

// Khoi tao cac chan BTS7960 va chan hanh trinh
void motorInit();

// Dung motor tuc thi
void motorStop();

// Chay motor theo chieu nang/ha voi deadtime bao ve va kiem tra cong tac hanh trinh
bool motorRun(bool up, RunMode requestedMode);

// Kiem tra trang thai cong tac hanh trinh vi co (NC)
bool topReached();
bool bottomReached();
bool limitsConflict();

// Giam sat hanh trinh va thoi gian chay toi da trong vong lap loop()
void monitorLimits();
void monitorRunTimeout();
