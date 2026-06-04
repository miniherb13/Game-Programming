#pragma once

#include <vector>

struct SDL_Renderer;

namespace cr {

// assets/player/: idle, jump, run_0..N, throw_0..2 (auto crop + background remove)
class PlayerSprite {
public:
  ~PlayerSprite();

  bool Load();
  void EnsureUploaded(SDL_Renderer* renderer) const;
  void Draw(SDL_Renderer* renderer,
            float screenX,
            float footY,
            bool onGround,
            bool paused,
            bool rewinding,
            bool throwing,
            bool throwRelease,
            float throwCharge01,
            float runAnimPhase) const;

  bool IsReady() const { return m_gpuReady; }

private:
  struct CropRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

  struct FrameTex {
    void* texture = nullptr; // SDL_Texture*
    int w = 0;
    int h = 0;
    CropRect crop{};
  };

  bool LoadFrameFile(const char* filename,
                     std::vector<unsigned char>& rgba,
                     int& w,
                     int& h,
                     CropRect& outBounds) const;
  bool UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, FrameTex& out) const;
  static void DrawFrame(SDL_Renderer* renderer,
                        const FrameTex& frame,
                        float screenX,
                        float footY,
                        int targetContentHeightPx);

  mutable FrameTex m_idle{};
  mutable FrameTex m_run[4]{};
  mutable FrameTex m_throw[3]{};
  mutable FrameTex m_jump{};
  mutable bool m_gpuReady = false;
  mutable bool m_uploadAttempted = false;
  mutable bool m_hasIdle = false;
  int m_runFrameCount = 0;

  std::vector<unsigned char> m_idlePixels{};
  std::vector<unsigned char> m_runPixels[4]{};
  std::vector<unsigned char> m_throwPixels[3]{};
  std::vector<unsigned char> m_jumpPixels{};
  int m_idleW = 0;
  int m_idleH = 0;
  CropRect m_idleCrop{};
  int m_runW[4]{};
  int m_runH[4]{};
  CropRect m_runCrop[4]{};
  int m_throwW[3]{};
  int m_throwH[3]{};
  CropRect m_throwCrop[3]{};
  int m_jumpW = 0;
  int m_jumpH = 0;
  CropRect m_jumpCrop{};
};

} // namespace cr
