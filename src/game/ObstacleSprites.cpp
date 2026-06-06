#include "game/ObstacleSprites.h"

#include "core/Log.h"

#include "stb_image.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <string>
#include <utility>
#include <vector>

namespace cr {

namespace {

constexpr int kBgKeyLight = 238;
constexpr int kBgKeyDark = 17;
constexpr int kAlphaVisible = 12;

struct PixelBounds {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

std::vector<std::string> CandidatePaths(const char* filename) {
  std::vector<std::string> paths;
  paths.emplace_back(std::string("assets/stages/mars/obstacles/") + filename);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + "assets/stages/mars/obstacles/" + filename);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../assets/stages/mars/obstacles/") + filename);
  paths.emplace_back(std::string("../../assets/stages/mars/obstacles/") + filename);
  return paths;
}

bool IsBackgroundColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  if (a <= kAlphaVisible) return true;
  if (r >= 245 && g >= 245 && b >= 245) return true;
  if (r >= kBgKeyLight && g >= kBgKeyLight && b >= kBgKeyLight) return true;

  const int maxC = std::max({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int minC = std::min({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int sat = maxC - minC;
  if (sat <= 28) {
    const int avg = (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
    if (avg >= 88 && avg <= 252) return true;
  }

  if (r <= kBgKeyDark && g <= kBgKeyDark && b <= kBgKeyDark) return true;
  return false;
}

void RemoveBackground(std::vector<unsigned char>& rgba, int w, int h) {
  if (rgba.empty() || w <= 0 || h <= 0) return;

  const int count = w * h;
  std::vector<unsigned char> visited(static_cast<std::size_t>(count), 0);
  std::queue<std::pair<int, int>> q;

  const auto tryPush = [&](int x, int y) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    const int idx = y * w + x;
    if (visited[static_cast<std::size_t>(idx)]) return;

    const std::size_t i = static_cast<std::size_t>(idx * 4);
    if (!IsBackgroundColor(rgba[i + 0], rgba[i + 1], rgba[i + 2], rgba[i + 3])) return;

    visited[static_cast<std::size_t>(idx)] = 1;
    q.emplace(x, y);
  };

  for (int x = 0; x < w; x++) {
    tryPush(x, 0);
    tryPush(x, h - 1);
  }
  for (int y = 0; y < h; y++) {
    tryPush(0, y);
    tryPush(w - 1, y);
  }

  while (!q.empty()) {
    const auto [x, y] = q.front();
    q.pop();

    const int idx = y * w + x;
    const std::size_t i = static_cast<std::size_t>(idx * 4);
    rgba[i + 3] = 0;

    tryPush(x + 1, y);
    tryPush(x - 1, y);
    tryPush(x, y + 1);
    tryPush(x, y - 1);
  }
}

PixelBounds ComputeContentBounds(const std::vector<unsigned char>& rgba, int w, int h) {
  PixelBounds out{};
  if (rgba.empty() || w <= 0 || h <= 0) return out;

  int minX = w;
  int minY = h;
  int maxX = -1;
  int maxY = -1;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const std::size_t i = static_cast<std::size_t>((y * w + x) * 4);
      if (rgba[i + 3] > kAlphaVisible) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
      }
    }
  }

  if (maxX < minX || maxY < minY) {
    out.x = 0;
    out.y = 0;
    out.w = w;
    out.h = h;
    return out;
  }

  out.x = minX;
  out.y = minY;
  out.w = maxX - minX + 1;
  out.h = maxY - minY + 1;
  return out;
}

bool IsSolidPixel(const std::vector<unsigned char>& rgba, int w, int h, int x, int y) {
  if (x < 0 || x >= w || y < 0 || y >= h) return false;
  return rgba[static_cast<std::size_t>((y * w + x) * 4 + 3)] > kAlphaVisible;
}

void AddWhiteOutlineRing(std::vector<unsigned char>& rgba, int w, int h, int ringPx = 2) {
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

} // namespace

ObstacleSprites::~ObstacleSprites() {
  const auto destroy = [](SpriteEntry& entry) {
    if (entry.tex.texture) {
      SDL_DestroyTexture(static_cast<SDL_Texture*>(entry.tex.texture));
      entry.tex.texture = nullptr;
    }
  };
  destroy(m_normal);
  destroy(m_tall);
  destroy(m_bounce);
  destroy(m_spike);
  destroy(m_triangle);
  destroy(m_ceiling);
  destroy(m_moving);
  destroy(m_falling);
}

ObstacleSprites::SpriteTex* ObstacleSprites::TexFor(ObstacleSpriteId id) {
  switch (id) {
  case ObstacleSpriteId::Normal: return &m_normal.tex;
  case ObstacleSpriteId::Tall: return &m_tall.tex;
  case ObstacleSpriteId::Bounce: return &m_bounce.tex;
  case ObstacleSpriteId::Spike: return &m_spike.tex;
  case ObstacleSpriteId::Triangle: return &m_triangle.tex;
  case ObstacleSpriteId::Ceiling: return &m_ceiling.tex;
  case ObstacleSpriteId::Moving: return &m_moving.tex;
  case ObstacleSpriteId::Falling: return &m_falling.tex;
  }
  return nullptr;
}

const ObstacleSprites::SpriteTex* ObstacleSprites::TexFor(ObstacleSpriteId id) const {
  return const_cast<ObstacleSprites*>(this)->TexFor(id);
}

bool ObstacleSprites::LoadFile(const char* filename,
                               std::vector<unsigned char>& rgba,
                               int& w,
                               int& h,
                               CropRect& outBounds) const {
  for (const auto& path : CandidatePaths(filename)) {
    int comp = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels) continue;

    rgba.assign(pixels, pixels + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(pixels);
    RemoveBackground(rgba, w, h);
    AddWhiteOutlineRing(rgba, w, h, 2);
    const PixelBounds bounds = ComputeContentBounds(rgba, w, h);
    outBounds.x = bounds.x;
    outBounds.y = bounds.y;
    outBounds.w = bounds.w;
    outBounds.h = bounds.h;
    Log(LogLevel::Info,
        "Obstacle sprite loaded: " + path + " (" + std::to_string(w) + "x" + std::to_string(h) + ", content " +
            std::to_string(outBounds.w) + "x" + std::to_string(outBounds.h) + ")");
    return true;
  }
  return false;
}

bool ObstacleSprites::Load() {
  const auto loadOne = [&](SpriteEntry& entry) {
    entry.loaded = LoadFile(entry.filename, entry.pixels, entry.w, entry.h, entry.crop);
    if (!entry.loaded) {
      Log(LogLevel::Warn, std::string("assets/stages/mars/obstacles/") + entry.filename + " missing");
    }
    return entry.loaded;
  };

  int count = 0;
  if (loadOne(m_normal)) ++count;
  if (loadOne(m_tall)) ++count;
  if (loadOne(m_bounce)) ++count;
  if (loadOne(m_spike)) ++count;
  if (loadOne(m_triangle)) ++count;
  if (loadOne(m_ceiling)) ++count;
  if (loadOne(m_moving)) ++count;
  if (loadOne(m_falling)) ++count;

  return count > 0;
}

bool ObstacleSprites::UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, SpriteTex& out) const {
  if (!rgba || w <= 0 || h <= 0) return false;

  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<unsigned char*>(rgba),
                                                            w,
                                                            h,
                                                            32,
                                                            w * 4,
                                                            SDL_PIXELFORMAT_RGBA32);
  if (!surface) return false;

  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!tex) return false;

  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  out.texture = tex;
  out.w = w;
  out.h = h;
  return true;
}

void ObstacleSprites::EnsureUploaded(SDL_Renderer* renderer) const {
  if (m_uploadAttempted || !renderer) return;
  m_uploadAttempted = true;

  const auto uploadOne = [&](SpriteEntry& entry) {
    if (!entry.loaded || entry.pixels.empty()) return true;
    if (!UploadRgba(renderer, entry.pixels.data(), entry.w, entry.h, entry.tex)) {
      Log(LogLevel::Error, std::string("Obstacle sprite GPU upload failed: ") + entry.filename);
      return false;
    }
    entry.tex.crop = entry.crop;
    return true;
  };

  if (!uploadOne(m_normal)) return;
  if (!uploadOne(m_tall)) return;
  if (!uploadOne(m_bounce)) return;
  if (!uploadOne(m_spike)) return;
  if (!uploadOne(m_triangle)) return;
  if (!uploadOne(m_ceiling)) return;
  if (!uploadOne(m_moving)) return;
  if (!uploadOne(m_falling)) return;

  m_gpuReady = m_normal.tex.texture || m_tall.tex.texture || m_bounce.tex.texture || m_triangle.tex.texture ||
               m_ceiling.tex.texture || m_moving.tex.texture || m_falling.tex.texture || m_spike.tex.texture;
  if (m_gpuReady) Log(LogLevel::Info, "Mars obstacle sprites ready");
}

void ObstacleSprites::BlitScaled(SDL_Renderer* renderer,
                                 const SpriteTex& sprite,
                                 float screenX,
                                 float anchorY,
                                 float displayHeight,
                                 bool anchorBottom) {
  if (!sprite.texture || sprite.w <= 0 || sprite.h <= 0) return;

  const CropRect& c = sprite.crop;
  if (c.w <= 0 || c.h <= 0) return;

  const int destH = std::max(1, static_cast<int>(displayHeight));
  const int destW = std::max(1, static_cast<int>(std::lround(static_cast<float>(c.w) * static_cast<float>(destH) /
                                                                 static_cast<float>(c.h))));

  SDL_Rect src{c.x, c.y, c.w, c.h};
  SDL_Rect dst{};
  dst.w = destW;
  dst.h = destH;
  dst.x = static_cast<int>(screenX - static_cast<float>(destW) * 0.5f);
  dst.y = anchorBottom ? static_cast<int>(anchorY - static_cast<float>(destH))
                       : static_cast<int>(anchorY);

  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(sprite.texture), &src, &dst);
}

void ObstacleSprites::BlitScaledCentered(SDL_Renderer* renderer,
                                         const SpriteTex& sprite,
                                         float screenX,
                                         float screenY,
                                         float displaySize) {
  if (!sprite.texture || sprite.w <= 0 || sprite.h <= 0) return;

  const CropRect& c = sprite.crop;
  if (c.w <= 0 || c.h <= 0) return;

  const int destH = std::max(1, static_cast<int>(displaySize));
  const int destW = std::max(1, static_cast<int>(std::lround(static_cast<float>(c.w) * static_cast<float>(destH) /
                                                                 static_cast<float>(c.h))));

  SDL_Rect src{c.x, c.y, c.w, c.h};
  SDL_Rect dst{};
  dst.w = destW;
  dst.h = destH;
  dst.x = static_cast<int>(screenX - static_cast<float>(destW) * 0.5f);
  dst.y = static_cast<int>(screenY - static_cast<float>(destH) * 0.5f);

  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(sprite.texture), &src, &dst);
}

void ObstacleSprites::DrawGrounded(SDL_Renderer* renderer,
                                   ObstacleSpriteId id,
                                   float screenX,
                                   float footY,
                                   float displayHeight) const {
  if (!m_gpuReady) return;
  const SpriteTex* sprite = TexFor(id);
  if (!sprite || !sprite->texture) return;
  BlitScaled(renderer, *sprite, screenX, footY, displayHeight, true);
}

void ObstacleSprites::DrawFromTop(SDL_Renderer* renderer,
                                  ObstacleSpriteId id,
                                  float screenX,
                                  float topY,
                                  float displayHeight) const {
  if (!m_gpuReady) return;
  const SpriteTex* sprite = TexFor(id);
  if (!sprite || !sprite->texture) return;
  BlitScaled(renderer, *sprite, screenX, topY, displayHeight, false);
}

void ObstacleSprites::DrawCentered(SDL_Renderer* renderer,
                                     ObstacleSpriteId id,
                                     float screenX,
                                     float screenY,
                                     float displaySize) const {
  if (!m_gpuReady) return;
  const SpriteTex* sprite = TexFor(id);
  if (!sprite || !sprite->texture) return;
  BlitScaledCentered(renderer, *sprite, screenX, screenY, displaySize);
}

} // namespace cr
