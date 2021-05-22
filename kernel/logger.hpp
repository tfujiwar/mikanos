#pragma once

enum LogLevel {
  kError,
  kWarn,
  kInfo,
  kDebug,
};

void SetLogLevel(LogLevel level);

int Log(LogLevel level, const char *format, ...);
