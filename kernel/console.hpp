#pragma once

#include "graphics.hpp"
#include "window.hpp"

class Console {
 public:
  static const int kRows = 25, kColumns = 80;

  Console(const PixelColor &fg_color, const PixelColor &bg_color);

  void SetWriter(PixelWriter *writer);
  void SetWindow(const std::shared_ptr<Window> &window);
  void SetLayerID(unsigned int layer_id);
  unsigned int LayerID();

  void PutString(const char *s);

 private:
  void NewLine();
  void Refresh();

  PixelWriter *writer_;
  std::shared_ptr<Window> window_;
  const PixelColor fg_color_, bg_color_;
  char buffer_[kRows][kColumns + 1];
  int cursor_row_, cursor_column_;
  unsigned int layer_id_;
};

extern Console *console;

void InitializeConsole();
