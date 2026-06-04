#pragma once

#include <vector>

struct SDL_Renderer;

namespace cr {

// Stage 2 (glacier). Separate from StageMap (mars) so mars edits stay isolated.
class StageGlacier {
public:
  ~StageGlacier();

  bool Load();
  bool IsLoaded() const { return m_loaded; }
  void EnsureUploaded(SDL_Renderer* renderer) const;
  bool IsDrawReady() const { return m_uploaded && m_tiles.texture != nullptr; }

  void DrawParallaxBackground(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const;
  void DrawParallaxNear(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const;
  void DrawTerrain(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const;
  void DrawStageObjects(SDL_Renderer* renderer, int screenW, int screenH, float cameraX, float groundY) const;

  // World X for stage-2 decor/platforms; matches Game::kGlacierStageStartM.
  static constexpr float kWorldOffsetX = 5000.0f;

private:
  struct SheetAsset {
    std::vector<unsigned char> rgba;
    int w = 0;
    int h = 0;
    int cols = 1;
    int rows = 1;
    mutable void* texture = nullptr;
    int cellW = 32;
    int cellH = 32;
  };

  struct ParallaxAsset {
    std::vector<unsigned char> rgba;
    int w = 0;
    int h = 0;
    mutable void* texture = nullptr;
  };

  bool LoadSheetFile(const char* filename, SheetAsset& out, int cols, int rows, bool stripBackground);
  bool LoadParallaxFile(const char* filename, ParallaxAsset& out, bool stripBackground);

  void UploadSheet(SDL_Renderer* renderer, SheetAsset& sheet) const;
  void UploadParallax(SDL_Renderer* renderer, ParallaxAsset& layer) const;

  void DrawScrollingLayer(SDL_Renderer* renderer,
                          const ParallaxAsset& layer,
                          float cameraX,
                          float scrollFactor,
                          int screenW,
                          int dstY,
                          int dstH) const;

  void DrawTile(SDL_Renderer* renderer, int col, int row, int dstX, int dstY, int dstW, int dstH) const;
  void DrawSheetCell(SDL_Renderer* renderer,
                     const SheetAsset& sheet,
                     int col,
                     int row,
                     int dstX,
                     int dstY,
                     int dstW,
                     int dstH) const;

  bool m_loaded = false;
  mutable bool m_uploaded = false;

  int m_cols = 8;
  int m_rows = 4;
  int m_tileW = 32;
  int m_tileH = 32;
  int m_displayTilePx = 32;

  mutable SheetAsset m_tiles{};
  mutable SheetAsset m_decor{};
  mutable SheetAsset m_hazards{};
  mutable SheetAsset m_pickups{};

  mutable ParallaxAsset m_parallaxFar{};
  mutable ParallaxAsset m_parallaxMid{};
  mutable ParallaxAsset m_parallaxNear{};
};

} // namespace cr
