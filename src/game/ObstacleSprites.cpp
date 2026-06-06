#include "game/ObstacleSprites.h"

#include "core/Log.h"
#include "render/SpriteOutline.h"

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

std::vector<std::string> CandidatePaths(const char* folder, const char* filename) {
  std::vector<std::string> paths;
  paths.emplace_back(std::string(folder) + filename);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + folder + filename);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../") + folder + filename);
  paths.emplace_back(std::string("../../") + folder + filename);
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

const char* StageFolder(ObstacleStage stage) {
  switch (stage) {
  case ObstacleStage::Mars: return "assets/stages/mars/obstacles/";
  case ObstacleStage::Glacier: return "assets/stages/glacier/obstacles/";
  case ObstacleStage::Emerald: return "assets/stages/emerald/obstacles/";
  }
  return "assets/stages/mars/obstacles/";
}

const char* StageLabel(ObstacleStage stage) {
  switch (stage) {
  case ObstacleStage::Mars: return "Mars";
  case ObstacleStage::Glacier: return "Glacier";
  case ObstacleStage::Emerald: return "Emerald";
  }
  return "Mars";
}

} // namespace

ObstacleSprites::SpriteEntry* ObstacleSprites::SpriteBank::Entry(ObstacleSpriteId id) {
  switch (id) {
  case ObstacleSpriteId::Normal: return &normal;
  case ObstacleSpriteId::Tall: return &tall;
  case ObstacleSpriteId::Bounce: return &bounce;
  case ObstacleSpriteId::Spike: return &spike;
  case ObstacleSpriteId::Triangle: return &triangle;
  case ObstacleSpriteId::Ceiling: return &ceiling;
  case ObstacleSpriteId::Moving: return &moving;
  case ObstacleSpriteId::Falling: return &falling;
  }
  return nullptr;
}

const ObstacleSprites::SpriteEntry* ObstacleSprites::SpriteBank::Entry(ObstacleSpriteId id) const {
  return const_cast<SpriteBank*>(this)->Entry(id);
}

void ObstacleSprites::SpriteBank::DestroyTextures() {
  const auto destroy = [](SpriteEntry& entry) {
    if (entry.tex.texture) {
      SDL_DestroyTexture(static_cast<SDL_Texture*>(entry.tex.texture));
      entry.tex.texture = nullptr;
    }
  };
  destroy(normal);
  destroy(tall);
  destroy(bounce);
  destroy(spike);
  destroy(triangle);
  destroy(ceiling);
  destroy(moving);
  destroy(falling);
}

ObstacleSprites::~ObstacleSprites() {
  m_mars.DestroyTextures();
  m_glacier.DestroyTextures();
  m_emerald.DestroyTextures();
}

ObstacleSprites::SpriteBank* ObstacleSprites::Bank(ObstacleStage stage) {
  switch (stage) {
  case ObstacleStage::Mars: return &m_mars;
  case ObstacleStage::Glacier: return &m_glacier;
  case ObstacleStage::Emerald: return &m_emerald;
  }
  return &m_mars;
}

const ObstacleSprites::SpriteBank* ObstacleSprites::Bank(ObstacleStage stage) const {
  return const_cast<ObstacleSprites*>(this)->Bank(stage);
}

bool ObstacleSprites::LoadFile(const char* folder,
                               const char* filename,
                               std::vector<unsigned char>& rgba,
                               int& w,
                               int& h,
                               CropRect& outBounds) const {
  for (const auto& path : CandidatePaths(folder, filename)) {
    int comp = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!pixels) continue;

    rgba.assign(pixels, pixels + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(pixels);
    RemoveBackground(rgba, w, h);
    AddBoldWhiteOutline(rgba, w, h);
    const PixelBounds bounds = ComputeContentBounds(rgba, w, h);
    outBounds.x = bounds.x;
    outBounds.y = bounds.y;
    outBounds.w = bounds.w;
    outBounds.h = bounds.h;
    Log(LogLevel::Info,
        std::string("Obstacle sprite loaded: ") + folder + filename + " (" + std::to_string(w) + "x" +
            std::to_string(h) + ", content " + std::to_string(outBounds.w) + "x" + std::to_string(outBounds.h) + ")");
    return true;
  }
  return false;
}

bool ObstacleSprites::LoadBank(SpriteBank& bank, const char* folder) const {
  const auto loadOne = [&](SpriteEntry& entry) {
    entry.loaded = LoadFile(folder, entry.filename, entry.pixels, entry.w, entry.h, entry.crop);
    if (!entry.loaded) {
      Log(LogLevel::Warn, std::string(folder) + entry.filename + " missing");
    }
    return entry.loaded;
  };

  int count = 0;
  if (loadOne(bank.normal)) ++count;
  if (loadOne(bank.tall)) ++count;
  if (loadOne(bank.bounce)) ++count;
  if (loadOne(bank.spike)) ++count;
  if (loadOne(bank.triangle)) ++count;
  if (loadOne(bank.ceiling)) ++count;
  if (loadOne(bank.moving)) ++count;
  if (loadOne(bank.falling)) ++count;

  return count > 0;
}

bool ObstacleSprites::Load() {
  const bool mars = LoadBank(m_mars, StageFolder(ObstacleStage::Mars));
  const bool glacier = LoadBank(m_glacier, StageFolder(ObstacleStage::Glacier));
  const bool emerald = LoadBank(m_emerald, StageFolder(ObstacleStage::Emerald));
  return mars || glacier || emerald;
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

void ObstacleSprites::EnsureBankUploaded(SDL_Renderer* renderer, SpriteBank& bank, ObstacleStage stage) const {
  if (bank.uploadAttempted || !renderer) return;
  bank.uploadAttempted = true;

  const auto uploadOne = [&](SpriteEntry& entry) {
    if (!entry.loaded || entry.pixels.empty()) return true;
    if (!UploadRgba(renderer, entry.pixels.data(), entry.w, entry.h, entry.tex)) {
      Log(LogLevel::Error, std::string("Obstacle sprite GPU upload failed: ") + entry.filename);
      return false;
    }
    entry.tex.crop = entry.crop;
    return true;
  };

  if (!uploadOne(bank.normal)) return;
  if (!uploadOne(bank.tall)) return;
  if (!uploadOne(bank.bounce)) return;
  if (!uploadOne(bank.spike)) return;
  if (!uploadOne(bank.triangle)) return;
  if (!uploadOne(bank.ceiling)) return;
  if (!uploadOne(bank.moving)) return;
  if (!uploadOne(bank.falling)) return;

  bank.gpuReady = bank.normal.tex.texture || bank.tall.tex.texture || bank.bounce.tex.texture ||
                  bank.triangle.tex.texture || bank.ceiling.tex.texture || bank.moving.tex.texture ||
                  bank.falling.tex.texture || bank.spike.tex.texture;
  if (bank.gpuReady) {
    Log(LogLevel::Info, std::string(StageLabel(stage)) + " obstacle sprites ready");
  }
}

void ObstacleSprites::EnsureUploaded(SDL_Renderer* renderer, ObstacleStage stage) const {
  EnsureBankUploaded(renderer, *const_cast<ObstacleSprites*>(this)->Bank(stage), stage);
}

bool ObstacleSprites::IsReady(ObstacleStage stage) const {
  const SpriteBank* bank = Bank(stage);
  return bank && bank->gpuReady;
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

void ObstacleSprites::BlitScaledCenteredRotated(SDL_Renderer* renderer,
                                                const SpriteTex& sprite,
                                                float screenX,
                                                float screenY,
                                                float displaySize,
                                                float rotationDeg) {
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

  SDL_RenderCopyEx(renderer,
                   static_cast<SDL_Texture*>(sprite.texture),
                   &src,
                   &dst,
                   rotationDeg,
                   nullptr,
                   SDL_FLIP_NONE);
}

void ObstacleSprites::DrawGrounded(SDL_Renderer* renderer,
                                   ObstacleStage stage,
                                   ObstacleSpriteId id,
                                   float screenX,
                                   float footY,
                                   float displayHeight) const {
  const SpriteBank* bank = Bank(stage);
  if (!bank || !bank->gpuReady) return;
  const SpriteEntry* entry = bank->Entry(id);
  if (!entry || !entry->tex.texture) return;
  BlitScaled(renderer, entry->tex, screenX, footY, displayHeight, true);
}

void ObstacleSprites::DrawFromTop(SDL_Renderer* renderer,
                                  ObstacleStage stage,
                                  ObstacleSpriteId id,
                                  float screenX,
                                  float topY,
                                  float displayHeight) const {
  const SpriteBank* bank = Bank(stage);
  if (!bank || !bank->gpuReady) return;
  const SpriteEntry* entry = bank->Entry(id);
  if (!entry || !entry->tex.texture) return;
  BlitScaled(renderer, entry->tex, screenX, topY, displayHeight, false);
}

void ObstacleSprites::DrawCentered(SDL_Renderer* renderer,
                                   ObstacleStage stage,
                                   ObstacleSpriteId id,
                                   float screenX,
                                   float screenY,
                                   float displaySize) const {
  const SpriteBank* bank = Bank(stage);
  if (!bank || !bank->gpuReady) return;
  const SpriteEntry* entry = bank->Entry(id);
  if (!entry || !entry->tex.texture) return;
  BlitScaledCentered(renderer, entry->tex, screenX, screenY, displaySize);
}

void ObstacleSprites::DrawCenteredRotated(SDL_Renderer* renderer,
                                          ObstacleStage stage,
                                          ObstacleSpriteId id,
                                          float screenX,
                                          float screenY,
                                          float displaySize,
                                          float rotationDeg) const {
  const SpriteBank* bank = Bank(stage);
  if (!bank || !bank->gpuReady) return;
  const SpriteEntry* entry = bank->Entry(id);
  if (!entry || !entry->tex.texture) return;
  BlitScaledCenteredRotated(renderer, entry->tex, screenX, screenY, displaySize, rotationDeg);
}

} // namespace cr
