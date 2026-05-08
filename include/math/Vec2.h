#pragma once

#include <cmath>

namespace cr {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;

  constexpr Vec2() = default;
  constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

  constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
  constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
  constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
  constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }

  Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(float s) { x *= s; y *= s; return *this; }

  float LenSq() const { return x * x + y * y; }
  float Len() const { return std::sqrt(LenSq()); }
};

inline Vec2 operator*(float s, const Vec2& v) { return v * s; }

inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

inline Vec2 Normalize(const Vec2& v) {
  const float l = v.Len();
  if (l <= 1e-6f) return {0.0f, 0.0f};
  return v / l;
}

} // namespace cr
