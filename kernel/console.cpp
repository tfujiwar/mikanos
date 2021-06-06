#include <cstring>

#include "console.hpp"
#include "font.hpp"
#include "layer.hpp"

Console::Console(const PixelColor &fg_color, const PixelColor &bg_color) :
  fg_color_{fg_color}, bg_color_{bg_color}, buffer_{}, cursor_row_{0}, cursor_column_{0} {}

void Console::PutString(const char *s) {
  while (*s) {
    if (*s == '\n') {
      NewLine();
    } else if (cursor_column_ < kColumns - 1) {
      WriteAscii(*writer_, {cursor_column_ * 8, cursor_row_ * 16}, *s, fg_color_);
      buffer_[cursor_row_][cursor_column_] = *s;
      ++cursor_column_;
    }
    ++s;
  }

  if (layer_manager) {
    layer_manager->Draw();
  }
}

void Console::NewLine() {
  cursor_column_ = 0;
  if (cursor_row_ < kRows - 1) {
    ++cursor_row_;
    return;
  }

  if (window_) {
    Rectangle<int> move_src{{0, 16}, {kColumns * 8, (kRows - 1) * 16}};
    window_->Move({0, 0}, move_src);
    FillRectangle(*writer_, {0, (kRows - 1) * 16}, {kColumns * 8, 16}, bg_color_);
  } else {
    FillRectangle(*writer_, {0, 0}, {kColumns * 8, kRows * 16}, bg_color_);
    for (int row = 0; row < kRows; ++row) {
      memcpy(buffer_[row], buffer_[row + 1], kColumns + 1);
      WriteString(*writer_, {0, row * 16}, buffer_[row], fg_color_);
    }
    memset(buffer_[kRows - 1], 0, kColumns + 1);
  }
}

void Console::SetWriter(PixelWriter *writer) {
  if (writer == writer_) {
    return;
  }
  writer_ = writer;
  Refresh();
}

void Console::SetWindow(const std::shared_ptr<Window> &window) {
  window_ = window;
  writer_ = window->Writer();
  Refresh();
}

void Console::Refresh() {
  for (int row = 0; row < kRows; ++row) {
    WriteString(*writer_, {0, row * 16}, buffer_[row], fg_color_);
  }
}
