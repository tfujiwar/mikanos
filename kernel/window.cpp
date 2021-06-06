#include <optional>
#include "logger.hpp"
#include "window.hpp"

Window::Window(int width, int height, PixelFormat shadow_format) : width_{width}, height_{height} {
  data_.resize(height);
  for (int y = 0; y < height; ++y) {
    data_[y].resize(width);
  }

  FrameBufferConfig config{};
  config.frame_buffer = nullptr;
  config.horizontal_resolution = width;
  config.vertical_resolution = height;
  config.pixel_format = shadow_format;

  if (auto err = shadow_buffer_.Initialize(config)) {
    Log(kError, "failed to initialize shadow buffer: %s at %s:%d\n", err.Name(), err.File(), err.Line());
  }
};

void Window::DrawTo(FrameBuffer &dst, Vector2D<int> pos) {
  if (!transparent_color_) {
    dst.Copy(pos, shadow_buffer_);
    return;
  }

  const auto tc = transparent_color_.value();
  auto &writer = dst.Writer();

  for (int y = std::max(0, 0 - pos.y); y < std::min(Height(), writer.Height() - pos.y); ++y) {
    for (int x = std::max(0, 0 - pos.x); x < std::min(Width(), writer.Width() - pos.x); ++x) {
      const auto c = At(Vector2D<int>{x, y});
      if (c != tc) {
        writer.Write(pos + Vector2D<int>{x, y}, c);
      }
    }
  }
};

void Window::Move(Vector2D<int> dst_pos, const Rectangle<int> &src) {
  shadow_buffer_.Move(dst_pos, src);
}

void Window::SetTransparentColor(std::optional<PixelColor> c) {
  transparent_color_ = c;
};

Window::WindowWriter *Window::Writer() {
  return &writer_;
};

const PixelColor &Window::At(Vector2D<int> pos) const {
  return data_[pos.y][pos.x];
};

void Window::Write(Vector2D<int> pos, PixelColor c) {
  data_[pos.y][pos.x] = c;
  shadow_buffer_.Writer().Write(pos, c);
}

int Window::Width() const {
  return width_;    
};

int Window::Height() const {
  return height_;
};
