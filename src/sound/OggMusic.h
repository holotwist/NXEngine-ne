#ifndef _OGGMUSIC_H
#define _OGGMUSIC_H

#include "Singleton.h"
#include "miniaudio.h"
#include <string>
#include <cstdint>

namespace NXE
{
namespace Sound
{

class OggMusic
{
public:
  static OggMusic *getInstance();

  bool init();
  void shutdown();

  bool load(const std::string &introPath, const std::string &loopPath, bool loops = true);
  bool load(const std::string &path, bool loops = true);

  void play(bool resume = false);
  void stop();
  void pause();
  void resume();
  bool isPlaying() const { return _playing; }

  void fade();
  void runFade();
  void setVolume(float vol) { _volume = vol; }

  void renderAudio(int16_t *stream, uint32_t frameCount);

  uint32_t getPosition() const;
  void setPosition(uint32_t pos);

protected:
  friend class Singleton<OggMusic>;
  OggMusic();
  ~OggMusic();

private:
  void _cleanup();

  ma_decoder _introDecoder;
  ma_decoder _loopDecoder;
  bool _introLoaded = false;
  bool _loopLoaded = false;
  bool _hasIntro = false;
  bool _playingIntro = false;
  bool _playing = false;
  bool _loops = true;

  float _volume = 1.0f;
  bool _fading = false;
  uint32_t _lastFadeTime = 0;

  uint32_t _savedPosition = 0;
  bool _savedInIntro = false;
};

} // namespace Sound
} // namespace NXE

#endif