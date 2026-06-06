#include "render/VfxLibrary.h"

#include "core/Log.h"

#include "stb_image.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace cr {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kBgKeyLight = 238;
constexpr int kBgKeyDark = 17;
constexpr int kAlphaVisible = 12;

void DrawCircleOutlineThick(SDL_Renderer* r,
                            float cx,
                            float cy,
                            float radius,
                            Uint8 cr,
                            Uint8 cg,
                            Uint8 cb,
                            Uint8 alpha,
                            int thickness,
                            int segments = 64) {
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  for (int t = 0; t < thickness; ++t) {
    const float rr = radius - static_cast<float>(t) * 0.5f;
    int px = static_cast<int>(cx + rr);
    int py = static_cast<int>(cy);
    for (int i = 1; i <= segments; ++i) {
      const float ang = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * kPi;
      const int nx = static_cast<int>(cx + std::cos(ang) * rr);
      const int ny = static_cast<int>(cy + std::sin(ang) * rr);
      SDL_RenderDrawLine(r, px, py, nx, ny);
      px = nx;
      py = ny;
    }
  }
}

void DrawFilledDisc(SDL_Renderer* r, float cx, float cy, float radius, Uint8 cr, Uint8 cg, Uint8 cb, Uint8 alpha) {
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  const int ir = static_cast<int>(radius);
  const int icx = static_cast<int>(cx);
  const int icy = static_cast<int>(cy);
  for (int y = -ir; y <= ir; ++y) {
    const float wy = static_cast<float>(y);
    const float halfW = std::sqrt(std::max(0.0f, radius * radius - wy * wy));
    SDL_RenderDrawLine(r, icx - static_cast<int>(halfW), icy + y, icx + static_cast<int>(halfW), icy + y);
  }
}

std::vector<std::string> CandidatePaths(const char* subpath) {
  std::vector<std::string> paths;
  paths.emplace_back(std::string("assets/vfx/") + subpath);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + "assets/vfx/" + subpath);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../assets/vfx/") + subpath);
  paths.emplace_back(std::string("../../assets/vfx/") + subpath);
  return paths;
}

bool IsEdgeKeyColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  if (a <= kAlphaVisible) return true;
  // White / near-white export background (preferred artist workflow).
  if (r >= 245 && g >= 245 && b >= 245) return true;
  if (r >= kBgKeyLight && g >= kBgKeyLight && b >= kBgKeyLight) return true;

  // Checkerboard / flat gray backdrop (common in AI PNG exports).
  const int maxC = std::max({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int minC = std::min({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int sat = maxC - minC;
  if (sat <= 36) {
    const int avg = (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
    if (avg >= 96 && avg <= 252) return true;
  }

  if (r <= kBgKeyDark && g <= kBgKeyDark && b <= kBgKeyDark) return true;
  return false;
}

bool IsBackgroundColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  return IsEdgeKeyColor(r, g, b, a);
}

void FloodKeyFromEdges(std::vector<unsigned char>& rgba, int w, int h) {
  if (rgba.empty() || w <= 0 || h <= 0) return;

  std::vector<unsigned char> marked(static_cast<std::size_t>(w * h), 0);
  std::vector<int> queue;
  queue.reserve(static_cast<std::size_t>(w * h));

  const auto tryPush = [&](int x, int y) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    const int i = y * w + x;
    if (marked[static_cast<std::size_t>(i)] != 0) return;
    const std::size_t pi = static_cast<std::size_t>(i) * 4;
    const unsigned char r = rgba[pi];
    const unsigned char g = rgba[pi + 1];
    const unsigned char b = rgba[pi + 2];
    const unsigned char a = rgba[pi + 3];
    if (!IsEdgeKeyColor(r, g, b, a)) return;
    marked[static_cast<std::size_t>(i)] = 1;
    queue.push_back(i);
  };

  for (int x = 0; x < w; ++x) {
    tryPush(x, 0);
    tryPush(x, h - 1);
  }
  for (int y = 0; y < h; ++y) {
    tryPush(0, y);
    tryPush(w - 1, y);
  }

  for (std::size_t qi = 0; qi < queue.size(); ++qi) {
    const int i = queue[qi];
    const int x = i % w;
    const int y = i / w;
    rgba[static_cast<std::size_t>(i) * 4 + 3] = 0;
    tryPush(x - 1, y);
    tryPush(x + 1, y);
    tryPush(x, y - 1);
    tryPush(x, y + 1);
  }
}

bool ValidateCornerTransparency(const std::vector<unsigned char>& rgba, int w, int h, float minClearRatio) {
  if (rgba.empty() || w <= 0 || h <= 0) return false;

  const int sample = std::max(8, std::min(w, h) / 8);
  const auto cornerClear = [&](int x0, int y0) {
    int clear = 0;
    int total = 0;
    for (int y = y0; y < y0 + sample && y < h; ++y) {
      for (int x = x0; x < x0 + sample && x < w; ++x) {
        const std::size_t pi = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                static_cast<std::size_t>(x)) *
                               4;
        if (rgba[pi + 3] <= kAlphaVisible) ++clear;
        ++total;
      }
    }
    return total > 0 && static_cast<float>(clear) / static_cast<float>(total) >= minClearRatio;
  };

  return cornerClear(0, 0) && cornerClear(w - sample, 0) && cornerClear(0, h - sample) &&
         cornerClear(w - sample, h - sample);
}

void RemoveBackground(std::vector<unsigned char>& rgba, int w, int h) {
  if (rgba.empty() || w <= 0 || h <= 0) return;

  for (int i = 0; i < w * h; ++i) {
    const std::size_t pi = static_cast<std::size_t>(i) * 4;
    if (IsBackgroundColor(rgba[pi], rgba[pi + 1], rgba[pi + 2], rgba[pi + 3])) {
      rgba[pi + 3] = 0;
    }
  }

  FloodKeyFromEdges(rgba, w, h);
}

bool LoadRgbaFile(const char* filename, std::vector<unsigned char>& rgba, int& w, int& h) {
  for (const auto& path : CandidatePaths(filename)) {
    int comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) continue;

    rgba.assign(data, data + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(data);
    RemoveBackground(rgba, w, h);
    return true;
  }
  return false;
}

bool UploadRgba(SDL_Renderer* renderer,
                const std::vector<unsigned char>& rgba,
                int w,
                int h,
                SDL_Texture*& outTex,
                SDL_BlendMode blend) {
  if (rgba.empty() || w <= 0 || h <= 0) return false;

  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormatFrom(const_cast<unsigned char*>(rgba.data()), w, h, 32, w * 4,
                                         SDL_PIXELFORMAT_RGBA8888);
  if (!surface) return false;

  outTex = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!outTex) return false;

  SDL_SetTextureBlendMode(outTex, blend);
  return true;
}

void DrawClockRingTicks(SDL_Renderer* r,
                        float cx,
                        float cy,
                        float outerR,
                        float spinAngle,
                        float fade) {
  const Uint8 ringA = static_cast<Uint8>(fade * 215.0f);
  DrawCircleOutlineThick(r, cx, cy, outerR, 195, 75, 255, ringA, 3);
  DrawCircleOutlineThick(r, cx, cy, outerR * 0.76f, 90, 190, 255, static_cast<Uint8>(fade * 38.0f), 1);

  constexpr int kTicks = 36;
  for (int i = 0; i < kTicks; ++i) {
    const bool major = (i % 3 == 0);
    const float a = spinAngle * 0.14f + static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kTicks));
    const float r0 = outerR * (major ? 0.68f : 0.80f);
    const float r1 = outerR * 0.97f;
    const Uint8 tickA = static_cast<Uint8>(fade * (major ? 230.0f : 125.0f));
    if (major) {
      SDL_SetRenderDrawColor(r, 210, 110, 255, tickA);
    } else {
      SDL_SetRenderDrawColor(r, 120, 215, 255, tickA);
    }
    SDL_RenderDrawLine(r,
                       static_cast<int>(cx + std::cos(a) * r0),
                       static_cast<int>(cy + std::sin(a) * r0),
                       static_cast<int>(cx + std::cos(a) * r1),
                       static_cast<int>(cy + std::sin(a) * r1));
  }
}

void DrawBlackHoleAccretionSparks(SDL_Renderer* r, float cx, float cy, float outerR, float spinAngle, float fade) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  constexpr int kCount = 36;
  for (int i = 0; i < kCount; ++i) {
    const float phase = static_cast<float>(i) * 1.414f;
    const float a = spinAngle * 0.18f + phase;
    const float u = 0.34f + 0.58f * (0.5f + 0.5f * std::sin(phase * 1.7f));
    const float rad = outerR * u;
    const float px = cx + std::cos(a) * rad;
    const float py = cy + std::sin(a) * rad;
    const int tone = i % 10;
    Uint8 sr = 70;
    Uint8 sg = 35;
    Uint8 sb = 110;
    float alphaScale = 0.55f;
    if (tone == 0) {
      sr = 90;
      sg = 120;
      sb = 210;
      alphaScale = 0.45f;
    } else if (tone == 1) {
      sr = 170;
      sg = 70;
      sb = 190;
      alphaScale = 0.50f;
    } else if (tone == 2) {
      sr = 120;
      sg = 55;
      sb = 150;
      alphaScale = 0.42f;
    }
    const Uint8 sparkA = static_cast<Uint8>(fade * alphaScale * (55.0f + static_cast<float>(i % 40)));
    SDL_SetRenderDrawColor(r, sr, sg, sb, sparkA);
    SDL_RenderDrawPoint(r, static_cast<int>(px), static_cast<int>(py));
    if ((i & 7) == 0) {
      SDL_RenderDrawPoint(r, static_cast<int>(px) + 1, static_cast<int>(py));
    }
  }
}

void DrawBlackHoleDarkBackdrop(SDL_Renderer* r,
                               float cx,
                               float cy,
                               float radius,
                               float strength,
                               float fade) {
  if (strength <= 0.01f || fade <= 0.0f) return;

  const float s = std::clamp(strength, 0.0f, 1.0f) * fade;
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  DrawFilledDisc(r, cx, cy, radius * 1.18f, 38, 30, 62, static_cast<Uint8>(s * 48.0f));
  DrawFilledDisc(r, cx, cy, radius * 1.02f, 52, 40, 82, static_cast<Uint8>(s * 72.0f));
  DrawFilledDisc(r, cx, cy, radius * 0.88f, 62, 48, 98, static_cast<Uint8>(s * 88.0f));
  DrawCircleOutlineThick(r, cx, cy, radius * 1.08f, 130, 95, 175, static_cast<Uint8>(s * 95.0f), 2);
  DrawCircleOutlineThick(r, cx, cy, radius * 1.16f, 95, 155, 210, static_cast<Uint8>(s * 42.0f), 1);
}

void DrawCircularSpiralVortex(SDL_Renderer* r,
                              float cx,
                              float cy,
                              float maxR,
                              float spinAngle,
                              float fade) {
  if (fade <= 0.0f) return;

  auto drawSpiralLayer = [&](int strands,
                             int segs,
                             float rInner,
                             float rOuter,
                             float turns,
                             float spinRate,
                             Uint8 cr,
                             Uint8 cg,
                             Uint8 cb,
                             float alphaMul,
                             SDL_BlendMode blend,
                             int skipMod) {
    SDL_SetRenderDrawBlendMode(r, blend);
    for (int strand = 0; strand < strands; ++strand) {
      const float phase = static_cast<float>(strand) * 2.399963f;
      for (int s = 0; s < segs; ++s) {
        if (skipMod > 0 && ((strand * 17 + s * 13) % skipMod) == 0) continue;

        const float u0 = static_cast<float>(s) / static_cast<float>(segs);
        const float u1 = static_cast<float>(s + 1) / static_cast<float>(segs);
        const float r0 = maxR * (rInner + (rOuter - rInner) * u0);
        const float r1 = maxR * (rInner + (rOuter - rInner) * u1);
        const float a0 = spinAngle * spinRate + phase + u0 * turns * 2.0f * kPi;
        const float a1 = spinAngle * spinRate + phase + u1 * turns * 2.0f * kPi;
        const float falloff = (1.0f - u0 * 0.88f) * fade * alphaMul;
        const Uint8 lineA = static_cast<Uint8>(falloff * 255.0f);
        if (lineA < 5) continue;

        const int x0 = static_cast<int>(cx + std::cos(a0) * r0);
        const int y0 = static_cast<int>(cy + std::sin(a0) * r0);
        const int x1 = static_cast<int>(cx + std::cos(a1) * r1);
        const int y1 = static_cast<int>(cy + std::sin(a1) * r1);

        SDL_SetRenderDrawColor(r, cr, cg, cb, lineA);
        SDL_RenderDrawLine(r, x0, y0, x1, y1);
        if (s % 6 == 0 && u0 > 0.18f) {
          SDL_SetRenderDrawColor(r, cr, cg, cb, static_cast<Uint8>(lineA * 0.55f));
          SDL_RenderDrawLine(r, x0 + 1, y0, x1 + 1, y1);
        }
      }
    }
  };

  // Dense inward spiral body — circular, dark purple / violet base.
  drawSpiralLayer(8, 44, 0.10f, 0.97f, 3.0f, 0.24f, 42, 20, 72, 0.82f, SDL_BLENDMODE_BLEND, 0);
  drawSpiralLayer(7, 40, 0.14f, 0.93f, 2.7f, 0.20f, 58, 30, 98, 0.74f, SDL_BLENDMODE_BLEND, 12);
  drawSpiralLayer(6, 36, 0.18f, 0.88f, 2.4f, 0.26f, 88, 38, 128, 0.66f, SDL_BLENDMODE_BLEND, 10);
  drawSpiralLayer(5, 32, 0.22f, 0.82f, 2.1f, 0.22f, 118, 48, 158, 0.58f, SDL_BLENDMODE_BLEND, 9);

  // Long wispy filaments for painterly streaks.
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  constexpr int kFilaments = 32;
  for (int i = 0; i < kFilaments; ++i) {
    if ((i % 5) == 0) continue;
    const float phase = static_cast<float>(i) * 2.513f + spinAngle * 0.18f;
    const float startU = 0.52f + static_cast<float>(i % 9) * 0.05f;
    const float endU = 0.10f + static_cast<float>((i * 3) % 7) * 0.025f;
    const int steps = 5 + (i % 4);
    const Uint8 cr = static_cast<Uint8>(70 + (i % 50));
    const Uint8 cg = static_cast<Uint8>(28 + (i % 40));
    const Uint8 cb = static_cast<Uint8>(95 + (i % 55));
    int px = static_cast<int>(cx + std::cos(phase) * maxR * startU);
    int py = static_cast<int>(cy + std::sin(phase) * maxR * startU);
    for (int step = 1; step <= steps; ++step) {
      const float t = static_cast<float>(step) / static_cast<float>(steps);
      const float u = startU + (endU - startU) * t;
      const float ang = phase + t * 1.35f * kPi;
      const int nx = static_cast<int>(cx + std::cos(ang) * maxR * u);
      const int ny = static_cast<int>(cy + std::sin(ang) * maxR * u);
      const Uint8 lineA = static_cast<Uint8>(fade * (120.0f - t * 70.0f));
      SDL_SetRenderDrawColor(r, cr, cg, cb, lineA);
      SDL_RenderDrawLine(r, px, py, nx, ny);
      px = nx;
      py = ny;
    }
  }

  // Sparse cyan/teal highlights on the outer spiral (keeps current dark color ratio).
  drawSpiralLayer(4, 26, 0.38f, 0.95f, 1.8f, 0.30f, 75, 170, 205, 0.24f, SDL_BLENDMODE_ADD, 7);
  drawSpiralLayer(3, 20, 0.45f, 0.88f, 1.4f, 0.28f, 90, 185, 220, 0.16f, SDL_BLENDMODE_ADD, 5);
}

void DrawOrbitPixelSparks(SDL_Renderer* r, float cx, float cy, float outerR, float spinAngle, float fade) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
  constexpr int kCount = 140;
  for (int i = 0; i < kCount; ++i) {
    const float phase = static_cast<float>(i) * 1.618f;
    const float a = spinAngle * (1.08f + 0.03f * static_cast<float>(i % 9)) + phase;
    const float u = 0.18f + 0.78f * (0.5f + 0.5f * std::sin(phase * 2.1f));
    const float rad = outerR * u;
    const float px = cx + std::cos(a) * rad;
    const float py = cy + std::sin(a) * rad;
    const int half = (i % 7 == 0) ? 2 : 1;
    const int tone = i % 10;
    Uint8 sr = 255;
    Uint8 sg = 230;
    Uint8 sb = 150;
    if (tone == 0 || tone == 1) {
      sr = 255;
      sg = static_cast<Uint8>(200 + (i % 40));
      sb = static_cast<Uint8>(70 + (i % 50));
    } else if (tone == 2 || tone == 3) {
      sr = sg = sb = static_cast<Uint8>(220 + (i % 35));
    } else if (tone == 4) {
      sr = 90;
      sg = static_cast<Uint8>(180 + (i % 60));
      sb = 255;
    } else {
      sr = static_cast<Uint8>(180 + (i % 60));
      sg = static_cast<Uint8>(80 + (i % 50));
      sb = 255;
    }
    const Uint8 sparkA = static_cast<Uint8>(fade * (140.0f + static_cast<float>(i % 80)));
    SDL_SetRenderDrawColor(r, sr, sg, sb, sparkA);
    SDL_Rect rc{static_cast<int>(px) - half, static_cast<int>(py) - half, half * 2, half * 2};
    SDL_RenderFillRect(r, &rc);
    if (half >= 2 && (i % 11 == 0)) {
      SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(sparkA * 0.65f));
      SDL_RenderDrawLine(r, static_cast<int>(px) - 3, static_cast<int>(py), static_cast<int>(px) + 3,
                         static_cast<int>(py));
      SDL_RenderDrawLine(r, static_cast<int>(px), static_cast<int>(py) - 3, static_cast<int>(px),
                         static_cast<int>(py) + 3);
    }
  }
}

void DrawClockSpiralTunnel(SDL_Renderer* r,
                           float cx,
                           float cy,
                           float maxRadius,
                           float spinAngle,
                           float fade) {
  if (fade <= 0.0f) return;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  constexpr int kArms = 4;
  constexpr int kSegs = 160;
  for (int arm = 0; arm < kArms; ++arm) {
    const float armBase = static_cast<float>(arm) * (2.0f * kPi / static_cast<float>(kArms));
    for (int s = 0; s < kSegs; ++s) {
      const float u0 = static_cast<float>(s) / static_cast<float>(kSegs);
      const float u1 = static_cast<float>(s + 1) / static_cast<float>(kSegs);
      const float r0 = maxRadius * std::pow(0.06f, u0 * 0.92f);
      const float r1 = maxRadius * std::pow(0.06f, u1 * 0.92f);
      const float a0 = spinAngle * 0.55f + armBase + u0 * 9.5f * kPi;
      const float a1 = spinAngle * 0.55f + armBase + u1 * 9.5f * kPi;
      const float falloff = (1.0f - u0 * 0.55f) * fade;
      const Uint8 coreA = static_cast<Uint8>(falloff * 255.0f);
      if (coreA < 6) continue;

      const int x0 = static_cast<int>(cx + std::cos(a0) * r0);
      const int y0 = static_cast<int>(cy + std::sin(a0) * r0);
      const int x1 = static_cast<int>(cx + std::cos(a1) * r1);
      const int y1 = static_cast<int>(cy + std::sin(a1) * r1);

      const int thick = (s % 16 == 0) ? 4 : (s % 4 == 0 ? 3 : 2);
      for (int oy = -thick; oy <= thick; ++oy) {
        for (int ox = -thick; ox <= thick; ++ox) {
          const Uint8 haloA = static_cast<Uint8>(coreA * 0.35f);
          SDL_SetRenderDrawColor(r, 255, 245, 210, haloA);
          SDL_RenderDrawLine(r, x0 + ox, y0 + oy, x1 + ox, y1 + oy);
          SDL_SetRenderDrawColor(r, 255, 195, 70, coreA);
          SDL_RenderDrawLine(r, x0 + ox, y0 + oy, x1 + ox, y1 + oy);
        }
      }

      if (s % 10 == 0 && r0 > maxRadius * 0.08f) {
        const float tx = std::cos(a0 + kPi * 0.5f);
        const float ty = std::sin(a0 + kPi * 0.5f);
        const float tickLen = maxRadius * 0.028f;
        SDL_SetRenderDrawColor(r, 255, 230, 150, static_cast<Uint8>(coreA * 0.95f));
        SDL_RenderDrawLine(r,
                           static_cast<int>(cx + std::cos(a0) * r0),
                           static_cast<int>(cy + std::sin(a0) * r0),
                           static_cast<int>(cx + std::cos(a0) * r0 + tx * tickLen),
                           static_cast<int>(cy + std::sin(a0) * r0 + ty * tickLen));
      }
    }
  }

  for (int ring = 0; ring < 6; ++ring) {
    const float rr = maxRadius * (0.18f + static_cast<float>(ring) * 0.13f);
    const Uint8 ringA = static_cast<Uint8>(fade * (150.0f - static_cast<float>(ring) * 18.0f));
    DrawCircleOutlineThick(r, cx, cy, rr, 235, 185, 80, ringA, 1);
  }
}

} // namespace

VfxLibrary& VfxLibrary::Instance() {
  static VfxLibrary lib;
  return lib;
}

float VfxLibrary::Envelope(float normalizedLife, float fadeInFrac, float fadeOutFrac) {
  const float t = std::clamp(normalizedLife, 0.0f, 1.0f);
  if (t <= fadeInFrac) return fadeInFrac > 0.0f ? t / fadeInFrac : 1.0f;
  if (t >= 1.0f - fadeOutFrac) {
    const float outT = (1.0f - t) / std::max(fadeOutFrac, 0.001f);
    return std::clamp(outT, 0.0f, 1.0f);
  }
  return 1.0f;
}

bool VfxLibrary::Init(SDL_Renderer* renderer) {
  if (m_ready) return true;
  if (!renderer) return false;

  m_ready = CreateSoftGlowTexture(renderer, m_texSize) && CreateRingTexture(renderer, m_texSize) &&
            CreateSparkTexture(renderer, 64);
  if (!m_ready) {
    Shutdown();
    return false;
  }

  LoadBlackHoleAssets(renderer);
  return m_ready;
}

void VfxLibrary::Shutdown() {
  auto destroyAsset = [](AssetTex& a) {
    if (a.tex) SDL_DestroyTexture(a.tex);
    a = {};
  };

  destroyAsset(m_assetSoftGlow);
  destroyAsset(m_assetAccretionRing);
  destroyAsset(m_assetShockwave);
  destroyAsset(m_assetEmberSheet);
  destroyAsset(m_assetSprite);

  if (m_softGlow) SDL_DestroyTexture(m_softGlow);
  if (m_ringGlow) SDL_DestroyTexture(m_ringGlow);
  if (m_spark) SDL_DestroyTexture(m_spark);
  m_softGlow = nullptr;
  m_ringGlow = nullptr;
  m_spark = nullptr;
  m_ready = false;
}

bool VfxLibrary::LoadBlackHoleAssets(SDL_Renderer* renderer) {
  const auto loadOne = [&](const char* file, AssetTex& slot, int frames, SDL_BlendMode blend) {
    std::vector<unsigned char> rgba;
    int w = 0;
    int h = 0;
    if (!LoadRgbaFile(file, rgba, w, h)) return;

    if (UploadRgba(renderer, rgba, w, h, slot.tex, blend)) {
      slot.w = w;
      slot.h = h;
      slot.frameCount = frames;
      Log(LogLevel::Info, std::string("Loaded VFX: assets/vfx/") + file);
    }
  };

  loadOne("blackhole_soft_glow.png", m_assetSoftGlow, 1, SDL_BLENDMODE_ADD);
  loadOne("blackhole_accretion_ring.png", m_assetAccretionRing, 1, SDL_BLENDMODE_ADD);
  loadOne("blackhole_shockwave_ring.png", m_assetShockwave, 1, SDL_BLENDMODE_ADD);
  loadOne("blackhole_sprite.png", m_assetSprite, 1, SDL_BLENDMODE_ADD);
  loadOne("blackhole_ember_sheet.png", m_assetEmberSheet, 4, SDL_BLENDMODE_ADD);

  return m_assetAccretionRing.tex != nullptr;
}

void VfxLibrary::DrawAsset(SDL_Renderer* r,
                           const AssetTex& asset,
                           float cx,
                           float cy,
                           float displayRadius,
                           Uint8 alpha,
                           float rotationDeg,
                           SDL_BlendMode blend,
                           int frameIndex,
                           Uint8 cr,
                           Uint8 cg,
                           Uint8 cb) const {
  if (!asset.tex || alpha == 0 || displayRadius <= 0.0f) return;

  SDL_SetTextureBlendMode(asset.tex, blend);
  SDL_SetTextureColorMod(asset.tex, cr, cg, cb);
  SDL_SetTextureAlphaMod(asset.tex, alpha);

  const int d = static_cast<int>(displayRadius * 2.0f);
  SDL_Rect dst{static_cast<int>(cx - displayRadius), static_cast<int>(cy - displayRadius), d, d};

  SDL_Rect src{};
  if (asset.frameCount > 1) {
    const int frameW = asset.w / asset.frameCount;
    const int idx = std::clamp(frameIndex, 0, asset.frameCount - 1);
    src = {idx * frameW, 0, frameW, asset.h};
  }

  SDL_RenderCopyEx(r, asset.tex, asset.frameCount > 1 ? &src : nullptr, &dst, rotationDeg, nullptr,
                   SDL_FLIP_NONE);
}

bool VfxLibrary::CreateSoftGlowTexture(SDL_Renderer* renderer, int size) {
  std::vector<Uint32> pixels(static_cast<std::size_t>(size * size), 0);
  const float center = static_cast<float>(size) * 0.5f;
  const float invR = 1.0f / center;

  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = (static_cast<float>(x) + 0.5f - center) * invR;
      const float dy = (static_cast<float>(y) + 0.5f - center) * invR;
      const float d = std::sqrt(dx * dx + dy * dy);
      if (d >= 1.0f) continue;
      const float a = std::pow(1.0f - d, 2.4f);
      const auto alpha = static_cast<Uint8>(a * 255.0f);
      pixels[static_cast<std::size_t>(y * size + x)] =
          (static_cast<Uint32>(alpha) << 24) | 0x00FFFFFFu;
    }
  }

  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(), size, size, 32, size * 4, SDL_PIXELFORMAT_RGBA8888);
  if (!surface) return false;

  m_softGlow = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!m_softGlow) return false;

  SDL_SetTextureBlendMode(m_softGlow, SDL_BLENDMODE_ADD);
  return true;
}

bool VfxLibrary::CreateRingTexture(SDL_Renderer* renderer, int size) {
  std::vector<Uint32> pixels(static_cast<std::size_t>(size * size), 0);
  const float center = static_cast<float>(size) * 0.5f;
  const float invR = 1.0f / center;

  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = (static_cast<float>(x) + 0.5f - center) * invR;
      const float dy = (static_cast<float>(y) + 0.5f - center) * invR;
      const float d = std::sqrt(dx * dx + dy * dy);
      const float ring = std::exp(-std::pow((d - 0.72f) / 0.11f, 2.0f) * 2.5f);
      const float outerFade = std::clamp((1.05f - d) / 0.35f, 0.0f, 1.0f);
      const float innerFade = std::clamp((d - 0.18f) / 0.18f, 0.0f, 1.0f);
      const float a = ring * outerFade * innerFade;
      if (a <= 0.01f) continue;
      const auto alpha = static_cast<Uint8>(std::min(a, 1.0f) * 255.0f);
      pixels[static_cast<std::size_t>(y * size + x)] =
          (static_cast<Uint32>(alpha) << 24) | 0x00FFFFFFu;
    }
  }

  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(), size, size, 32, size * 4, SDL_PIXELFORMAT_RGBA8888);
  if (!surface) return false;

  m_ringGlow = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!m_ringGlow) return false;

  SDL_SetTextureBlendMode(m_ringGlow, SDL_BLENDMODE_ADD);
  return true;
}

bool VfxLibrary::CreateSparkTexture(SDL_Renderer* renderer, int size) {
  std::vector<Uint32> pixels(static_cast<std::size_t>(size * size), 0);
  const float center = static_cast<float>(size) * 0.5f;

  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = static_cast<float>(x) + 0.5f - center;
      const float dy = static_cast<float>(y) + 0.5f - center;
      const float d = std::sqrt(dx * dx + dy * dy) / center;
      if (d >= 1.0f) continue;
      float a = std::pow(1.0f - d, 1.6f);
      const float cross = std::exp(-std::abs(dx) * 0.35f) + std::exp(-std::abs(dy) * 0.35f);
      a *= 0.35f + 0.65f * std::clamp(cross * 0.35f, 0.0f, 1.0f);
      const auto alpha = static_cast<Uint8>(std::min(a, 1.0f) * 255.0f);
      pixels[static_cast<std::size_t>(y * size + x)] =
          (static_cast<Uint32>(alpha) << 24) | 0x00FFFFFFu;
    }
  }

  SDL_Surface* surface =
      SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(), size, size, 32, size * 4, SDL_PIXELFORMAT_RGBA8888);
  if (!surface) return false;

  m_spark = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!m_spark) return false;

  SDL_SetTextureBlendMode(m_spark, SDL_BLENDMODE_ADD);
  return true;
}

void VfxLibrary::DrawSoftGlow(SDL_Renderer* r,
                              float x,
                              float y,
                              float radius,
                              Uint8 cr,
                              Uint8 cg,
                              Uint8 cb,
                              Uint8 alpha,
                              float rotationDeg) const {
  // Always procedural — PNG soft glow is black-hole-only (see DrawBlackHole).
  if (!m_softGlow || alpha == 0) return;

  SDL_SetTextureColorMod(m_softGlow, cr, cg, cb);
  SDL_SetTextureAlphaMod(m_softGlow, alpha);

  const int d = static_cast<int>(radius * 2.0f);
  SDL_Rect dst{static_cast<int>(x - radius), static_cast<int>(y - radius), d, d};
  SDL_RenderCopyEx(r, m_softGlow, nullptr, &dst, rotationDeg, nullptr, SDL_FLIP_NONE);
}

void VfxLibrary::DrawRingGlow(SDL_Renderer* r,
                              float x,
                              float y,
                              float radius,
                              float scale,
                              Uint8 cr,
                              Uint8 cg,
                              Uint8 cb,
                              Uint8 alpha,
                              float rotationDeg) const {
  // Always procedural — PNG ring is black-hole-only (see DrawBlackHole).
  if (!m_ringGlow || alpha == 0) return;

  SDL_SetTextureColorMod(m_ringGlow, cr, cg, cb);
  SDL_SetTextureAlphaMod(m_ringGlow, alpha);

  const float rr = radius * scale;
  const int d = static_cast<int>(rr * 2.0f);
  SDL_Rect dst{static_cast<int>(x - rr), static_cast<int>(y - rr), d, d};
  SDL_RenderCopyEx(r, m_ringGlow, nullptr, &dst, rotationDeg, nullptr, SDL_FLIP_NONE);
}

void VfxLibrary::DrawSpark(SDL_Renderer* r,
                           float x,
                           float y,
                           float size,
                           Uint8 cr,
                           Uint8 cg,
                           Uint8 cb,
                           Uint8 alpha) const {
  // Always procedural spark for particles (ember sheet has opaque frame bounds).
  if (!m_spark || alpha == 0) return;

  SDL_SetTextureColorMod(m_spark, cr, cg, cb);
  SDL_SetTextureAlphaMod(m_spark, alpha);

  const int d = static_cast<int>(size * 2.0f);
  SDL_Rect dst{static_cast<int>(x - size), static_cast<int>(y - size), d, d};
  SDL_RenderCopy(r, m_spark, nullptr, &dst);
}

void VfxLibrary::DrawRewindSpark(SDL_Renderer* r, float x, float y, float size, Uint8 alpha) const {
  if (alpha == 0 || size <= 0.0f) return;
  DrawRewindPixelSpark(r, x, y, size, 110, 220, 255, alpha);
}

void VfxLibrary::DrawRewindPixelSpark(SDL_Renderer* r,
                                      float x,
                                      float y,
                                      float size,
                                      Uint8 cr,
                                      Uint8 cg,
                                      Uint8 cb,
                                      Uint8 alpha) const {
  if (alpha == 0 || size <= 0.0f) return;

  const int half = std::max(1, static_cast<int>(size * 0.55f));
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  SDL_Rect rc{static_cast<int>(x) - half, static_cast<int>(y) - half, half * 2, half * 2};
  SDL_RenderFillRect(r, &rc);

  if (half >= 2) {
    SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(alpha * 0.35f));
    SDL_Rect core{static_cast<int>(x), static_cast<int>(y), 1, 1};
    SDL_RenderFillRect(r, &core);
  }
}

void VfxLibrary::DrawRewindGhostAura(SDL_Renderer* r,
                                     float x,
                                     float y,
                                     float radius,
                                     Uint8 alpha,
                                     float rotationDeg) const {
  DrawRingGlow(r, x, y, radius, 1.0f, 160, 220, 255, static_cast<Uint8>(alpha * 0.75f), rotationDeg);
}

void VfxLibrary::DrawSpiralArms(SDL_Renderer* r,
                                float cx,
                                float cy,
                                float innerR,
                                float outerR,
                                float baseAngle,
                                int arms,
                                int segmentsPerArm,
                                float fade,
                                Uint8 cr,
                                Uint8 cg,
                                Uint8 cb,
                                float spiralTurns,
                                float whiteHalo) const {
  if (fade <= 0.0f || arms <= 0 || segmentsPerArm <= 1) return;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  for (int arm = 0; arm < arms; ++arm) {
    const float armBase = baseAngle + static_cast<float>(arm) * (2.0f * kPi / static_cast<float>(arms));
    for (int s = 0; s < segmentsPerArm; ++s) {
      const float u0 = static_cast<float>(s) / static_cast<float>(segmentsPerArm);
      const float u1 = static_cast<float>(s + 1) / static_cast<float>(segmentsPerArm);
      const float r0 = innerR + u0 * (outerR - innerR);
      const float r1 = innerR + u1 * (outerR - innerR);
      const float a0 = armBase + u0 * spiralTurns * 2.0f * kPi;
      const float a1 = armBase + u1 * spiralTurns * 2.0f * kPi;
      const float falloff = (1.0f - u0 * 0.85f) * fade;
      const Uint8 lineA = static_cast<Uint8>(falloff * 255.0f);
      if (lineA < 4) continue;

      const int x0 = static_cast<int>(cx + std::cos(a0) * r0);
      const int y0 = static_cast<int>(cy + std::sin(a0) * r0);
      const int x1 = static_cast<int>(cx + std::cos(a1) * r1);
      const int y1 = static_cast<int>(cy + std::sin(a1) * r1);

      for (int oy = -2; oy <= 2; ++oy) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(lineA * whiteHalo));
        SDL_RenderDrawLine(r, x0, y0 + oy, x1, y1 + oy);
        SDL_SetRenderDrawColor(r, cr, cg, cb, lineA);
        SDL_RenderDrawLine(r, x0, y0 + oy, x1, y1 + oy);
      }
    }
  }
}

void VfxLibrary::DrawShockwaveRing(SDL_Renderer* r,
                                   float cx,
                                   float cy,
                                   float radius,
                                   Uint8 cr,
                                   Uint8 cg,
                                   Uint8 cb,
                                   Uint8 alpha,
                                   float thickness) const {
  if (alpha == 0 || radius <= 1.0f) return;

  // Procedural only — shockwave PNG also reads as a flat disc when scaled.
  DrawCircleOutlineThick(r, cx, cy, radius, cr, cg, cb, alpha, static_cast<int>(thickness));
  DrawRingGlow(r, cx, cy, radius, 1.05f, cr, cg, cb, static_cast<Uint8>(alpha / 2), 0.0f);
}

void VfxLibrary::DrawBlackHole(SDL_Renderer* r,
                               float cx,
                               float cy,
                               float radius,
                               float lifeT,
                               float spinAngle,
                               float darkBackdropStrength) const {
  const float fade = Envelope(lifeT, 0.18f, 0.26f);
  const float age = 1.0f - lifeT;
  if (fade <= 0.0f) return;

  const float backdrop = std::clamp(darkBackdropStrength, 0.0f, 1.0f);
  const float bodyFade = std::clamp((age - 0.06f) / 0.14f, 0.0f, 1.0f) * fade;
  const float vortexFade = bodyFade * (1.0f + backdrop * 0.40f);
  const float rotDeg = spinAngle * 57.2958f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  DrawBlackHoleDarkBackdrop(r, cx, cy, radius, backdrop, fade);

  if (IsReady()) {
    const int glowCount = backdrop > 0.01f ? 4 : 3;
    for (int i = 0; i < glowCount; ++i) {
      const float a = spinAngle * 0.08f + static_cast<float>(i) * 2.09f;
      const float dist = radius * (0.45f + 0.12f * static_cast<float>(i));
      const float glowAlpha = fade * (22.0f + backdrop * 18.0f);
      DrawSoftGlow(r,
                   cx + std::cos(a) * dist,
                   cy + std::sin(a) * dist,
                   radius * (0.22f + backdrop * 0.04f),
                   static_cast<Uint8>(55 + backdrop * 35.0f),
                   static_cast<Uint8>(28 + backdrop * 40.0f),
                   static_cast<Uint8>(85 + backdrop * 30.0f),
                   static_cast<Uint8>(glowAlpha),
                   rotDeg * 0.2f);
    }
    if (backdrop > 0.01f) {
      DrawRingGlow(r, cx, cy, radius * 1.08f, 1.05f, 110, 75, 165, static_cast<Uint8>(fade * backdrop * 42.0f),
                   rotDeg);
    }
  }

  DrawFilledDisc(r, cx, cy, radius * 0.26f, 0, 0, 0, static_cast<Uint8>(fade * 255.0f));
  DrawFilledDisc(r, cx, cy, radius * 0.16f, 0, 0, 0, static_cast<Uint8>(fade * 255.0f));

  if (age < 0.30f) {
    const float shockT = age / 0.30f;
    const float shockR = radius * (0.48f + shockT * 1.55f);
    const Uint8 shockA = static_cast<Uint8>((1.0f - shockT) * 70.0f * fade);
    DrawCircleOutlineThick(r, cx, cy, shockR, 95, 45, 140, shockA, 2);
    if (age < 0.06f) {
      const float flashT = 1.0f - age / 0.06f;
      DrawRingGlow(r, cx, cy, radius * (0.46f + flashT * 0.22f), 0.92f, 120, 55, 165,
                   static_cast<Uint8>(flashT * 38.0f * fade), rotDeg);
    }
  }

  if (bodyFade > 0.02f) {
    DrawCircularSpiralVortex(r, cx, cy, radius, spinAngle, vortexFade);
    const float ringAlpha = 24.0f + backdrop * 28.0f;
    DrawRingGlow(r, cx, cy, radius * 0.88f, 1.02f, 110, 45, 150, static_cast<Uint8>(bodyFade * ringAlpha), rotDeg);
    if (backdrop > 0.01f) {
      DrawCircleOutlineThick(r, cx, cy, radius * 0.98f, 145, 105, 195, static_cast<Uint8>(bodyFade * backdrop * 70.0f),
                             1);
    }
  }

  DrawBlackHoleAccretionSparks(r, cx, cy, radius, spinAngle, fade * (0.65f + backdrop * 0.35f));

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void VfxLibrary::DrawRewindStarfield(SDL_Renderer* r, int screenW, int screenH, float fade, float spinAngle) const {
  if (fade <= 0.0f || screenW <= 0 || screenH <= 0) return;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);
  constexpr int kStars = 780;
  for (int i = 0; i < kStars; ++i) {
    const unsigned hash = static_cast<unsigned>(i) * 2654435761u + 1597334677u;
    const int x = static_cast<int>(hash % static_cast<unsigned>(screenW));
    const int y = static_cast<int>((hash >> 10) % static_cast<unsigned>(screenH));
    const float twinkle = 0.45f + 0.55f * std::sin(spinAngle * 2.4f + static_cast<float>(i) * 0.61f);
    const Uint8 a = static_cast<Uint8>(fade * twinkle * static_cast<float>(70 + (hash % 140)));
    const bool cool = (hash & 7u) == 0u;
    if (cool) {
      SDL_SetRenderDrawColor(r, 150, 190, 255, a);
    } else {
      SDL_SetRenderDrawColor(r, 255, 245, 230, a);
    }
    const int sz = (hash & 15u) == 0u ? 2 : 1;
    SDL_Rect rc{x, y, sz, sz};
    SDL_RenderFillRect(r, &rc);
    if (sz >= 2 && (hash & 31u) == 0u) {
      SDL_SetRenderDrawColor(r, 255, 255, 255, static_cast<Uint8>(a * 0.7f));
      SDL_RenderDrawLine(r, x - 2, y, x + 2, y);
      SDL_RenderDrawLine(r, x, y - 2, x, y + 2);
    }
  }
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
}

void VfxLibrary::DrawRewindVortex(SDL_Renderer* r,
                                  float cx,
                                  float cy,
                                  float radius,
                                  float lifeT,
                                  float spinAngle) const {
  const float fade = Envelope(lifeT, 0.20f, 0.28f);
  const float age = 1.0f - lifeT;
  if (fade <= 0.0f || radius <= 1.0f) return;

  const float bodyFade = std::clamp((age - 0.04f) / 0.10f, 0.0f, 1.0f) * fade;
  const float rotDeg = spinAngle * 57.3f;

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  if (IsReady()) {
    for (int i = 0; i < 6; ++i) {
      const float a = spinAngle * 0.07f + static_cast<float>(i) * 1.047f;
      const float dist = radius * (0.28f + 0.11f * static_cast<float>(i));
      DrawSoftGlow(r,
                   cx + std::cos(a) * dist,
                   cy + std::sin(a) * dist,
                   radius * 0.20f,
                   35,
                   65,
                   155,
                   static_cast<Uint8>(fade * 42.0f),
                   rotDeg * 0.3f);
    }
  }

  DrawClockSpiralTunnel(r, cx, cy, radius * 0.98f, spinAngle, fade * 0.95f);

  DrawSpiralArms(r, cx, cy, radius * 0.06f, radius * 1.65f, -spinAngle * 0.85f, 5, 88, fade * 0.38f, 80,
                 170, 255, 5.8f, 0.18f);
  DrawSpiralArms(r, cx, cy, radius * 0.10f, radius * 1.45f, spinAngle * 1.1f, 4, 72, fade * 0.32f, 190, 90,
                 255, 4.6f, 0.12f);

  DrawFilledDisc(r, cx, cy, radius * 0.16f, 2, 4, 14, static_cast<Uint8>(fade * 240.0f));
  DrawFilledDisc(r, cx, cy, radius * 0.07f, 255, 220, 120, static_cast<Uint8>(fade * bodyFade * 220.0f));

  if (age < 0.40f) {
    const float shockT = age / 0.40f;
    const float shockR = radius * (0.50f + shockT * 2.0f);
    const Uint8 shockA = static_cast<Uint8>((1.0f - shockT) * 120.0f * fade);
    DrawShockwaveRing(r, cx, cy, shockR, 255, 200, 90, static_cast<Uint8>(shockA * 0.75f), 2.5f);
    DrawShockwaveRing(r, cx, cy, shockR * 0.96f, 90, 180, 255, shockA, 3.0f);
    if (age < 0.10f) {
      const float flashT = 1.0f - age / 0.10f;
      DrawRingGlow(r, cx, cy, radius * (0.42f + flashT * 0.28f), 1.0f, 255, 220, 120,
                   static_cast<Uint8>(flashT * 85.0f * fade), 0.0f);
      DrawRingGlow(r, cx, cy, radius * (0.36f + flashT * 0.22f), 0.95f, 120, 200, 255,
                   static_cast<Uint8>(flashT * 65.0f * fade), rotDeg);
    }
  }

  if (bodyFade > 0.02f && IsReady()) {
    DrawRingGlow(r, cx, cy, radius * 0.55f, 1.05f, 255, 210, 90, static_cast<Uint8>(bodyFade * 70.0f), rotDeg);
    DrawRingGlow(r, cx, cy, radius * 0.38f, 0.82f, 110, 175, 255, static_cast<Uint8>(bodyFade * 55.0f),
                 -rotDeg * 0.6f);
  }

  DrawOrbitPixelSparks(r, cx, cy, radius, spinAngle, fade);

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

void VfxLibrary::DrawVignette(SDL_Renderer* r, int w, int h, Uint8 alpha, Uint8 cr, Uint8 cg, Uint8 cb) const {
  if (alpha == 0) return;

  SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
  constexpr int band = 96;
  SDL_Rect top{0, 0, w, band};
  SDL_Rect bottom{0, h - band, w, band};
  SDL_Rect left{0, 0, band, h};
  SDL_Rect right{w - band, 0, band, h};
  SDL_RenderFillRect(r, &top);
  SDL_RenderFillRect(r, &bottom);
  SDL_RenderFillRect(r, &left);
  SDL_RenderFillRect(r, &right);
}

} // namespace cr
