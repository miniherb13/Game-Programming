#include "render/UiText.h"

#include "core/Log.h"

#include <SDL.h>

#include <cmath>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cr {

namespace {

#if defined(_WIN32)
std::wstring Utf8ToWide(const char* text) {
  if (!text || !*text) return {};
  const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
  if (needed <= 0) return {};
  std::vector<wchar_t> buf(static_cast<std::size_t>(needed));
  MultiByteToWideChar(CP_UTF8, 0, text, -1, buf.data(), needed);
  if (buf.empty()) return {};
  if (buf.back() == L'\0') buf.pop_back();
  return std::wstring(buf.begin(), buf.end());
}
#endif

} // namespace

bool UiText::Init(int fontPtSize) {
  m_fontPtSize = fontPtSize;
#if defined(_WIN32)
  m_ready = true;
  return true;
#else
  Log(LogLevel::Warn, "UiText: Korean UI text is only supported on Windows builds");
  m_ready = false;
  return false;
#endif
}

void UiText::Shutdown() {
  m_ready = false;
}

int UiText::LineHeight() const {
  return m_fontPtSize + 8;
}

#if defined(_WIN32)
void UiText::Draw(SDL_Renderer* renderer, int x, int y, const char* text, SDL_Color color) const {
  if (!m_ready || !text || !*text) return;

  const std::wstring wide = Utf8ToWide(text);
  if (wide.empty()) return;

  HDC screenDc = GetDC(nullptr);
  if (!screenDc) return;

  HFONT font = CreateFontW(-m_fontPtSize,
                           0,
                           0,
                           0,
                           FW_NORMAL,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE,
                           L"Malgun Gothic");
  HFONT oldFont = static_cast<HFONT>(SelectObject(screenDc, font));

  SIZE size{};
  GetTextExtentPoint32W(screenDc, wide.c_str(), static_cast<int>(wide.size()), &size);
  const int w = size.cx + 4;
  const int h = size.cy + 4;

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bmp || !bits) {
    SelectObject(screenDc, oldFont);
    DeleteObject(font);
    ReleaseDC(nullptr, screenDc);
    return;
  }

  HDC memDc = CreateCompatibleDC(screenDc);
  HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDc, bmp));
  SelectObject(memDc, font);

  const UINT bg = 0x00000000;
  const RECT fillRc{0, 0, w, h};
  const HBRUSH brush = CreateSolidBrush(bg);
  FillRect(memDc, &fillRc, brush);
  DeleteObject(brush);

  SetBkMode(memDc, TRANSPARENT);
  SetTextColor(memDc, RGB(color.r, color.g, color.b));
  TextOutW(memDc, 2, 2, wide.c_str(), static_cast<int>(wide.size()));

  auto* pixels = static_cast<Uint8*>(bits);
  std::vector<Uint8> rgba(static_cast<std::size_t>(w * h * 4));
  for (int py = 0; py < h; py++) {
    for (int px = 0; px < w; px++) {
      const int i = (py * w + px) * 4;
      const Uint8 b = pixels[i + 0];
      const Uint8 g = pixels[i + 1];
      const Uint8 r = pixels[i + 2];
      const Uint8 lum = static_cast<Uint8>((static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3);
      rgba[static_cast<std::size_t>(i) + 0] = color.r;
      rgba[static_cast<std::size_t>(i) + 1] = color.g;
      rgba[static_cast<std::size_t>(i) + 2] = color.b;
      rgba[static_cast<std::size_t>(i) + 3] =
          static_cast<Uint8>((static_cast<int>(lum) * static_cast<int>(color.a)) / 255);
    }
  }

  SDL_Surface* surface =
      SDL_CreateRGBSurfaceFrom(rgba.data(), w, h, 32, w * 4, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
  if (surface) {
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_Rect dst{x, y, w, h};
      SDL_RenderCopy(renderer, texture, nullptr, &dst);
      SDL_DestroyTexture(texture);
    }
  }

  SelectObject(memDc, oldBmp);
  SelectObject(memDc, oldFont);
  DeleteObject(bmp);
  DeleteObject(font);
  DeleteDC(memDc);
  ReleaseDC(nullptr, screenDc);
}
#else
void UiText::Draw(SDL_Renderer* /*renderer*/, int /*x*/, int /*y*/, const char* /*text*/, SDL_Color /*color*/) const {}
#endif

void UiText::DrawCentered(SDL_Renderer* renderer, int centerX, int y, const char* text, SDL_Color color) const {
#if defined(_WIN32)
  if (!m_ready || !text || !*text) return;

  const std::wstring wide = Utf8ToWide(text);
  if (wide.empty()) return;

  HDC screenDc = GetDC(nullptr);
  if (!screenDc) return;

  HFONT font = CreateFontW(-m_fontPtSize,
                           0,
                           0,
                           0,
                           FW_NORMAL,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE,
                           L"Malgun Gothic");
  HFONT oldFont = static_cast<HFONT>(SelectObject(screenDc, font));

  SIZE size{};
  GetTextExtentPoint32W(screenDc, wide.c_str(), static_cast<int>(wide.size()), &size);
  SelectObject(screenDc, oldFont);
  DeleteObject(font);
  ReleaseDC(nullptr, screenDc);

  Draw(renderer, centerX - size.cx / 2, y, text, color);
#else
  (void)renderer;
  (void)centerX;
  (void)y;
  (void)text;
  (void)color;
#endif
}

void UiText::DrawCenteredBlink(SDL_Renderer* renderer,
                               int centerX,
                               int y,
                               const char* text,
                               SDL_Color color,
                               float blinkPhaseSeconds,
                               float periodSeconds) const {
  const float wave = 0.5f + 0.5f * std::sin(blinkPhaseSeconds * 6.2831853f / periodSeconds);
  color.a = static_cast<Uint8>(80.0f + wave * 175.0f);
  DrawCentered(renderer, centerX, y, text, color);
}

} // namespace cr
