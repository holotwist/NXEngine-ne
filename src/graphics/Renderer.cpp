#include "Renderer.h"
#include "core/object.h"
#include "Logger.h"
#include "ResourceManager.h"
#include "core/version.h"
#include "nx.h"
#include <cmath>
#include <algorithm>

namespace NXE
{
namespace Graphics
{

static const gres_t g_resolutions[] = {
  {"320x240 (4:3)",    320,  240, 320, 240, 1, false, true},
  {"426x240 (16:9)",   426,  240, 426, 240, 1, true,  true},
  {"854x480 (16:9)",   854,  480, 426, 240, 2, true,  true},
  {"1280x720 (16:9)",  1280, 720, 426, 240, 3, true,  true},
  {"1920x1080 (16:9)", 1920, 1080, 426, 240, 4, true,  true},
  {nullptr, 0, 0, 0, 0, 0, false, false}
};

Renderer::Renderer()
{
  _target.id = 0;
  _spotLightTex.id = 0;
}

Renderer::~Renderer()
{
  close();
}

Renderer *Renderer::getInstance()
{
  static Renderer instance;
  return &instance;
}

bool Renderer::init(int resolution)
{
  _currentRes = resolution;
  const gres_t *res = getResolutions();

  screenWidth  = res[_currentRes].base_width;
  screenHeight = res[_currentRes].base_height;
  widescreen   = res[_currentRes].widescreen;
  scale        = res[_currentRes].scale;

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(res[_currentRes].width, res[_currentRes].height, NXVERSION);
  SetExitKey(KEY_NULL); // Disable Raylib default exit on ESC
  SetWindowMinSize(320, 240);
  SetTargetFPS(50); // Standard freq ticks

  _target = LoadRenderTexture(screenWidth, screenHeight);
  SetTextureFilter(_target.texture, TEXTURE_FILTER_POINT);

  std::string spotPath = ResourceManager::getInstance()->getPath("spot.png", false);
  if (FileExists(spotPath.c_str()))
  {
    _spotLightTex = LoadTexture(spotPath.c_str());
  }

  if (!font.load()) return false;
  if (!sprites.init()) return false;

  return true;
}

void Renderer::close()
{
  font.cleanup();
  sprites.close();

  if (_spotLightTex.id != 0)
  {
    UnloadTexture(_spotLightTex);
    _spotLightTex.id = 0;
  }
  if (_target.id != 0)
  {
    UnloadRenderTexture(_target);
    _target.id = 0;
  }
  if (IsWindowReady())
  {
    CloseWindow();
  }
}

bool Renderer::isWindowVisible()
{
  return !IsWindowMinimized() && !IsWindowHidden();
}

void Renderer::setFullscreen(bool enable)
{
  if (IsWindowFullscreen() != enable)
  {
    ToggleFullscreen();
  }
}

bool Renderer::setResolution(int factor, bool restoreOnFailure)
{
  (void)restoreOnFailure;
  const gres_t *res = getResolutions();
  int count = getResolutionCount();
  if (factor < 0 || factor >= count) return false;

  _currentRes  = factor;
  screenWidth  = res[_currentRes].base_width;
  screenHeight = res[_currentRes].base_height;
  widescreen   = res[_currentRes].widescreen;
  scale        = res[_currentRes].scale;

  SetWindowSize(res[_currentRes].width, res[_currentRes].height);

  if (_target.id != 0) UnloadRenderTexture(_target);
  _target = LoadRenderTexture(screenWidth, screenHeight);
  SetTextureFilter(_target.texture, TEXTURE_FILTER_POINT);

  return flushAll();
}

const gres_t *Renderer::getResolutions(bool full_list)
{
  (void)full_list;
  return g_resolutions;
}

int Renderer::getResolutionCount()
{
  int i = 0;
  while (g_resolutions[i].name != nullptr) i++;
  return i;
}

bool Renderer::flushAll()
{
  sprites.flushSheets();
  tileset.reload();
  return font.load();
}

void Renderer::showLoadingScreen()
{
  Surface loading;
  if (!loading.loadImage(ResourceManager::getInstance()->getPath("Loading.pbm", false))) return;

  int x = (screenWidth / 2) - (loading.width() / 2);
  int y = (screenHeight / 2) - loading.height();

  beginFrame();
  clearScreen(BLACK);
  drawSurface(&loading, x, y);
  endFrame();
  flip();
}

void Renderer::beginFrame()
{
  font.clearQueue();
  BeginTextureMode(_target);
}

void Renderer::endFrame()
{
  if (_clipActive) clearClip();
  EndTextureMode();
}

void Renderer::flip()
{
  BeginDrawing();
  ClearBackground(Color{0, 0, 0, 255});

  // Pillarbox/letterbox target presentation
  float scaleFactor = std::min((float)GetScreenWidth() / (float)screenWidth,
                               (float)GetScreenHeight() / (float)screenHeight);

  Rectangle src = { 0.0f, 0.0f, (float)screenWidth, -(float)screenHeight }; // Invert Y for FBO
  Rectangle dst = {
    (GetScreenWidth() - ((float)screenWidth * scaleFactor)) * 0.5f,
    (GetScreenHeight() - ((float)screenHeight * scaleFactor)) * 0.5f,
    (float)screenWidth * scaleFactor,
    (float)screenHeight * scaleFactor
  };

  DrawTexturePro(_target.texture, src, dst, Vector2{0.0f, 0.0f}, 0.0f, Color{255, 255, 255, 255});

  // Render high-res text overlay
  font.flushQueue(dst.x, dst.y, scaleFactor);

  EndDrawing();
}

void Renderer::drawSurface(Surface *src, int dstx, int dsty, int srcx, int srcy, int wd, int ht)
{
  if (!src || src->texture().id == 0) return;

  Rectangle srcRec = { (float)srcx, (float)srcy, (float)wd, (float)ht };
  Rectangle dstRec = { (float)dstx, (float)dsty, (float)wd, (float)ht };
  Color tint = ColorAlpha(Color{255, 255, 255, 255}, (float)src->alpha / 255.0f);

  DrawTexturePro(src->texture(), srcRec, dstRec, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void Renderer::drawSurfaceMirrored(Surface *src, int dstx, int dsty, int srcx, int srcy, int wd, int ht)
{
  if (!src || src->texture().id == 0) return;

  Rectangle srcRec = { (float)srcx, (float)srcy, -(float)wd, (float)ht }; // Negative width mirrors
  Rectangle dstRec = { (float)dstx, (float)dsty, (float)wd, (float)ht };
  Color tint = ColorAlpha(Color{255, 255, 255, 255}, (float)src->alpha / 255.0f);

  DrawTexturePro(src->texture(), srcRec, dstRec, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void Renderer::blitPatternAcross(Surface *sfc, int x_dst, int y_dst, int y_src, int height)
{
  if (!sfc || sfc->texture().id == 0) return;

  int w = sfc->width();
  int x = x_dst;

  while (x < screenWidth)
  {
    drawSurface(sfc, x, y_dst, 0, y_src, w, height);
    x += w;
  }
}

void Renderer::clearScreen(uint8_t r, uint8_t g, uint8_t b)
{
  ClearBackground(Color{r, g, b, 255});
}

void Renderer::drawLine(int x1, int y1, int x2, int y2, NXColor color)
{
  DrawLine(x1, y1, x2, y2, color.toRaylib());
}

void Renderer::drawRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b)
{
  DrawRectangleLines(x1, y1, (x2 - x1) + 1, (y2 - y1) + 1, Color{r, g, b, 255});
}

void Renderer::fillRect(int x1, int y1, int x2, int y2, uint8_t r, uint8_t g, uint8_t b)
{
  DrawRectangle(x1, y1, (x2 - x1) + 1, (y2 - y1) + 1, Color{r, g, b, 255});
}

void Renderer::drawPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
  DrawPixel(x, y, Color{r, g, b, 255});
}

void Renderer::setClip(int x, int y, int w, int h)
{
  if (_clipActive) EndScissorMode();
  _clipRect = Rectangle{(float)x, (float)y, (float)w, (float)h};
  _clipActive = true;
  BeginScissorMode(x, y, w, h);
}

void Renderer::clearClip()
{
  if (_clipActive)
  {
    EndScissorMode();
    _clipActive = false;
  }
}

bool Renderer::isClipSet()
{
  return _clipActive;
}

void Renderer::tintScreen()
{
  DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 150});
}

void Renderer::drawSpotLight(int x, int y, Object *o, int r, int g, int b, int upscale)
{
  if (_spotLightTex.id == 0 || !o) return;

  int w = (o->Width() / CSFI) * upscale;
  int h = (o->Height() / CSFI) * upscale;

  Rectangle src = {0.0f, 0.0f, (float)_spotLightTex.width, (float)_spotLightTex.height};
  Rectangle dst = {(float)(x - (w / 2)), (float)(y - (h / 2)), (float)w, (float)h};

  DrawTexturePro(_spotLightTex, src, dst, Vector2{0.0f, 0.0f}, 0.0f, Color{(uint8_t)r, (uint8_t)g, (uint8_t)b, 255});
}

void Renderer::saveScreenshot()
{
  TakeScreenshot("screenshot.png");
}

} // namespace Graphics
} // namespace NXE