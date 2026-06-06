#pragma once

#include <vector>

struct SDL_Renderer;

namespace cr {

enum class ItemSpriteId {
  Health,
  Stamina,
  Shield,
};

// assets/items/: health.png, stamina.png, shield.png
class ItemSprites {
public:
  ~ItemSprites();

  bool Load();
  void EnsureUploaded(SDL_Renderer* renderer) const;
  void Draw(SDL_Renderer* renderer, ItemSpriteId id, float screenX, float screenY, float displaySize) const;

  bool IsReady() const { return m_gpuReady; }

private:
  struct CropRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

  struct IconTex {
    void* texture = nullptr; // SDL_Texture*
    int w = 0;
    int h = 0;
    CropRect crop{};
  };

  bool LoadIconFile(const char* filename,
                    std::vector<unsigned char>& rgba,
                    int& w,
                    int& h,
                    CropRect& outBounds) const;
  bool UploadRgba(SDL_Renderer* renderer, const unsigned char* rgba, int w, int h, IconTex& out) const;
  static void DrawCentered(SDL_Renderer* renderer,
                           const IconTex& icon,
                           float screenX,
                           float screenY,
                           int targetSizePx);

  IconTex* IconFor(ItemSpriteId id);
  const IconTex* IconFor(ItemSpriteId id) const;

  mutable IconTex m_health{};
  mutable IconTex m_stamina{};
  mutable IconTex m_shield{};
  mutable bool m_gpuReady = false;
  mutable bool m_uploadAttempted = false;

  std::vector<unsigned char> m_healthPixels{};
  std::vector<unsigned char> m_staminaPixels{};
  std::vector<unsigned char> m_shieldPixels{};
  int m_healthW = 0;
  int m_healthH = 0;
  int m_staminaW = 0;
  int m_staminaH = 0;
  int m_shieldW = 0;
  int m_shieldH = 0;
  CropRect m_healthCrop{};
  CropRect m_staminaCrop{};
  CropRect m_shieldCrop{};
  bool m_hasHealth = false;
  bool m_hasStamina = false;
  bool m_hasShield = false;
};

} // namespace cr
