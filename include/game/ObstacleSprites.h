#pragma once

#include <vector>

struct SDL_Renderer;

namespace cr {

enum class ObstacleSpriteId {
  Normal,
  Tall,
  Bounce,
  Spike,
  Triangle,
  Ceiling,
  Moving,
  Falling,
};

// assets/stages/mars/obstacles/*.png — Stage 1 (Mars) gameplay hazards
class ObstacleSprites {
public:
  ~ObstacleSprites();

  bool Load();
  void EnsureUploaded(SDL_Renderer* renderer) const;
  bool IsReady() const { return m_gpuReady; }

  void DrawGrounded(SDL_Renderer* renderer, ObstacleSpriteId id, float screenX, float footY, float displayHeight) const;
  void DrawFromTop(SDL_Renderer* renderer, ObstacleSpriteId id, float screenX, float topY, float displayHeight) const;
  void DrawCentered(SDL_Renderer* renderer, ObstacleSpriteId id, float screenX, float screenY, float displaySize) const;

private:
  struct CropRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

  struct SpriteTex {
    void* texture = nullptr; // SDL_Texture*
    int w = 0;
    int h = 0;
    CropRect crop{};
  };

  struct SpriteEntry {
    const char* filename = nullptr;
    SpriteTex tex{};
    std::vector<unsigned char> pixels{};
    int w = 0;
    int h = 0;
    CropRect crop{};
    bool loaded = false;
  };

  bool LoadFile(const char* filename,
                std::vector<unsigned char>& rgba,
                int& w,
                int& h,
                CropRect& outBounds) const;
  bool UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, SpriteTex& out) const;
  static void BlitScaled(SDL_Renderer* renderer,
                         const SpriteTex& sprite,
                         float screenX,
                         float anchorY,
                         float displayHeight,
                         bool anchorBottom);
  static void BlitScaledCentered(SDL_Renderer* renderer,
                                 const SpriteTex& sprite,
                                 float screenX,
                                 float screenY,
                                 float displaySize);

  const SpriteTex* TexFor(ObstacleSpriteId id) const;
  SpriteTex* TexFor(ObstacleSpriteId id);

  mutable SpriteEntry m_normal{"normal.png"};
  mutable SpriteEntry m_tall{"tall.png"};
  mutable SpriteEntry m_bounce{"bounce.png"};
  mutable SpriteEntry m_spike{"spike.png"};
  mutable SpriteEntry m_triangle{"triangle.png"};
  mutable SpriteEntry m_ceiling{"ceiling.png"};
  mutable SpriteEntry m_moving{"moving.png"};
  mutable SpriteEntry m_falling{"falling.png"};
  mutable bool m_gpuReady = false;
  mutable bool m_uploadAttempted = false;
};

} // namespace cr
