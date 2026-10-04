#include "SoundManager.h"
#include "Pixtone.h"
#include "Organya.h"
#include "OggMusic.h"
#include "ResourceManager.h"
#include "core/common/misc.h"
#include "Logger.h"
#include "core/game.h"
#include "core/settings.h"

#include <json.hpp>
#include <fstream>
#include <cstring>

namespace NXE
{
namespace Sound
{

static void ma_audio_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
  SoundManager *sm = static_cast<SoundManager *>(pDevice->pUserData);
  if (sm)
  {
    sm->audioCallback(pOutput, pInput, frameCount);
  }
}

SoundManager::SoundManager() {}
SoundManager::~SoundManager() { shutdown(); }

SoundManager *SoundManager::getInstance()
{
  static SoundManager instance;
  return &instance;
}

bool SoundManager::init()
{
  LOG_INFO("Initializing miniaudio device...");

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format   = ma_format_s16; // 16-bit signed PCM
  config.playback.channels = AUDIO_CHANNELS;
  config.sampleRate        = SAMPLE_RATE;
  config.dataCallback      = ma_audio_callback;
  config.pUserData         = this;

  if (ma_device_init(nullptr, &config, &_device) != MA_SUCCESS)
  {
    LOG_ERROR("Failed to initialize miniaudio playback device.");
    return false;
  }
  _deviceInitialized = true;

  // Initialize synthesizers
  Pixtone::getInstance()->init();
  Organya::getInstance()->init();
  OggMusic::getInstance()->init();

  _reloadTrackList();
  _detectSoundtracks();
  _applySfxForSoundtrack(settings->new_music);

  if (ma_device_start(&_device) != MA_SUCCESS)
  {
    LOG_ERROR("Failed to start miniaudio device playback.");
    return false;
  }

  LOG_INFO("Sound system online at {} Hz (Stereo 16-bit)", SAMPLE_RATE);
  return true;
}

void SoundManager::shutdown()
{
  if (_deviceInitialized)
  {
    ma_device_uninit(&_device);
    _deviceInitialized = false;
  }
  OggMusic::getInstance()->shutdown();
  Organya::getInstance()->shutdown();
  Pixtone::getInstance()->shutdown();
}

void SoundManager::audioCallback(void *pOutput, const void *pInput, ma_uint32 frameCount)
{
  (void)pInput;
  int16_t *out = static_cast<int16_t *>(pOutput);
  std::memset(out, 0, frameCount * AUDIO_CHANNELS * sizeof(int16_t));

  if (settings->music_enabled)
  {
    if (OggMusic::getInstance()->isPlaying())
    {
      OggMusic::getInstance()->renderAudio(out, frameCount);
    }
    else if (Organya::getInstance()->isPlaying())
    {
      Organya::getInstance()->renderAudio(out, frameCount);
    }
  }

  if (settings->sound_enabled)
  {
    Pixtone::getInstance()->mixActiveChannels(out, frameCount);
  }
}

void SoundManager::playSfx(SFX snd, int32_t loop)
{
  if (!settings->sound_enabled) return;
  Pixtone::getInstance()->play((int32_t)snd, loop);
}

void SoundManager::playSfxResampled(SFX snd, uint32_t percent)
{
  if (!settings->sound_enabled) return;
  Pixtone::getInstance()->playResampled((int32_t)snd, percent);
}

void SoundManager::stopSfx(SFX snd)
{
  Pixtone::getInstance()->stop((int32_t)snd);
}

void SoundManager::startStreamSound(int32_t freq)
{
  playSfxResampled(SFX::SND_STREAM1, freq);
  playSfxResampled(SFX::SND_STREAM2, freq + 100);
}

void SoundManager::startPropSound()
{
  playSfx(SFX::SND_PROPELLOR, -1);
}

void SoundManager::stopLoopSfx()
{
  stopSfx(SFX::SND_STREAM1);
  stopSfx(SFX::SND_STREAM2);
  stopSfx(SFX::SND_PROPELLOR);
}

static const char *s_song_names[] = {
  "", "wanpaku", "anzen", "gameover", "gravity", "weed", "mdown2", "fireeye",
  "vivi", "mura", "fanfale1", "ginsuke", "cemetery", "plant", "kodou",
  "fanfale3", "fanfale2", "dr", "escape", "jenka", "maze", "access",
  "ironh", "grand", "curly", "oside", "requiem", "wanpak2", "quiet",
  "lastcave", "balcony", "lastbtl", "lastbt3", "ending", "zonbie", "bdown",
  "hell", "jenka2", "marine", "ballos", "toroko", "white", "kaze", "ika"
};

static inline bool is_song_looping(int songno)
{
  return (songno != 3 && songno != 10 && songno != 15 && songno != 16);
}

void SoundManager::_detectSoundtracks()
{
  _soundtracks.clear();
  _music_dir_names.clear();

  // 1. Original Organya
  _soundtracks.push_back({"organya", "Original", "org/", true, false});
  _music_dir_names.push_back("Original");

  struct Candidate {
    const char *id;
    const char *name;
    const char *dir;
    const char *testFile;
  } candidates[] = {
    {"remastered", "Remastered",  "ogg11/",     "access_intro.ogg"},
    {"new",        "Cave Story+", "ogg/",       "access.ogg"},
    {"famitracks", "Famitracks",  "ogg17/",     "access.ogg"},
    {"ridiculon",  "Ridiculon",   "ogg_ridic/", "access.ogg"}
  };

  for (const auto &c : candidates)
  {
    std::string testPath = ResourceManager::getInstance()->getOstPath(std::string(c.dir) + c.testFile);
    if (ResourceManager::fileExists(testPath))
    {
      _soundtracks.push_back({c.id, c.name, c.dir, false, true});
      _music_dir_names.push_back(c.name);
    }
    else
    {
      std::string basePath = ResourceManager::getInstance()->getOstPath(std::string("base/") + c.dir + c.testFile);
      if (ResourceManager::fileExists(basePath))
      {
        _soundtracks.push_back({c.id, c.name, std::string("base/") + c.dir, false, true});
        _music_dir_names.push_back(c.name);
      }
    }
  }

  // Detect Pixtone WAV directory
  _csPlusPixDir.clear();
  if (ResourceManager::fileExists(ResourceManager::getInstance()->getOstPath("pixtone/pix1.wav")))
    _csPlusPixDir = "pixtone/";
  else if (ResourceManager::fileExists(ResourceManager::getInstance()->getOstPath("base/pixtone/pix1.wav")))
    _csPlusPixDir = "base/pixtone/";

  if (settings->new_music >= _soundtracks.size())
    settings->new_music = 0;
}

void SoundManager::_applySfxForSoundtrack(size_t stIndex)
{
  if (stIndex < _soundtracks.size() && _soundtracks[stIndex].is_csplus && !_csPlusPixDir.empty())
    Pixtone::getInstance()->loadCsPlusSfx(_csPlusPixDir);
  else
    Pixtone::getInstance()->restoreDefaultSfx();
}

void SoundManager::music(uint32_t songno, bool resume)
{
  if (songno == _currentSong) return;
  _lastSong = _currentSong;
  _currentSong = songno;

  if (songno != 0 && !_shouldMusicPlay(songno, settings->music_enabled))
  {
    _lastSongPos = Organya::getInstance()->stop();
    OggMusic::getInstance()->stop();
    return;
  }

  _start_track(songno, resume);
}

void SoundManager::_start_track(int songno, bool resume)
{
  if (songno <= 0 || songno >= (int)_music_names.size())
  {
    _lastSongPos = Organya::getInstance()->stop();
    OggMusic::getInstance()->stop();
    return;
  }

  if (settings->new_music >= _soundtracks.size())
    settings->new_music = 0;

  const auto &st = _soundtracks.at(settings->new_music);
  std::string songName = _music_names[songno];
  if (songName.empty()) return;

  if (st.is_organya)
  {
    OggMusic::getInstance()->stop();
    _lastSongPos = Organya::getInstance()->stop();

    std::string songPath = ResourceManager::getInstance()->getOstPath(st.dir + songName + ".org");
    if (Organya::getInstance()->load(songPath))
    {
      Organya::getInstance()->start(resume ? _lastSongPos : 0);
    }
  }
  else
  {
    Organya::getInstance()->stop();
    OggMusic::getInstance()->stop();

    bool loop = is_song_looping(songno);

    std::string introPath = ResourceManager::getInstance()->getOstPath(st.dir + songName + "_intro.ogg");
    std::string loopPath  = ResourceManager::getInstance()->getOstPath(st.dir + songName + "_loop.ogg");

    if (ResourceManager::fileExists(introPath) && ResourceManager::fileExists(loopPath))
    {
      if (OggMusic::getInstance()->load(introPath, loopPath, loop))
      {
        OggMusic::getInstance()->play(resume);
        return;
      }
    }

    std::string singlePath = ResourceManager::getInstance()->getOstPath(st.dir + songName + ".ogg");
    if (ResourceManager::fileExists(singlePath))
    {
      if (OggMusic::getInstance()->load(singlePath, loop))
      {
        OggMusic::getInstance()->play(resume);
        return;
      }
    }

    // Fallback to original Organya
    std::string fallbackPath = ResourceManager::getInstance()->getOstPath("org/" + songName + ".org");
    if (Organya::getInstance()->load(fallbackPath))
    {
      Organya::getInstance()->start(resume ? _lastSongPos : 0);
    }
  }
}

void SoundManager::enableMusic(int newstate)
{
  settings->music_enabled = newstate;
  bool play = _shouldMusicPlay(_currentSong, newstate);
  bool isPlaying = Organya::getInstance()->isPlaying() || OggMusic::getInstance()->isPlaying();
  if (play != isPlaying)
  {
    if (play) _start_track(_currentSong, false);
    else
    {
      _lastSongPos = Organya::getInstance()->stop();
      OggMusic::getInstance()->stop();
    }
  }
}

void SoundManager::setNewmusic(int newstate)
{
  if (newstate < 0 || newstate >= (int)_soundtracks.size())
    newstate = 0;

  settings->new_music = newstate;
  Organya::getInstance()->stop();
  OggMusic::getInstance()->stop();

  _applySfxForSoundtrack(newstate);
  _start_track(_currentSong, false);
}

void SoundManager::fadeMusic()
{
  Organya::getInstance()->fade();
  OggMusic::getInstance()->fade();
}

void SoundManager::runFade()
{
  Organya::getInstance()->runFade();
  OggMusic::getInstance()->runFade();
}

void SoundManager::pause()
{
  Organya::getInstance()->pause();
  OggMusic::getInstance()->pause();
}

void SoundManager::resume()
{
  Organya::getInstance()->resume();
  OggMusic::getInstance()->resume();
}

void SoundManager::updateMusicVolume() {}
void SoundManager::updateSfxVolume() {}

bool SoundManager::_shouldMusicPlay(uint32_t songno, uint32_t musicmode)
{
  if (game.mode == GM_TITLE || game.mode == GM_CREDITS) return true;
  switch (musicmode)
  {
    case 0: return false;
    case 1: return true;
    case 2: return _musicIsBoss(songno);
  }
  return false;
}

bool SoundManager::_musicIsBoss(uint32_t songno)
{
  return (std::strchr(_bossmusic, songno) != nullptr);
}

void SoundManager::_reloadTrackList()
{
  _music_names.clear();
  for (size_t i = 0; i < sizeof(s_song_names) / sizeof(s_song_names[0]); i++)
  {
    _music_names.push_back(s_song_names[i]);
  }
}

} // namespace Sound
} // namespace NXE