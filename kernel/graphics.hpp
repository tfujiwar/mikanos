#pragma once

#include <algorithm>
#include "frame_buffer_config.hpp"

struct PixelColor {
  uint8_t r, g, b;
};

constexpr PixelColor ToColor(uint32_t c) {
  return {
    static_cast<uint8_t>((c >> 16) & 0xff),
    static_cast<uint8_t>((c >> 8) & 0xff),
    static_cast<uint8_t>(c & 0xff),
  };
}

inline bool operator==(const PixelColor &lhs, const PixelColor &rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator!=(const PixelColor &lhs, const PixelColor &rhs) {
  return !(lhs == rhs);
}

template <typename T>
struct Vector2D {
  T x, y;

  template <typename U>
  Vector2D<T> &operator +=(const Vector2D<U> &rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
};

template <typename T, typename U>
auto operator +(const Vector2D<T> &lhs, const Vector2D<U> &rhs)
    -> Vector2D<decltype(lhs.x + rhs.x)> {
  return {lhs.x + rhs.x, lhs.y + rhs.y};
}

template <typename T, typename U>
auto operator -(const Vector2D<T> &lhs, const Vector2D<U> &rhs)
    -> Vector2D<decltype(lhs.x - rhs.x)> {
  return {lhs.x - rhs.x, lhs.y - rhs.y};
}

template <typename T>
struct Rectangle {
  Vector2D<T> pos;
  Vector2D<T> size;
};

template <typename T, typename U>
Rectangle<T> operator &(const Rectangle<T> &lhs, const Rectangle<U> &rhs) {
  const auto lhs_end = lhs.pos + lhs.size;
  const auto rhs_end = rhs.pos + rhs.size;

  if (lhs_end.x < rhs.pos.x || lhs_end.y < rhs.pos.y ||
      rhs_end.x < lhs.pos.x || rhs_end.y < lhs.pos.y) {
    return {{0, 0}, {0, 0}};
  }

  Vector2D<int> new_pos = {
    std::max(lhs.pos.x, rhs.pos.x),
    std::max(lhs.pos.y, rhs.pos.y),
  };

  Vector2D<int> new_size = {
    std::min(lhs_end.x, rhs_end.x) - new_pos.x,
    std::min(lhs_end.y, rhs_end.y) - new_pos.y,
  };

  return {new_pos, new_size};
}

class PixelWriter {
 public:
  virtual ~PixelWriter() = default;
  virtual void Write(Vector2D<int> pos, const PixelColor &c) = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
};

class FrameBufferWriter : public PixelWriter {
 public:
  FrameBufferWriter(const FrameBufferConfig &config) : config_{config} {}
  virtual ~FrameBufferWriter() = default;
  virtual int Width() const override { return config_.horizontal_resolution; }
  virtual int Height() const override { return config_.vertical_resolution; }

 protected:
  uint8_t *PixelAt(Vector2D<int> pos) {
    return config_.frame_buffer + 4 * (config_.pixels_per_scan_line * pos.y + pos.x);
  }

 private:
  const FrameBufferConfig &config_;
};

class RGBResv8BitPerColorPixelWriter : public FrameBufferWriter {
 public:
  using FrameBufferWriter::FrameBufferWriter;
  virtual void Write(Vector2D<int> pos, const PixelColor &c) override;
};

class BGRResv8BitPerColorPixelWriter : public FrameBufferWriter {
 public:
  using FrameBufferWriter::FrameBufferWriter;
  virtual void Write(Vector2D<int> pos, const PixelColor &c) override;
};

void FillRectangle(PixelWriter &writer, const Vector2D<int> &pos, const Vector2D<int> &size, const PixelColor &c);
void DrawRectangle(PixelWriter &writer, const Vector2D<int> &pos, const Vector2D<int> &size, const PixelColor &c);

void DrawDesktop(PixelWriter &writer);

extern FrameBufferConfig screen_config;
extern PixelWriter *screen_writer;
Vector2D<int> ScreenSize();

void InitializeGraphics(const FrameBufferConfig &screen_config);

const PixelColor kDesktopBGColor = {45, 118, 237};
const PixelColor kDesktopFGColor = {255, 255, 255};
