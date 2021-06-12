#include "graphics.hpp"
#include "layer.hpp"
#include "mouse.hpp"
#include "window.hpp"
#include "usb/classdriver/mouse.hpp"

namespace {
  const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
  };
}

void DrawMouseCursor(PixelWriter *pixel_writer, Vector2D<int> pos) {
  for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
    for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
      if (mouse_cursor_shape[dy][dx] == '@') {
        pixel_writer->Write(pos + Vector2D<int>{dx, dy}, {0, 0, 0});
      } else if (mouse_cursor_shape[dy][dx] == '.') {
        pixel_writer->Write(pos + Vector2D<int>{dx, dy}, {255, 255, 255});
      } else {
        pixel_writer->Write(pos + Vector2D<int>{dx, dy}, kMouseTransparentColor);
      }
    }
  }
}

Mouse::Mouse(unsigned int layer_id) : layer_id_{layer_id} {}

void Mouse::OnInterrupt(uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
  const auto oldpos = pos_;
  auto newpos = pos_ + Vector2D<int>{displacement_x, displacement_y};

  newpos.x = std::min(newpos.x, ScreenSize().x - 1);
  newpos.y = std::min(newpos.y, ScreenSize().y - 1);

  pos_.x = std::max(newpos.x, 0);
  pos_.y = std::max(newpos.y, 0);

  const auto diff = pos_ - oldpos;
  layer_manager->Move(layer_id_, pos_);

  const bool previous_left_pressed = (previous_buttons_ & 0x01);
  const bool left_pressed = (buttons & 0x01);

  if (!previous_left_pressed && left_pressed) {
    auto layer = layer_manager->FindLayerByPosition(pos_, layer_id_);
    if (layer && layer->IsDraggable()) {
      drag_layer_id_ = layer->ID();
    }
  } else if (previous_left_pressed && left_pressed) {
    if (drag_layer_id_ > 0) {
      layer_manager->MoveRelative(drag_layer_id_, diff);
    }
  } else if (previous_left_pressed && !left_pressed) {
    drag_layer_id_ = 0;
  }

  previous_buttons_ = buttons;
}

void Mouse::SetPosition(Vector2D<int> pos) {}

void InitializeMouse() {
  auto mouse_window = std::make_shared<Window>(
      kMouseCursorWidth, kMouseCursorHeight, screen_config.pixel_format);

  mouse_window->SetTransparentColor(kMouseTransparentColor);
  DrawMouseCursor(mouse_window->Writer(), {0, 0});

  auto mouse_layer_id = layer_manager->NewLayer()
    .SetWindow(mouse_window)
    .ID();

  auto mouse = std::make_shared<Mouse>(mouse_layer_id);
  mouse->SetPosition({200, 200});
  layer_manager->UpDown(mouse_layer_id, std::numeric_limits<int>::max());

  usb::HIDMouseDriver::default_observer = 
      [mouse](uint8_t buttons, int8_t displacement_x, int8_t displacement_y) {
        mouse->OnInterrupt(buttons, displacement_x, displacement_y);
      };
}
