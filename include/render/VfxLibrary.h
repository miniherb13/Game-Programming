#pragma once

#include "math/Vec2.h"

#include <SDL.h>
#include <cstdint>

namespace cr {

// Procedural glow textures + optional black hole PNG assets (assets/vfx/blackhole_*.png).
class VfxLibrary {
public:
  static VfxLibrary& Instance();

  bool Init(SDL_Renderer* renderer);
  void Shutdown();
  bool IsReady() const { return m_ready; }
  bool HasBlackHoleAssets() const { return m_assetAccretionRing.tex != nullptr; }

  static float Envelope(float normalizedLife, float fadeInFrac, float fadeOutFrac);

  void DrawSoftGlow(SDL_Renderer* r,
                    float x,
                    float y,
                    float radius,
                    Uint8 cr,
                    Uint8 cg,
                    Uint8 cb,
                    Uint8 alpha,
                    float rotationDeg = 0.0f) const;

  void DrawRingGlow(SDL_Renderer* r,
                    float x,
                    float y,
                    float radius,
                    float scale,
                    Uint8 cr,
                    Uint8 cg,
                    Uint8 cb,
                    Uint8 alpha,
                    float rotationDeg = 0.0f) const;

  void DrawSpark(SDL_Renderer* r,
                 float x,
                 float y,
                 float size,
                 Uint8 cr,
                 Uint8 cg,
                 Uint8 cb,
                 Uint8 alpha) const;

  void DrawRewindSpark(SDL_Renderer* r, float x, float y, float size, Uint8 alpha) const;
  void DrawRewindPixelSpark(SDL_Renderer* r,
                            float x,
                            float y,
                            float size,
                            Uint8 cr,
                            Uint8 cg,
                            Uint8 cb,
                            Uint8 alpha) const;

  void DrawRewindGhostAura(SDL_Renderer* r,
                           float x,
                           float y,
                           float radius,
                           Uint8 alpha,
                           float rotationDeg = 0.0f) const;

  void DrawSpiralArms(SDL_Renderer* r,
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
                      float spiralTurns = 5.5f,
                      float whiteHalo = 0.35f) const;

  void DrawShockwaveRing(SDL_Renderer* r,
                         float cx,
                         float cy,
                         float radius,
                         Uint8 cr,
                         Uint8 cg,
                         Uint8 cb,
                         Uint8 alpha,
                         float thickness = 5.0f) const;

  void DrawBlackHole(SDL_Renderer* r,
                     float cx,
                     float cy,
                     float radius,
                     float lifeT,
                     float spinAngle,
                     float darkBackdropStrength = 0.0f) const;

  void DrawRewindStarfield(SDL_Renderer* r, int screenW, int screenH, float fade, float spinAngle) const;

  void DrawRewindVortex(SDL_Renderer* r,
                        float cx,
                        float cy,
                        float radius,
                        float lifeT,
                        float spinAngle) const;

  void DrawVignette(SDL_Renderer* r, int w, int h, Uint8 alpha, Uint8 cr, Uint8 cg, Uint8 cb) const;

private:
  struct AssetTex {
    SDL_Texture* tex = nullptr;
    int w = 0;
    int h = 0;
    int frameCount = 1;
  };

  VfxLibrary() = default;

  bool CreateSoftGlowTexture(SDL_Renderer* renderer, int size);
  bool CreateRingTexture(SDL_Renderer* renderer, int size);
  bool CreateSparkTexture(SDL_Renderer* renderer, int size);
  bool LoadBlackHoleAssets(SDL_Renderer* renderer);

  void DrawAsset(SDL_Renderer* r,
                 const AssetTex& asset,
                 float cx,
                 float cy,
                 float displayRadius,
                 Uint8 alpha,
                 float rotationDeg,
                 SDL_BlendMode blend,
                 int frameIndex = 0,
                 Uint8 cr = 255,
                 Uint8 cg = 255,
                 Uint8 cb = 255) const;

  SDL_Texture* m_softGlow = nullptr;
  SDL_Texture* m_ringGlow = nullptr;
  SDL_Texture* m_spark = nullptr;

  AssetTex m_assetSoftGlow{};
  AssetTex m_assetAccretionRing{};
  AssetTex m_assetShockwave{};
  AssetTex m_assetEmberSheet{};
  AssetTex m_assetSprite{};

  int m_texSize = 256;
  bool m_ready = false;
};

} // namespace cr
