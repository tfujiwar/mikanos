#pragma once

#include <cstdint>

extern "C" {
  void IoOut32(uint32_t addr, uint32_t data);
  uint32_t IoIn32(uint32_t addr);
}