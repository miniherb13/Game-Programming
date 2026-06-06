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

enum class ObstacleStage {
  Mars,
  Glacier,
  Emerald,
};

// assets/stages/{mars|glacier|emerald}/obstacles/*.png
class ObstacleSprites {
public:
  ~ObstacleSprites();

  bool Load();
  void EnsureUploaded(SDL_Renderer* renderer, ObstacleStage stage) const;
  bool IsReady(ObstacleStage stage) const;

  void DrawGrounded(SDL_Renderer* renderer,
                    ObstacleStage stage,
                    ObstacleSpriteId id,
                    float screenX,
                    float footY,
                    float displayHeight) const;
  void DrawFromTop(SDL_Renderer* renderer,
                   ObstacleStage stage,
                   ObstacleSpriteId id,
                   float screenX,
                   float topY,
                   float displayHeight) const;
  void DrawCentered(SDL_Renderer* renderer,
                    ObstacleStage stage,
                    ObstacleSpriteId id,
                    float screenX,
                    float screenY,
                    float displaySize) const;
  void DrawCenteredRotated(SDL_Renderer* renderer,
                           ObstacleStage stage,
                           ObstacleSpriteId id,
                           float screenX,
                           float screenY,
                           float displaySize,
                           float rotationDeg) const;

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

  struct SpriteBank {
    SpriteEntry normal{"normal.png"};
    SpriteEntry tall{"tall.png"};
    SpriteEntry bounce{"bounce.png"};
    SpriteEntry spike{"spike.png"};
    SpriteEntry triangle{"triangle.png"};
    SpriteEntry ceiling{"ceiling.png"};
    SpriteEntry moving{"moving.png"};
    SpriteEntry falling{"falling.png"};
    mutable bool gpuReady = false;
    mutable bool uploadAttempted = false;

    SpriteEntry* Entry(ObstacleSpriteId id);
    const SpriteEntry* Entry(ObstacleSpriteId id) const;
    void DestroyTextures();
  };

  bool LoadFile(const char* folder,
                const char* filename,
                std::vector<unsigned char>& rgba,
                int& w,
                int& h,
                CropRect& outBounds) const;
  bool LoadBank(SpriteBank& bank, const char* folder) const;
  bool UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, SpriteTex& out) const;
  void EnsureBankUploaded(SDL_Renderer* renderer, SpriteBank& bank, ObstacleStage stage) const;
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
  static void BlitScaledCenteredRotated(SDL_Renderer* renderer,
                                        const SpriteTex& sprite,
                                        float screenX,
                                        float screenY,
                                        float displaySize,
                                        float rotationDeg);

  SpriteBank* Bank(ObstacleStage stage);
  const SpriteBank* Bank(ObstacleStage stage) const;

  mutable SpriteBank m_mars{};
  mutable SpriteBank m_glacier{};
  mutable SpriteBank m_emerald{};
};

} // namespace cr
