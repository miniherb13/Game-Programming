#include "game/PlayerSprite.h"

#include "core/Log.h"

#define STB_IMAGE_IMPLEMENTATION
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

constexpr float kDisplayHeight = 82.0f;
constexpr float kRunFrameSeconds = 0.14f;
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
  paths.emplace_back(std::string("assets/player/") + filename);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + "assets/player/" + filename);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../assets/player/") + filename);
  paths.emplace_back(std::string("../../assets/player/") + filename);
  return paths;
}

bool IsBackgroundColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  if (a <= kAlphaVisible) return true;

  if (r >= kBgKeyLight && g >= kBgKeyLight && b >= kBgKeyLight) return true;

  const int maxC = std::max({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int minC = std::min({static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)});
  const int sat = maxC - minC;

  // Photopea checkerboard / gray export (not character cream tones)
  if (sat <= 28) {
    const int avg = (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
    if (avg >= 88 && avg <= 252) return true;
  }

  if (r <= kBgKeyDark && g <= kBgKeyDark && b <= kBgKeyDark) return true;

  return false;
}

// Flood from image edges so black outlines on the character are preserved.
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

} // namespace

PlayerSprite::~PlayerSprite() {
  const auto destroy = [](FrameTex& f) {
    if (f.texture) {
      SDL_DestroyTexture(static_cast<SDL_Texture*>(f.texture));
      f.texture = nullptr;
    }
  };
  destroy(m_idle);
  destroy(m_jump);
  for (auto& f : m_run) destroy(f);
  for (auto& f : m_throw) destroy(f);
}

bool PlayerSprite::LoadFrameFile(const char* filename,
                                std::vector<unsigned char>& rgba,
                                int& w,
                                int& h,
                                PlayerSprite::CropRect& outBounds) const {
  for (const auto& path : CandidatePaths(filename)) {
    int comp = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels) continue;

    rgba.assign(pixels, pixels + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(pixels);
    RemoveBackground(rgba, w, h);
    const PixelBounds bounds = ComputeContentBounds(rgba, w, h);
    outBounds.x = bounds.x;
    outBounds.y = bounds.y;
    outBounds.w = bounds.w;
    outBounds.h = bounds.h;
    Log(LogLevel::Info,
        "Player frame loaded: " + path + " (" + std::to_string(w) + "x" + std::to_string(h) + ", content " +
            std::to_string(outBounds.w) + "x" + std::to_string(outBounds.h) + ")");
    return true;
  }
  return false;
}

bool PlayerSprite::Load() {
  m_hasIdle = LoadFrameFile("idle.png", m_idlePixels, m_idleW, m_idleH, m_idleCrop);
  if (!m_hasIdle) {
    Log(LogLevel::Warn, "Player idle.png missing; pause uses run_0");
  }

  static const char* kRunNames[] = {"run_0.png", "run_1.png", "run_2.png", "run_3.png"};
  m_runFrameCount = 0;
  for (int i = 0; i < 4; i++) {
    if (LoadFrameFile(kRunNames[i], m_runPixels[i], m_runW[i], m_runH[i], m_runCrop[i])) {
      m_runFrameCount++;
    } else if (i == 0) {
      Log(LogLevel::Error, "Player run_0.png not found in assets/player/");
      return false;
    } else {
      break;
    }
  }
  if (m_runFrameCount < 1) return false;

  static const char* kThrowNames[] = {"throw_0.png", "throw_1.png", "throw_2.png"};
  for (int i = 0; i < 3; i++) {
    if (!LoadFrameFile(kThrowNames[i], m_throwPixels[i], m_throwW[i], m_throwH[i], m_throwCrop[i])) {
      Log(LogLevel::Error, std::string("Player ") + kThrowNames[i] + " not found in assets/player/");
      return false;
    }
  }

  if (!LoadFrameFile("jump.png", m_jumpPixels, m_jumpW, m_jumpH, m_jumpCrop)) {
    Log(LogLevel::Error, "Player jump.png not found in assets/player/");
    return false;
  }

  return true;
}

bool PlayerSprite::UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, FrameTex& out) const {
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

void PlayerSprite::EnsureUploaded(SDL_Renderer* renderer) const {
  if (m_uploadAttempted || !renderer) return;
  m_uploadAttempted = true;

  if (m_jumpPixels.empty()) return;

  if (m_hasIdle && !m_idlePixels.empty()) {
    if (!UploadRgba(renderer, m_idlePixels.data(), m_idleW, m_idleH, m_idle)) {
      Log(LogLevel::Error, "Player idle GPU upload failed");
      return;
    }
    m_idle.crop = m_idleCrop;
  }

  for (int i = 0; i < m_runFrameCount; i++) {
    if (m_runPixels[i].empty()) return;
    if (!UploadRgba(renderer, m_runPixels[i].data(), m_runW[i], m_runH[i], m_run[i])) {
      Log(LogLevel::Error, "Player run GPU upload failed: " + std::to_string(i));
      return;
    }
    m_run[i].crop = m_runCrop[i];
  }

  for (int i = 0; i < 3; i++) {
    if (m_throwPixels[i].empty()) return;
    if (!UploadRgba(renderer, m_throwPixels[i].data(), m_throwW[i], m_throwH[i], m_throw[i])) {
      Log(LogLevel::Error, "Player throw GPU upload failed: " + std::to_string(i));
      return;
    }
    m_throw[i].crop = m_throwCrop[i];
  }

  if (!UploadRgba(renderer, m_jumpPixels.data(), m_jumpW, m_jumpH, m_jump)) {
    Log(LogLevel::Error, "Player jump GPU upload failed");
    return;
  }
  m_jump.crop = m_jumpCrop;

  m_gpuReady = true;
  Log(LogLevel::Info, "Player sprite GPU frames ready");
}

void PlayerSprite::DrawFrame(SDL_Renderer* renderer,
                             const FrameTex& frame,
                             float screenX,
                             float footY,
                             int targetContentHeightPx) {
  if (!frame.texture || frame.w <= 0 || frame.h <= 0) return;

  const CropRect& c = frame.crop;
  if (c.w <= 0 || c.h <= 0) return;

  const int destH = std::max(1, targetContentHeightPx);
  const int destW = std::max(1, static_cast<int>(std::lround(static_cast<float>(c.w) * static_cast<float>(destH) /
                                                                 static_cast<float>(c.h))));

  SDL_Rect src{c.x, c.y, c.w, c.h};
  SDL_Rect dst{};
  dst.w = destW;
  dst.h = destH;
  dst.x = static_cast<int>(screenX - static_cast<float>(destW) * 0.5f);
  dst.y = static_cast<int>(footY - static_cast<float>(destH));

  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(frame.texture), &src, &dst);
}

void PlayerSprite::DrawGhost(SDL_Renderer* renderer, float screenX, float footY, Uint8 alpha) const {
  if (!m_gpuReady || alpha == 0) return;

  const int targetContentH = std::max(1, static_cast<int>(kDisplayHeight));
  const FrameTex& frame = m_jump;
  auto* tex = static_cast<SDL_Texture*>(frame.texture);
  SDL_SetTextureAlphaMod(tex, alpha);
  SDL_SetTextureColorMod(tex, 40, 45, 55);
  DrawFrame(renderer, frame, screenX, footY, targetContentH);
  SDL_SetTextureAlphaMod(tex, 255);
  SDL_SetTextureColorMod(tex, 255, 255, 255);
}

void PlayerSprite::Draw(SDL_Renderer* renderer,
                        float screenX,
                        float footY,
                        bool onGround,
                        bool paused,
                        bool rewinding,
                        bool throwing,
                        bool throwRelease,
                        float throwCharge01,
                        float runAnimPhase) const {
  if (!m_gpuReady) return;

  const int targetContentH = std::max(1, static_cast<int>(kDisplayHeight));

  const FrameTex* frame = &m_run[0];

  if (paused) {
    frame = m_hasIdle ? &m_idle : &m_run[0];
  } else if (rewinding) {
    frame = &m_jump;
  } else if (!onGround) {
    frame = &m_jump;
  } else if (throwRelease) {
    frame = &m_throw[2];
  } else if (throwing) {
    const float t = std::clamp(throwCharge01, 0.0f, 1.0f);
    const int idx = std::min(1, static_cast<int>(t * 2.0f));
    frame = &m_throw[idx];
  } else {
    const int idx =
        static_cast<int>(std::floor(runAnimPhase / kRunFrameSeconds)) % std::max(1, m_runFrameCount);
    frame = &m_run[idx];
  }

  DrawFrame(renderer, *frame, screenX, footY, targetContentH);
}

} // namespace cr
