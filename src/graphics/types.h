#ifndef _G_TYPES_H
#define _G_TYPES_H

#include <cstdint>
#include <raylib.h>

struct NXColor
{
  uint8_t r, g, b;

  NXColor() : r(0), g(0), b(0) {}
  NXColor(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
  NXColor(uint32_t hexcolor)
  {
    r = (hexcolor >> 16) & 0xFF;
    g = (hexcolor >> 8) & 0xFF;
    b = hexcolor & 0xFF;
  }

  Color toRaylib(uint8_t alpha = 255) const
  {
    return Color{r, g, b, alpha};
  }

  inline bool operator==(const NXColor& rhs) const
  {
    return (r == rhs.r && g == rhs.g && b == rhs.b);
  }

  inline bool operator!=(const NXColor& rhs) const
  {
    return !(*this == rhs);
  }
};

struct NXRect
{
  int x, y, w, h;

  Rectangle toRaylib() const
  {
    return Rectangle{(float)x, (float)y, (float)w, (float)h};
  }
};

#endif
