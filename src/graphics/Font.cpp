#include "Font.h"
#include "Renderer.h"
#include "platform/ResourceManager.h"
#include "core/common/misc.h"
#include "core/utils/Logger.h"
#include "core/game.h"
#include "autogen/sprites.h"

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
    // Load Unifont at native 16px pixel height
    _rayFont = LoadFontEx(unifontPath.c_str(), 16, nullptr, 0);
    SetTextureFilter(_rayFont.texture, TEXTURE_FILTER_POINT);
    _useRayFont = true;
    _height = 16;
    _base = 12;
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

uint32_t Font::drawLTR(int x, int y, const std::string &text, uint32_t color, bool isShaded)
{
  int orgx = x;
  Color c = Color{
    (uint8_t)((color >> 16) & 0xFF),
    (uint8_t)((color >> 8) & 0xFF),
    (uint8_t)(color & 0xFF),
    255
  };

  // Fast-path, Unifont rendering via Raylib
  if (_useRayFont)
  {
    if (_rendering)
    {
      if (isShaded)
        DrawTextEx(_rayFont, text.c_str(), Vector2{(float)x + 1, (float)y + 1}, 16.0f, 0.0f, ::BLACK);
      DrawTextEx(_rayFont, text.c_str(), Vector2{(float)x, (float)y}, 16.0f, 0.0f, c);
    }
    return (uint32_t)MeasureTextEx(_rayFont, text.c_str(), 16.0f, 0.0f).x;
  }

  // BMFont Atlas rendering
  auto it = text.begin();
  while (it != text.end())
  {
    char32_t ch = utf8::next(it, text.end());
    if (ch == '=')
    {
      if (_rendering)
        Renderer::getInstance()->sprites.drawSprite(x, y + 2, SPR_TEXTBULLET);
      x += 7;
      continue;
    }

    if (ch == ' ')
    {
      x += 5;
      continue;
    }

    const Glyph &g = getGlyph(ch);
    Texture2D atlas = getAtlas(g.atlasid);

    if (_rendering && atlas.id != 0)
    {
      Rectangle src{(float)g.x, (float)g.y, (float)g.w, (float)g.h};
      if (isShaded)
      {
        Rectangle dstShd{(float)(x + g.xoffset + 1), (float)(y + g.yoffset + 1), (float)g.w, (float)g.h};
        DrawTexturePro(atlas, src, dstShd, Vector2{0, 0}, 0.0f, ::BLACK);
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