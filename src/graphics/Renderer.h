#ifndef _RENDERER_H
#define _RENDERER_H

#include "types.h"
#include "Surface.h"
#include "Font.h"
#include "Tileset.h"
#include "Sprites.h"
#include <raylib.h>
#include <string>

class Object;

namespace NXE
{
namespace Graphics
{

const NXColor DK_BLUE(0, 0, 0x21);
const NXColor BLACK(0, 0, 0);
const NXColor WHITE(0xFF, 0xFF, 0xFF);
const NXColor GREEN1(0x0, 0xFF, 0x0);
const NXColor GREEN2(0x0, 0x98, 0x0);
const NXColor GREEN3(0x0, 0x4E, 0x0);
const NXColor GREEN4(0x0, 0x19, 0x0);

struct gres_t
{
  const char *name;
  int width;
  int height;
  int base_width;
  int base_height;
  int scale;
  bool widescreen;
  bool enabled;
};

class Renderer
{
public:
  static Renderer *getInstance();

  int screenWidth  = 426; // 16:9 base width (or 320 for 4:3)
  int screenHeight = 240; // 16:9 base height
  bool widescreen  = true;
  int scale        = 1;

  bool init(int resolution);
  void close();

  bool isWindowVisible();
  void setFullscreen(bool enable);
  bool setResolution(int factor, bool restoreOnFailure = true);
  const gres_t *getResolutions(bool full_list = false);
  int getResolutionCount();

  bool flushAll();
  void showLoadingScreen();

  void beginFrame();
  void endFrame();
  void flip();

  void drawSurface(Surface *src, int x, int y);
  void drawSurface(Surface *src, int dstx, int dsty, int srcx, int srcy, int wd, int ht);
  void drawSurfaceMirrored(Surface *src, int dstx, int dsty, int srcx, int srcy, int wd, int ht);
  void blitPatternAcross(Surface *sfc, int x_dst, int y_dst, int y_src, int height);

  void clearScreen(NXColor color);
  void clearScreen(uint8_t r, uint8_t g, uint8_t b);

  void drawLine(int x1, int y1, int x2, int y2, NXColor color);
  void drawRect(int x1, int y1, int x2, int y2, NXColor color);
  void drawRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b);
  void drawRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b);
  void drawRect(NXRect *rect, NXColor color);

  void fillRect(int x1, int y1, int x2, int y2, NXColor color);
  void fillRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b);
  void fillRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b);
  void fillRect(NXRect *rect, NXColor color);void drawRect(int x1, int y1, int x2, int y2, NXColor color);
  void drawRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b);
  void drawRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b);
  void drawRect(NXRect *rect, NXColor color);

  void fillRect(int x1, int y1, int x2, int y2, NXColor color);
  void fillRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b);
  void fillRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b);
  void fillRect(NXRect *rect, NXColor color);

  void drawPixel(int x, int y, NXColor color);
  void drawPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);

  void setClip(int x, int y, int w, int h);
  void setClip(NXRect *rect);
  void clearClip();
  bool isClipSet();

  void saveScreenshot();
  void drawSpotLight(int x, int y, Object *o, int r = 255, int g = 255, int b = 255, int upscale = 6);
  void tintScreen();

  Font font;
  Tileset tileset;
  Sprites sprites;

private:
  Renderer();
  ~Renderer();

  RenderTexture2D _target;
  Texture2D _spotLightTex;
  Rectangle _clipRect;
  bool _clipActive = false;
  int _currentRes  = 1;
};

inline void Renderer::drawSurface(Surface *src, int dstx, int dsty)
{
  drawSurface(src, dstx, dsty, 0, 0, src->width(), src->height());
}

inline void Renderer::drawRect(int x1, int y1, int x2, int y2, NXColor color)
{
  drawRect(x1, y1, x2, y2, color.r, color.g, color.b);
}

inline void Renderer::drawRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b)
{
  drawRect(rect->x, rect->y, rect->x + (rect->w - 1), rect->y + (rect->h - 1), r, g, b);
}

inline void Renderer::drawRect(NXRect *rect, NXColor color)
{
  drawRect(rect->x, rect->y, rect->x + (rect->w - 1), rect->y + (rect->h - 1), color.r, color.g, color.b);
}

inline void Renderer::fillRect(int x1, int y1, int x2, int y2, NXColor color)
{
  fillRect(x1, y1, x2, y2, color.r, color.g, color.b);
}

inline void Renderer::fillRect(NXRect *rect, uint8_t r, uint8_t g, uint8_t b)
{
  fillRect(rect->x, rect->y, rect->x + (rect->w - 1), rect->y + (rect->h - 1), r, g, b);
}

inline void Renderer::fillRect(NXRect *rect, NXColor color)
{
  fillRect(rect->x, rect->y, rect->x + (rect->w - 1), rect->y + (rect->h - 1), color.r, color.g, color.b);
}

inline void Renderer::drawPixel(int x, int y, NXColor color)
{
  drawPixel(x, y, color.r, color.g, color.b);
}

inline void Renderer::clearScreen(NXColor color)
{
  clearScreen(color.r, color.g, color.b);
}

inline void Renderer::setClip(NXRect *rect)
{
  setClip(rect->x, rect->y, rect->w, rect->h);
}

} // namespace Graphics
} // namespace NXE

#endif