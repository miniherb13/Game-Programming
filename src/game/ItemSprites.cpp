#include "game/ItemSprites.h"

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
  paths.emplace_back(std::string("assets/items/") + filename);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + "assets/items/" + filename);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../assets/items/") + filename);
  paths.emplace_back(std::string("../../assets/items/") + filename);
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

} // namespace

ItemSprites::~ItemSprites() {
  const auto destroy = [](IconTex& icon) {
    if (icon.texture) {
      SDL_DestroyTexture(static_cast<SDL_Texture*>(icon.texture));
      icon.texture = nullptr;
    }
  };
  destroy(m_health);
  destroy(m_stamina);
  destroy(m_shield);
}

ItemSprites::IconTex* ItemSprites::IconFor(ItemSpriteId id) {
  switch (id) {
  case ItemSpriteId::Health: return &m_health;
  case ItemSpriteId::Stamina: return &m_stamina;
  case ItemSpriteId::Shield: return &m_shield;
  }
  return nullptr;
}

const ItemSprites::IconTex* ItemSprites::IconFor(ItemSpriteId id) const {
  return const_cast<ItemSprites*>(this)->IconFor(id);
}

bool ItemSprites::LoadIconFile(const char* filename,
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
    const PixelBounds bounds = ComputeContentBounds(rgba, w, h);
    outBounds.x = bounds.x;
    outBounds.y = bounds.y;
    outBounds.w = bounds.w;
    outBounds.h = bounds.h;
    Log(LogLevel::Info,
        "Item icon loaded: " + path + " (" + std::to_string(w) + "x" + std::to_string(h) + ", content " +
            std::to_string(outBounds.w) + "x" + std::to_string(outBounds.h) + ")");
    return true;
  }
  return false;
}

bool ItemSprites::Load() {
  m_hasHealth = LoadIconFile("health.png", m_healthPixels, m_healthW, m_healthH, m_healthCrop);
  m_hasStamina = LoadIconFile("stamina.png", m_staminaPixels, m_staminaW, m_staminaH, m_staminaCrop);
  m_hasShield = LoadIconFile("shield.png", m_shieldPixels, m_shieldW, m_shieldH, m_shieldCrop);

  if (!m_hasHealth) Log(LogLevel::Warn, "assets/items/health.png missing");
  if (!m_hasStamina) Log(LogLevel::Warn, "assets/items/stamina.png missing");
  if (!m_hasShield) Log(LogLevel::Warn, "assets/items/shield.png missing");

  return m_hasHealth || m_hasStamina || m_hasShield;
}

bool ItemSprites::UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, IconTex& out) const {
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

void ItemSprites::EnsureUploaded(SDL_Renderer* renderer) const {
  if (m_uploadAttempted || !renderer) return;
  m_uploadAttempted = true;

  const auto uploadOne = [&](bool hasPixels,
                             const std::vector<unsigned char>& pixels,
                             int pw,
                             int ph,
                             IconTex& tex,
                             const CropRect& crop,
                             const char* label) {
    if (!hasPixels || pixels.empty()) return true;
    if (!UploadRgba(renderer, pixels.data(), pw, ph, tex)) {
      Log(LogLevel::Error, std::string("Item icon GPU upload failed: ") + label);
      return false;
    }
    tex.crop = crop;
    return true;
  };

  if (!uploadOne(m_hasHealth, m_healthPixels, m_healthW, m_healthH, m_health, m_healthCrop, "health")) return;
  if (!uploadOne(m_hasStamina, m_staminaPixels, m_staminaW, m_staminaH, m_stamina, m_staminaCrop, "stamina")) return;
  if (!uploadOne(m_hasShield, m_shieldPixels, m_shieldW, m_shieldH, m_shield, m_shieldCrop, "shield")) return;

  m_gpuReady = m_health.texture || m_stamina.texture || m_shield.texture;
  if (m_gpuReady) Log(LogLevel::Info, "Item sprite GPU icons ready");
}

void ItemSprites::DrawCentered(SDL_Renderer* renderer,
                               const IconTex& icon,
                               float screenX,
                               float screenY,
                               int targetSizePx) {
  if (!icon.texture || icon.w <= 0 || icon.h <= 0) return;

  const CropRect& c = icon.crop;
  if (c.w <= 0 || c.h <= 0) return;

  const int destH = std::max(1, targetSizePx);
  const int destW = std::max(1, static_cast<int>(std::lround(static_cast<float>(c.w) * static_cast<float>(destH) /
                                                                 static_cast<float>(c.h))));

  SDL_Rect src{c.x, c.y, c.w, c.h};
  SDL_Rect dst{};
  dst.w = destW;
  dst.h = destH;
  dst.x = static_cast<int>(screenX - static_cast<float>(destW) * 0.5f);
  dst.y = static_cast<int>(screenY - static_cast<float>(destH) * 0.5f);

  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(icon.texture), &src, &dst);
}

void ItemSprites::Draw(SDL_Renderer* renderer,
                       ItemSpriteId id,
                       float screenX,
                       float screenY,
                       float displaySize) const {
  if (!m_gpuReady) return;

  const IconTex* icon = IconFor(id);
  if (!icon || !icon->texture) return;

  const int sizePx = std::max(1, static_cast<int>(displaySize));
  DrawCentered(renderer, *icon, screenX, screenY, sizePx);
}

} // namespace cr
