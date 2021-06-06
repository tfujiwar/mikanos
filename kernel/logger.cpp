#include <cstddef>
#include <cstdio>
#include "console.hpp"
#include "logger.hpp"
#include "timer.hpp"

extern Console *console;

namespace {
  LogLevel log_level = kWarn;
}

void SetLogLevel(LogLevel level) {
  log_level = level;
}

int Log(LogLevel level, const char *format, ...) {
  if (level > log_level) {
    return 0;
  }

  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  StartLAPICTimer();
  console->PutString(s);
  auto elapsed = LAPICTimerElapsed();
  StopLAPICTimer();

  sprintf(s, "[%9d] ", elapsed);
  console->PutString(s);

  return result;
}
