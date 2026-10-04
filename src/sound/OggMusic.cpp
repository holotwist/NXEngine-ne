#include "OggMusic.h"
#include "Common.h"
#include "settings.h"
#include "SoundManager.h"
#include "Logger.h"

#include <chrono>
#include <cstring>

namespace NXE
{
namespace Sound
{

OggMusic::OggMusic()
{
  std::memset(&_introDecoder, 0, sizeof(_introDecoder));
  std::memset(&_loopDecoder, 0, sizeof(_loopDecoder));
}

OggMusic::~OggMusic()
{
  shutdown();
}

OggMusic *OggMusic::getInstance()
{
  return Singleton<OggMusic>::get();
}

bool OggMusic::init()
{
  _cleanup();
  return true;
}

void OggMusic::shutdown()
{
  _cleanup();
}

void OggMusic::_cleanup()
{
  _playing = false;
  if (_introLoaded)
  {
    ma_decoder_uninit(&_introDecoder);
    std::memset(&_introDecoder, 0, sizeof(_introDecoder));
    _introLoaded = false;
  }
  if (_loopLoaded)
  {
    ma_decoder_uninit(&_loopDecoder);
    std::memset(&_loopDecoder, 0, sizeof(_loopDecoder));
    _loopLoaded = false;
  }
  _hasIntro = false;
  _playingIntro = false;
}

bool OggMusic::load(const std::string &introPath, const std::string &loopPath, bool loops)
{
  _cleanup();

  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, AUDIO_CHANNELS, SAMPLE_RATE);

  if (ma_decoder_init_file(introPath.c_str(), &config, &_introDecoder) != MA_SUCCESS)
    return false;
  _introLoaded = true;

  if (ma_decoder_init_file(loopPath.c_str(), &config, &_loopDecoder) != MA_SUCCESS)
  {
    _cleanup();
    return false;
  }
  _loopLoaded = true;

  _hasIntro = true;
  _playingIntro = true;
  _loops = loops;
  _savedPosition = 0;
  _savedInIntro = true;
  return true;
}

bool OggMusic::load(const std::string &path, bool loops)
{
  _cleanup();

  ma_decoder_config config = ma_decoder_config_init(ma_format_s16, AUDIO_CHANNELS, SAMPLE_RATE);
  if (ma_decoder_init_file(path.c_str(), &config, &_loopDecoder) != MA_SUCCESS)
    return false;

  _loopLoaded = true;
  _hasIntro = false;
  _playingIntro = false;
  _loops = loops;
  _savedPosition = 0;
  _savedInIntro = false;
  return true;
}

void OggMusic::play(bool resume)
{
  if (!_loopLoaded && !_introLoaded) return;
  _playing = true;
  _fading = false;
  _volume = 1.0f;

  if (resume)
  {
    _playingIntro = _savedInIntro;
    if (_hasIntro && _playingIntro && _introLoaded)
      ma_decoder_seek_to_pcm_frame(&_introDecoder, _savedPosition);
    else if (_loopLoaded)
      ma_decoder_seek_to_pcm_frame(&_loopDecoder, _savedPosition);
  }
  else
  {
    _playingIntro = _hasIntro;
    if (_hasIntro && _introLoaded)
      ma_decoder_seek_to_pcm_frame(&_introDecoder, 0);
    if (_loopLoaded)
      ma_decoder_seek_to_pcm_frame(&_loopDecoder, 0);
  }
}

void OggMusic::stop()
{
  if (_playing)
  {
    _savedInIntro = (_hasIntro && _playingIntro);
    _savedPosition = getPosition();
    _playing = false;
  }
}

void OggMusic::pause()
{
  _playing = false;
}

void OggMusic::resume()
{
  if (_loopLoaded || _introLoaded)
    _playing = true;
}

uint32_t OggMusic::getPosition() const
{
  ma_uint64 cursor = 0;
  if (_hasIntro && _playingIntro && _introLoaded)
    ma_decoder_get_cursor_in_pcm_frames(const_cast<ma_decoder*>(&_introDecoder), &cursor);
  else if (_loopLoaded)
    ma_decoder_get_cursor_in_pcm_frames(const_cast<ma_decoder*>(&_loopDecoder), &cursor);
  return (uint32_t)cursor;
}

void OggMusic::setPosition(uint32_t pos)
{
  if (_hasIntro && _playingIntro && _introLoaded)
    ma_decoder_seek_to_pcm_frame(&_introDecoder, pos);
  else if (_loopLoaded)
    ma_decoder_seek_to_pcm_frame(&_loopDecoder, pos);
}

void OggMusic::fade()
{
  _fading = true;
  _lastFadeTime = 0;
}

void OggMusic::runFade()
{
  if (!_fading) return;

  uint32_t cur = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();

  if (cur - _lastFadeTime >= 25)
  {
    _volume -= 0.01f;
    if (_volume <= 0.0f)
    {
      _volume = 0.0f;
      _fading = false;
      stop();
    }
    _lastFadeTime = cur;
  }
}

void OggMusic::renderAudio(int16_t *stream, uint32_t frameCount)
{
  if (!_playing) return;

  float effectiveVol = _volume * ((float)settings->music_volume / 100.0f);
  if (effectiveVol <= 0.0f) return;

  uint32_t framesRendered = 0;
  int16_t tempBuffer[512 * AUDIO_CHANNELS];

  while (framesRendered < frameCount && _playing)
  {
    uint32_t framesToRead = frameCount - framesRendered;
    if (framesToRead > 512) framesToRead = 512;

    ma_decoder *curDecoder = (_hasIntro && _playingIntro) ? &_introDecoder : &_loopDecoder;
    ma_uint64 framesRead = 0;

    ma_result res = ma_decoder_read_pcm_frames(curDecoder, tempBuffer, framesToRead, &framesRead);
    (void)res;

    for (uint32_t i = 0; i < (uint32_t)framesRead; i++)
    {
      uint32_t outIdx = (framesRendered + i) * AUDIO_CHANNELS;
      int32_t sL = stream[outIdx]     + (int32_t)(tempBuffer[i * 2] * effectiveVol);
      int32_t sR = stream[outIdx + 1] + (int32_t)(tempBuffer[i * 2 + 1] * effectiveVol);
      stream[outIdx]     = (int16_t)clamp(sL, -32768, 32767);
      stream[outIdx + 1] = (int16_t)clamp(sR, -32768, 32767);
    }

    framesRendered += (uint32_t)framesRead;

    if (framesRead < framesToRead)
    {
      if (_hasIntro && _playingIntro)
      {
        _playingIntro = false;
        if (_loopLoaded)
          ma_decoder_seek_to_pcm_frame(&_loopDecoder, 0);
      }
      else if (_loops)
      {
        if (_loopLoaded)
          ma_decoder_seek_to_pcm_frame(&_loopDecoder, 0);
        else
          _playing = false;
      }
      else
      {
        _playing = false;
        break;
      }
    }
  }
}

} // namespace Sound
} // namespace NXE