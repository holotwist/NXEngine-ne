#ifndef _BMFONT_H
#define _BMFONT_H

#include <raylib.h>
#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace NXE
{
namespace Graphics
{

class Font
{
public:
  struct Glyph
  {
    uint32_t glyph_id;
    uint32_t atlasid;
    uint32_t x, y, w, h;
    uint32_t xadvance, xoffset, yoffset;
  };

  Font();
  ~Font();

  bool load();
  void cleanup();

  struct TextDrawCmd
  {
    int x, y;
    std::string text;
    uint32_t color;
    bool isShaded;
    Rectangle clip;
    bool clipActive;
  };

  uint32_t draw(int x, int y, const std::string &text, uint32_t color = 0xFFFFFF, bool isShaded = false);
  uint32_t drawLTR(int x, int y, const std::string &text, uint32_t color = 0xFFFFFF, bool isShaded = false);
  uint32_t getWidth(const std::string &text);
  uint32_t getHeight() const;
  uint32_t getBase() const;

  void flushQueue(float dstX, float dstY, float scaleFactor);
  void clearQueue();

private:
  std::vector<TextDrawCmd> _queue;
  const Glyph &getGlyph(uint32_t codepoint);
  Texture2D getAtlas(uint32_t idx);

  // Unifont / TTF support via Raylib
  ::Font _rayFont;
  bool _useRayFont = false;

  // BMFont Fallback
  std::vector<Texture2D> _atlases;
  std::map<uint32_t, Glyph> _glyphs;
  uint32_t _height = 12;
  uint32_t _base   = 10;
  bool _rendering  = true;
};

} // namespace Graphics
} // namespace NXE

#endif