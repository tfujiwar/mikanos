#pragma once

#include "window.hpp"

class Terminal {
 public:
  static const int kRows = 15, kColumns = 60;
  static const int kLineMax = 128;

  Terminal();
  unsigned int LayerID() const { return layer_id_; };
  Rectangle<int> BlinkCursor();
  Rectangle<int> InputKey(uint8_t modifier, uint8_t keycode, char ascii);

 private:
  void DrawCursor(bool visible);
  Vector2D<int> CalcCursorPos() const;
  void Scroll1();
  void Print(const char *s);
  void ExecuteLine();

  std::shared_ptr<ToplevelWindow> window_;
  unsigned int layer_id_;
  Vector2D<int> cursor_{0, 0};
  bool cursor_visible_{false};
  int linebuf_index_{0};
  std::array<char, kLineMax> linebuf_{};
};

void TaskTerminal(uint64_t task_id, int64_t data);
