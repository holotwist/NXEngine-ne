#include "Font.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "core/common/misc.h"
#include "Logger.h"
#include "core/game.h"
#include "sprites.h"

#include <json.hpp>
#include <utf8.h>
#include <fstream>
#include <cmath>

namespace NXE
{
namespace Graphics
{

Font::Font() {}
Font::~Font() { cleanup(); }

void Font::cleanup()
{
  _queue.clear();

  if (_useRayFont)
  {
    UnloadFont(_rayFont);
    _useRayFont = false;
  }

  for (auto &atlas : _atlases)
  {
    UnloadTexture(atlas);
  }
  _atlases.clear();
  _glyphs.clear();
}

bool Font::load()
{
  cleanup();

  // Try loading Unifont from resources/
  std::string unifontPath = "resources/unifont.otf";
  if (!FileExists(unifontPath.c_str()))
    unifontPath = "resources/unifont.ttf";

  if (FileExists(unifontPath.c_str()))
  {
    // Load Unifont at native 16px bitmap size
    std::vector<int> codepoints;
    for (int i = 32; i < 256; i++) codepoints.push_back(i);
    for (int i = 0x2010; i <= 0x2026; i++) codepoints.push_back(i);
    for (int i = 0x0400; i <= 0x04FF; i++) codepoints.push_back(i);

    _rayFont = LoadFontEx(unifontPath.c_str(), 16, codepoints.data(), (int)codepoints.size());
    SetTextureFilter(_rayFont.texture, TEXTURE_FILTER_POINT);
    _useRayFont = true;
    _height = 12;
    _base = 10;
    LOG_INFO("Loaded Unifont successfully from '{}'", unifontPath);
    return true;
  }

  // Fallback to bundled font_*.fnt BMFont
  std::string fontFnt = ResourceManager::getInstance()->getPath("font_1.fnt", false);
  std::ifstream fl(widen(fontFnt), std::ifstream::in | std::ifstream::binary);
  if (!fl.is_open())
  {
    LOG_ERROR("Failed to load font file: {}", fontFnt);
    return false;
  }

  nlohmann::json fontdef = nlohmann::json::parse(fl, nullptr, false);
  if (fontdef.is_discarded()) return false;

  _height = fontdef["common"]["lineHeight"].get<uint32_t>();
  _base   = fontdef["common"]["base"].get<uint32_t>();

  for (auto &glyph : fontdef["chars"])
  {
    _glyphs[glyph["id"].get<uint32_t>()] = Glyph{
      glyph["id"].get<uint32_t>(),
      glyph["page"].get<uint32_t>(),
      glyph["x"].get<uint32_t>(),
      glyph["y"].get<uint32_t>(),
      glyph["width"].get<uint32_t>(),
      glyph["height"].get<uint32_t>(),
      glyph["xadvance"].get<uint32_t>(),
      glyph["xoffset"].get<uint32_t>(),
      glyph["yoffset"].get<uint32_t>()
    };
  }

  for (auto &atlas : fontdef["pages"])
  {
    std::string atlasPath = ResourceManager::getInstance()->getPath(atlas.get<std::string>(), false);
    Texture2D tex = LoadTexture(atlasPath.c_str());
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    _atlases.push_back(tex);
  }

  return true;
}

const Font::Glyph &Font::getGlyph(uint32_t codepoint)
{
  auto it = _glyphs.find(codepoint);
  if (it != _glyphs.end()) return it->second;
  static Glyph empty{0, 0, 0, 0, 0, 0, 0, 0, 0};
  return empty;
}

Texture2D Font::getAtlas(uint32_t idx)
{
  if (idx < _atlases.size()) return _atlases[idx];
  return Texture2D{0};
}

uint32_t Font::draw(int x, int y, const std::string &text, uint32_t color, bool isShaded)
{
  return drawLTR(x, y, text, color, isShaded);
}

void Font::clearQueue()
{
  _queue.clear();
}

void Font::flushQueue(float dstX, float dstY, float scaleFactor)
{
  if (!_useRayFont || _queue.empty()) return;

  float fontScale = (float)_height / (float)_rayFont.baseSize;
  float drawSize  = (float)_height * scaleFactor;

  for (const auto &cmd : _queue)
  {
    if (cmd.clipActive)
    {
      int clipX = (int)std::round(dstX + cmd.clip.x * scaleFactor);
      int clipY = (int)std::round(dstY + cmd.clip.y * scaleFactor);
      int clipW = (int)std::round(cmd.clip.width * scaleFactor);
      int clipH = (int)std::round(cmd.clip.height * scaleFactor);
      BeginScissorMode(clipX, clipY, clipW, clipH);
    }

    float sx = dstX + (float)cmd.x * scaleFactor;
    float sy = dstY + (float)cmd.y * scaleFactor;

    Color c = Color{
      (uint8_t)((cmd.color >> 16) & 0xFF),
      (uint8_t)((cmd.color >> 8) & 0xFF),
      (uint8_t)(cmd.color & 0xFF),
      255
    };

    auto it = cmd.text.begin();
    while (it != cmd.text.end())
    {
      char32_t ch = utf8::next(it, cmd.text.end());

      if (ch == '=')
      {
        sx += 7.0f * scaleFactor;
        continue;
      }

      if (ch == ' ')
      {
        sx += 6.0f * scaleFactor;
        continue;
      }

      int glyphIndex = GetGlyphIndex(_rayFont, (int)ch);
      float advX = _rayFont.glyphs[glyphIndex].advanceX > 0
                 ? (float)_rayFont.glyphs[glyphIndex].advanceX
                 : ((float)_rayFont.recs[glyphIndex].width + 1.0f);
      float adv240 = advX * fontScale;

      // Snap to integer pixels
      Vector2 pos = { std::round(sx), std::round(sy) };
      if (cmd.isShaded)
      {
        float shd = std::max(1.0f, std::round(scaleFactor * 0.5f));
        Vector2 posShd = { std::round(sx + shd), std::round(sy + shd) };
        DrawTextCodepoint(_rayFont, (int)ch, posShd, drawSize, Color{0, 0, 0, 255});
      }
      DrawTextCodepoint(_rayFont, (int)ch, pos, drawSize, c);

      sx += adv240 * scaleFactor;
    }

    if (cmd.clipActive)
    {
      EndScissorMode();
    }
  }

  _queue.clear();
}

uint32_t Font::drawLTR(int x, int y, const std::string &text, uint32_t color, bool isShaded)
{
  int orgx = x;

  // Queue high-res overlay command
  if (_rendering && _useRayFont)
  {
    Rectangle clipRect = { 0, 0, 0, 0 };
    bool clipActive = Renderer::getInstance()->isClipSet();
    if (clipActive)
      clipRect = Renderer::getInstance()->getClip();

    _queue.push_back(TextDrawCmd{ x, y, text, color, isShaded, clipRect, clipActive });
  }

  float fontScale = _useRayFont ? ((float)_height / (float)_rayFont.baseSize) : 1.0f;
  Color c = Color{
    (uint8_t)((color >> 16) & 0xFF),
    (uint8_t)((color >> 8) & 0xFF),
    (uint8_t)(color & 0xFF),
    255
  };

  auto it = text.begin();
  while (it != text.end())
  {
    char32_t ch = utf8::next(it, text.end());

    // Dialogue bullet symbol drawn to 240p canvas
    if (ch == '=')
    {
      if (_rendering)
        Renderer::getInstance()->sprites.drawSprite(x, y + 2, SPR_TEXTBULLET);
      x += 7;
      continue;
    }

    if (ch == ' ')
    {
      x += _useRayFont ? 6 : 5;
      continue;
    }

    // Advance measurement for 240p layout
    if (_useRayFont)
    {
      int glyphIndex = GetGlyphIndex(_rayFont, (int)ch);
      float advX = _rayFont.glyphs[glyphIndex].advanceX > 0
                 ? (float)_rayFont.glyphs[glyphIndex].advanceX
                 : ((float)_rayFont.recs[glyphIndex].width + 1.0f);
      float adv240 = advX * fontScale;
      x += (int)std::round(adv240);
      continue;
    }

    // BMFont Atlas fallback
    const Glyph &g = getGlyph(ch);
    Texture2D atlas = getAtlas(g.atlasid);

    if (_rendering && atlas.id != 0)
    {
      Rectangle src{(float)g.x, (float)g.y, (float)g.w, (float)g.h};
      if (isShaded)
      {
        Rectangle dstShd{(float)(x + g.xoffset + 1), (float)(y + g.yoffset + 1), (float)g.w, (float)g.h};
        DrawTexturePro(atlas, src, dstShd, Vector2{0, 0}, 0.0f, Color{0, 0, 0, 255});
      }
      Rectangle dst{(float)(x + g.xoffset), (float)(y + g.yoffset), (float)g.w, (float)g.h};
      DrawTexturePro(atlas, src, dst, Vector2{0, 0}, 0.0f, c);
    }

    x += g.xadvance;
  }

  return (uint32_t)(x - orgx);
}

uint32_t Font::getWidth(const std::string &text)
{
  _rendering = false;
  uint32_t w = draw(0, 0, text);
  _rendering = true;
  return w;
}

uint32_t Font::getHeight() const { return _height; }
uint32_t Font::getBase() const   { return _base; }

} // namespace Graphics
} // namespace NXE