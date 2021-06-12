#pragma once

#include <cstdint>

void InitializeLAPICTimer();
void StartLAPICTimer();
void StopLAPICTimer();
uint32_t LAPICTimerElapsed();

class TimerManager {
 public:
  void Tick();
  unsigned long CurrentTick() const { return tick_; };

 private:
  volatile unsigned long tick_{0};
};

extern TimerManager *timer_manager;

void LAPICTimerInterrupt();