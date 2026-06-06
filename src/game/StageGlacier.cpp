#include "game/StageGlacier.h"

#include "core/Log.h"
#include "render/SpriteOutline.h"

#include "stb_image.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <queue>
#include <string>
#include <utility>

namespace cr {

namespace {

constexpr int kTileSheetCols = 8;
constexpr int kTileSheetRows = 4;

// glacier_tiles.png 8x4 (row 0 = ground, row 1 = platforms)
constexpr int kTileGroundTop = 2;
constexpr int kTileGroundFill = 4;
constexpr int kTilePlatformSmall = 0;
constexpr int kTilePlatformWideL = 1;
constexpr int kTilePlatformWideC = 0;
constexpr int kTilePlatformWideR = 2;
constexpr int kPlatformRow = 1;

constexpr int kDecorCols = 8;
constexpr int kDecorRows = 2;

constexpr int kHazardCols = 8;
constexpr int kHazardRows = 3;

constexpr int kPickupCols = 8;
constexpr int kPickupRows = 2;

constexpr int kDisplayTilePx = 32;
constexpr int kPickupDisplayPx = 28;
constexpr int kDecorDisplayPx = 26;

std::vector<std::string> CandidatePaths(const char* relative) {
  std::vector<std::string> paths;
  paths.emplace_back(std::string("assets/stages/glacier/") + relative);

  if (char* base = SDL_GetBasePath()) {
    paths.emplace_back(std::string(base) + "assets/stages/glacier/" + relative);
    SDL_free(base);
  }

  paths.emplace_back(std::string("../assets/stages/glacier/") + relative);
  paths.emplace_back(std::string("../../assets/stages/glacier/") + relative);
  return paths;
}

constexpr int kBgKeyLight = 238;
constexpr int kBgKeyDark = 17;
constexpr int kAlphaVisible = 12;

bool IsBackgroundColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  if (a <= kAlphaVisible) return true;
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

void RemoveEdgeBackground(std::vector<unsigned char>& rgba, int w, int h) {
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

bool LoadRgbaFile(const char* filename, std::vector<unsigned char>& rgba, int& w, int& h) {
  for (const std::string& path : CandidatePaths(filename)) {
    int comp = 0;
    unsigned char* img = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!img) continue;

    rgba.assign(img, img + static_cast<std::size_t>(w * h * 4));
    stbi_image_free(img);
    Log(LogLevel::Info,
        "Glacier stage asset loaded: " + path + " (" + std::to_string(w) + "x" + std::to_string(h) + ")");
    return true;
  }
  return false;
}

SDL_Texture* CreateTexture(SDL_Renderer* renderer, std::vector<unsigned char>& rgba, int w, int h) {
  if (rgba.empty() || w <= 0 || h <= 0) return nullptr;

  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(rgba.data(),
                                                            w,
                                                            h,
                                                            32,
                                                            w * 4,
                                                            SDL_PIXELFORMAT_RGBA32);
  if (!surface) return nullptr;

  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  return tex;
}

void DestroyTexture(void*& tex) {
  if (tex) {
    SDL_DestroyTexture(static_cast<SDL_Texture*>(tex));
    tex = nullptr;
  }
}

} // namespace

StageGlacier::~StageGlacier() {
  DestroyTexture(m_tiles.texture);
  DestroyTexture(m_decor.texture);
  DestroyTexture(m_hazards.texture);
  DestroyTexture(m_pickups.texture);
  DestroyTexture(m_parallaxFar.texture);
  DestroyTexture(m_parallaxMid.texture);
  DestroyTexture(m_parallaxNear.texture);
}

bool StageGlacier::LoadSheetFile(const char* filename, SheetAsset& out, int cols, int rows, bool stripBackground) {
  if (!LoadRgbaFile(filename, out.rgba, out.w, out.h)) return false;
  if (stripBackground) RemoveEdgeBackground(out.rgba, out.w, out.h);
  out.cols = cols;
  out.rows = rows;
  out.cellW = std::max(1, out.w / cols);
  out.cellH = std::max(1, out.h / rows);
  return true;
}

bool StageGlacier::LoadParallaxFile(const char* filename, ParallaxAsset& out, bool stripBackground) {
  if (!LoadRgbaFile(filename, out.rgba, out.w, out.h)) return false;
  if (stripBackground) RemoveEdgeBackground(out.rgba, out.w, out.h);
  return true;
}

bool StageGlacier::Load() {
  m_cols = kTileSheetCols;
  m_rows = kTileSheetRows;
  m_displayTilePx = kDisplayTilePx;

  if (!LoadSheetFile("glacier_tiles.png", m_tiles, kTileSheetCols, kTileSheetRows, true)) {
    Log(LogLevel::Error, "Failed to load assets/stages/glacier/glacier_tiles.png");
    return false;
  }

  m_tileW = m_tiles.cellW;
  m_tileH = m_tiles.cellH;

  if (!LoadParallaxFile("glacier_parallax_far.png", m_parallaxFar, false)) {
    Log(LogLevel::Warn, "glacier_parallax_far.png not found");
  }
  if (!LoadParallaxFile("glacier_parallax_mid.png", m_parallaxMid, true)) {
    Log(LogLevel::Warn, "glacier_parallax_mid.png not found");
  }
  if (!LoadParallaxFile("glacier_parallax_near.png", m_parallaxNear, true)) {
    Log(LogLevel::Warn, "glacier_parallax_near.png not found");
  }

  if (!LoadSheetFile("glacier_decor.png", m_decor, kDecorCols, kDecorRows, true)) {
    Log(LogLevel::Warn, "glacier_decor.png not found");
  }
  if (!LoadSheetFile("glacier_hazards.png", m_hazards, kHazardCols, kHazardRows, true)) {
    Log(LogLevel::Warn, "glacier_hazards.png not found");
  }
  if (!LoadSheetFile("glacier_pickups.png", m_pickups, kPickupCols, kPickupRows, true)) {
    Log(LogLevel::Warn, "glacier_pickups.png not found");
  } else {
    AddBoldWhiteOutline(m_pickups.rgba, m_pickups.w, m_pickups.h);
  }

  m_loaded = true;
  return true;
}

void StageGlacier::UploadSheet(SDL_Renderer* renderer, SheetAsset& sheet) const {
  if (sheet.texture || sheet.rgba.empty()) return;
  std::vector<unsigned char> scratch = sheet.rgba;
  sheet.texture = CreateTexture(renderer, scratch, sheet.w, sheet.h);
  sheet.rgba.clear();
}

void StageGlacier::UploadParallax(SDL_Renderer* renderer, ParallaxAsset& layer) const {
  if (layer.texture || layer.rgba.empty()) return;
  std::vector<unsigned char> scratch = layer.rgba;
  layer.texture = CreateTexture(renderer, scratch, layer.w, layer.h);
  layer.rgba.clear();
}

void StageGlacier::EnsureUploaded(SDL_Renderer* renderer) const {
  if (!m_loaded || m_uploaded || !renderer) return;

  UploadSheet(renderer, m_tiles);
  UploadSheet(renderer, m_decor);
  UploadSheet(renderer, m_hazards);
  UploadSheet(renderer, m_pickups);
  UploadParallax(renderer, m_parallaxFar);
  UploadParallax(renderer, m_parallaxMid);
  UploadParallax(renderer, m_parallaxNear);

  m_uploaded = m_tiles.texture != nullptr;
}

void StageGlacier::DrawScrollingLayer(SDL_Renderer* renderer,
                                  const ParallaxAsset& layer,
                                  float cameraX,
                                  float scrollFactor,
                                  int screenW,
                                  int dstY,
                                  int dstH) const {
  if (!layer.texture || layer.w <= 0 || layer.h <= 0 || dstH <= 0) return;

  auto* tex = static_cast<SDL_Texture*>(layer.texture);
  const float parallaxX = cameraX * scrollFactor;
  const int drawW = static_cast<int>(static_cast<float>(layer.w) * (static_cast<float>(dstH) / layer.h));
  if (drawW <= 0) return;

  const int startTile = static_cast<int>(std::floor(parallaxX / static_cast<float>(drawW)));
  const int endTile = startTile + screenW / drawW + 2;

  for (int i = startTile; i <= endTile; i++) {
    const int x = static_cast<int>(static_cast<float>(i) * drawW - parallaxX);
    SDL_Rect dst{x, dstY, drawW, dstH};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
  }
}

void StageGlacier::DrawParallaxBackground(SDL_Renderer* renderer,
                                      int screenW,
                                      int screenH,
                                      float cameraX,
                                      float groundY) const {
  DrawScrollingLayer(renderer, m_parallaxFar, cameraX, 0.08f, screenW, 0, screenH);

  const int midH = static_cast<int>(static_cast<float>(screenH) * 0.38f);
  const int midY = static_cast<int>(groundY) - midH + 12;
  DrawScrollingLayer(renderer, m_parallaxMid, cameraX, 0.22f, screenW, midY, midH);
}

void StageGlacier::DrawParallaxNear(SDL_Renderer* renderer,
                                int screenW,
                                int screenH,
                                float cameraX,
                                float groundY) const {
  (void)screenH;
  const int nearH = static_cast<int>(static_cast<float>(screenH) * 0.20f);
  const int nearY = static_cast<int>(groundY) - nearH;
  DrawScrollingLayer(renderer, m_parallaxNear, cameraX, 0.42f, screenW, nearY, nearH);
}

void StageGlacier::DrawTile(SDL_Renderer* renderer, int col, int row, int dstX, int dstY, int dstW, int dstH) const {
  if (!m_tiles.texture || col < 0 || col >= m_cols || row < 0 || row >= m_rows) return;

  SDL_Rect src{col * m_tileW, row * m_tileH, m_tileW, m_tileH};
  SDL_Rect dst{dstX, dstY, dstW, dstH};
  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(m_tiles.texture), &src, &dst);
}

void StageGlacier::DrawSheetCell(SDL_Renderer* renderer,
                             const SheetAsset& sheet,
                             int col,
                             int row,
                             int dstX,
                             int dstY,
                             int dstW,
                             int dstH) const {
  if (!sheet.texture || col < 0 || col >= sheet.cols || row < 0 || row >= sheet.rows) return;

  SDL_Rect src{col * sheet.cellW, row * sheet.cellH, sheet.cellW, sheet.cellH};
  SDL_Rect dst{dstX, dstY, dstW > 0 ? dstW : sheet.cellW, dstH > 0 ? dstH : sheet.cellH};
  SDL_RenderCopy(renderer, static_cast<SDL_Texture*>(sheet.texture), &src, &dst);
}

void StageGlacier::DrawTerrain(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const {
  if (!m_tiles.texture) return;

  const int px = m_displayTilePx;
  const int topY = static_cast<int>(groundY) - px;
  const int fillH = std::max(px, screenH - topY);
  const int startCol = static_cast<int>(std::floor(cameraX / static_cast<float>(px)));
  const int endCol = startCol + screenW / px + 3;

  const int topCol = kTileGroundTop % m_cols;
  const int topRow = kTileGroundTop / m_cols;
  const int fillCol = kTileGroundFill % m_cols;
  const int fillRow = kTileGroundFill / m_cols;

  for (int col = startCol; col <= endCol; col++) {
    const int worldX = col * px;
    const int screenX = static_cast<int>(worldX - cameraX);

    DrawTile(renderer, topCol, topRow, screenX, topY, px, px);

    for (int fy = topY + px; fy < topY + fillH; fy += px) {
      DrawTile(renderer, fillCol, fillRow, screenX, fy, px, px);
    }
  }

  struct PlatformSpec {
    float worldX;
    int tilesWide;
    bool useWide;
  };

  const PlatformSpec platforms[] = {
      {384.0f, 1, false},  {704.0f, 3, true},   {1088.0f, 1, false}, {1408.0f, 3, true},
      {1824.0f, 1, false}, {2176.0f, 3, true},  {2592.0f, 1, false}, {2944.0f, 3, true},
      {3360.0f, 1, false},
  };

  const int platY = topY - px - 40;

  for (const PlatformSpec& p : platforms) {
    const int baseX = static_cast<int>(p.worldX + kWorldOffsetX - cameraX);
    if (baseX < -px * 5 || baseX > screenW + px * 5) continue;

    if (!p.useWide || p.tilesWide <= 1) {
      DrawTile(renderer, kTilePlatformSmall, kPlatformRow, baseX, platY, px, px);
      continue;
    }

    DrawTile(renderer, kTilePlatformWideL, kPlatformRow, baseX, platY, px, px);
    for (int i = 1; i < p.tilesWide - 1; i++) {
      DrawTile(renderer, kTilePlatformWideC, kPlatformRow, baseX + px * i, platY, px, px);
    }
    DrawTile(renderer, kTilePlatformWideR, kPlatformRow, baseX + px * (p.tilesWide - 1), platY, px, px);
  }
}

void StageGlacier::DrawStageObjects(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const {
  (void)screenH;
  const int px = m_displayTilePx;
  const int topY = static_cast<int>(groundY) - px;

  struct DecorPlacement {
    float worldX;
    int col;
    int row;
  };

  const DecorPlacement decors[] = {
      {256.0f, 0, 0},  {480.0f, 1, 0},  {608.0f, 6, 0},  {832.0f, 7, 0},   {992.0f, 5, 0},
      {1344.0f, 2, 0}, {1664.0f, 0, 1}, {1920.0f, 4, 0}, {2176.0f, 1, 1}, {2560.0f, 3, 0},
      {2880.0f, 6, 1}, {3200.0f, 2, 1},
  };

  for (const DecorPlacement& d : decors) {
    const int x = static_cast<int>(d.worldX + kWorldOffsetX - cameraX);
    if (x < -kDecorDisplayPx * 2 || x > screenW + kDecorDisplayPx * 2) continue;
    const int y = topY - kDecorDisplayPx + 6;
    DrawSheetCell(renderer, m_decor, d.col, d.row, x, y, kDecorDisplayPx, kDecorDisplayPx);
  }

  struct HazardPlacement {
    float worldX;
    int col;
    int row;
    bool ceiling;
  };

  const HazardPlacement hazards[] = {
      {544.0f, 0, 0, false},  {896.0f, 0, 0, false},  {1280.0f, 0, 0, false},
      {1632.0f, 0, 0, false}, {2016.0f, 0, 0, false}, {2400.0f, 0, 0, false},
      {1056.0f, 1, 0, true},  {1760.0f, 1, 0, true},
  };

  for (const HazardPlacement& h : hazards) {
    const int x = static_cast<int>(h.worldX + kWorldOffsetX - cameraX);
    if (x < -px * 2 || x > screenW + px * 2) continue;
    const int y = h.ceiling ? topY - px * 2 - 24 : topY - px;
    DrawSheetCell(renderer, m_hazards, h.col, h.row, x, y, px, px);
  }

  struct PickupPlacement {
    float worldX;
    float yOffset;
    int col;
  };

  const PickupPlacement pickups[] = {
      {416.0f, static_cast<float>(-px - 44), 4},
      {736.0f, static_cast<float>(-px * 2 - 52), 0},
      {1120.0f, static_cast<float>(-px - 44), 5},
      {1440.0f, static_cast<float>(-px * 2 - 52), 3},
      {1888.0f, static_cast<float>(-px - 44), 4},
      {2240.0f, static_cast<float>(-px * 2 - 52), 1},
      {2624.0f, static_cast<float>(-px - 44), 2},
      {3040.0f, static_cast<float>(-px * 2 - 52), 0},
  };

  for (const PickupPlacement& p : pickups) {
    const int x = static_cast<int>(p.worldX + kWorldOffsetX - cameraX);
    if (x < -kPickupDisplayPx * 2 || x > screenW + kPickupDisplayPx * 2) continue;
    const int y = topY + static_cast<int>(p.yOffset);
    DrawSheetCell(renderer, m_pickups, p.col, 0, x, y, kPickupDisplayPx, kPickupDisplayPx);
  }
}

} // namespace cr
