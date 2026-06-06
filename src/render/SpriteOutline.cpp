#include "render/SpriteOutline.h"

#include <vector>

namespace cr {

namespace {

constexpr int kAlphaVisible = 12;

bool IsSolidPixel(const std::vector<unsigned char>& rgba, int w, int h, int x, int y) {
  if (x < 0 || x >= w || y < 0 || y >= h) return false;
  return rgba[static_cast<std::size_t>((y * w + x) * 4 + 3)] > kAlphaVisible;
}

} // namespace

void AddWhiteOutlineRing(std::vector<unsigned char>& rgba, int w, int h, int ringPx) {
  if (rgba.empty() || w <= 0 || h <= 0 || ringPx <= 0) return;

  const std::vector<unsigned char> src = rgba;
  const int radiusSq = (ringPx + 1) * (ringPx + 1);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
      if (src[i + 3] > kAlphaVisible) continue;

      bool nearSolid = false;
      for (int dy = -ringPx; dy <= ringPx && !nearSolid; ++dy) {
        for (int dx = -ringPx; dx <= ringPx; ++dx) {
          if (dx == 0 && dy == 0) continue;
          if (dx * dx + dy * dy > radiusSq) continue;
          if (IsSolidPixel(src, w, h, x + dx, y + dy)) {
            nearSolid = true;
            break;
          }
        }
      }

      if (nearSolid) {
        rgba[i + 0] = 255;
        rgba[i + 1] = 255;
        rgba[i + 2] = 255;
        rgba[i + 3] = 255;
      }
    }
  }
}

void AddBoldWhiteOutline(std::vector<unsigned char>& rgba, int w, int h) {
  AddWhiteOutlineRing(rgba, w, h, 3);
  AddWhiteOutlineRing(rgba, w, h, 3);
}

void AddExtraBoldWhiteOutline(std::vector<unsigned char>& rgba, int w, int h) {
  AddWhiteOutlineRing(rgba, w, h, 4);
  AddWhiteOutlineRing(rgba, w, h, 4);
  AddWhiteOutlineRing(rgba, w, h, 3);
}

} // namespace cr
