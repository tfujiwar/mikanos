#pragma once

#include <cstdint>

void InitializeLAPICTimer();
void StartLAPICTimer();
void StopLAPICTimer();
uint32_t LAPICTimerElapsed();
