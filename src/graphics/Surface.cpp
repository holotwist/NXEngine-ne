#include "Surface.h"
#include "Renderer.h"
#include "Logger.h"
#include <algorithm>

namespace NXE
{
namespace Graphics
{

Surface::Surface()
  : _texture{0}
  , _width(0)
  , _height(0)
  , alpha(255)
{
}

Surface::~Surface()
{
  cleanup();
}

bool Surface::loadImage(const std::string &filename, bool use_colorkey)
{
  cleanup();

  int dataSize = 0;
  unsigned char *fileData = LoadFileData(filename.c_str(), &dataSize);
  if (!fileData)
  {
    LOG_ERROR("Surface::loadImage: failed to read file '{}'", filename);
    return false;
  }

  // Cave Story .pbm files are standard BMP files
  const char *fileType = ".bmp";
  std::string lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if (lower.rfind(".png") != std::string::npos)
    fileType = ".png";

  Image image = LoadImageFromMemory(fileType, fileData, dataSize);
  UnloadFileData(fileData);

  if (image.data == nullptr)
  {
    LOG_ERROR("Surface::loadImage: failed to decode image '{}'", filename);
    return false;
  }

  _width = image.width;
  _height = image.height;

  // Replace black (colorkey) with transparency
  if (use_colorkey)
  {
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageColorReplace(&image, Color{0, 0, 0, 255}, Color{0, 0, 0, 0});
  }

  _texture = LoadTextureFromImage(image);
  SetTextureFilter(_texture, TEXTURE_FILTER_POINT);

  UnloadImage(image);
  return (_texture.id != 0);
}

Surface *Surface::fromFile(const std::string &pbm_name, bool use_colorkey)
{
  Surface *sfc = new Surface;
  if (!sfc->loadImage(pbm_name, use_colorkey))
  {
    delete sfc;
    return nullptr;
  }
  return sfc;
}

int Surface::width() const
{
  return _width;
}

int Surface::height() const
{
  return _height;
}

const Texture2D &Surface::texture() const
{
  return _texture;
}

void Surface::cleanup()
{
  if (_texture.id != 0)
  {
    UnloadTexture(_texture);
    _texture = Texture2D{0};
  }
  _width = 0;
  _height = 0;
}

} // namespace Graphics
} // namespace NXE
