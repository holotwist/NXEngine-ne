
#ifndef _SURFACE_H
#define _SURFACE_H

#include "basics.h"
#include "types.h"
#include <raylib.h>
#include <string>

namespace NXE
{
namespace Graphics
{

class Surface
{
public:
  Surface();
  ~Surface();

  bool loadImage(const std::string &pbm_name, bool use_colorkey = false);
  static Surface *fromFile(const std::string &pbm_name, bool use_colorkey = false);

  int width() const;
  int height() const;
  const Texture2D &texture() const;

private:
  void cleanup();

  Texture2D _texture;
  int _width;
  int _height;

public:
  int alpha = 255;
};

} // namespace Graphics
} // namespace NXE

#endif
