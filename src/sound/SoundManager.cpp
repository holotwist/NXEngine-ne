#include "SoundManager.h"
#include "Pixtone.h"
#include "Organya.h"
#include "platform/ResourceManager.h"
#include "core/common/misc.h"
#include "core/utils/Logger.h"
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

  // Load tracklists
  std::string path = ResourceManager::getInstance()->getPath("music_dirs.json", false);
  _music_dirs.clear();
  _music_dir_names.clear();
  _music_playlists.clear();
  _music_dirs.push_back("org/");
  _music_dir_names.push_back("Original");
  _music_playlists.push_back("music.json");

  std::ifstream fl(widen(path), std::ifstream::in | std::ifstream::binary);
  if (fl.is_open())
  {
    nlohmann::json dirlist = nlohmann::json::parse(fl, nullptr, false);
    if (!dirlist.is_discarded())
    {
      for (auto it = dirlist.begin(); it != dirlist.end(); ++it)
      {
        std::string dir = it.value().at("dir");
        if (ResourceManager::getInstance()->fileExists(ResourceManager::getInstance()->getPathForDir(dir)))
        {
          auto it_playlist = it.value().find("playlist");
          _music_playlists.push_back(it_playlist != it.value().end() ? it_playlist->get<std::string>() : "music.json");
          _music_dirs.push_back(dir);
          _music_dir_names.push_back(it.value().at("name"));
        }
      }
    }
  }

  _reloadTrackList();

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
  Organya::getInstance()->shutdown();
  Pixtone::getInstance()->shutdown();
}

void SoundManager::audioCallback(void *pOutput, const void *pInput, ma_uint32 frameCount)
{
  (void)pInput;
  int16_t *out = static_cast<int16_t *>(pOutput);
  std::memset(out, 0, frameCount * AUDIO_CHANNELS * sizeof(int16_t));

  // Synthesize and mix active Organya music
  if (settings->music_enabled && Organya::getInstance()->isPlaying())
  {
    Organya::getInstance()->renderAudio(out, frameCount);
  }

  // Mix active Pixtone sound effects
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

void SoundManager::music(uint32_t songno, bool resume)
{
  if (songno == _currentSong) return;
  _lastSong = _currentSong;
  _currentSong = songno;

  if (songno != 0 && !_shouldMusicPlay(songno, settings->music_enabled))
  {
    _lastSongPos = Organya::getInstance()->stop();
    return;
  }

  _start_org_track(songno, resume);
}

void SoundManager::_start_org_track(int songno, bool resume)
{
  if (_music_names.size() < 2) return;
  _lastSongPos = Organya::getInstance()->stop();

  if (songno == 0) return;

  std::string songPath = ResourceManager::getInstance()->getPath(_music_dirs.at(0) + _music_names[songno] + ".org", false);
  if (Organya::getInstance()->load(songPath))
  {
    Organya::getInstance()->start(resume ? _lastSongPos : 0);
  }
}

void SoundManager::enableMusic(int newstate)
{
  settings->music_enabled = newstate;
  bool play = _shouldMusicPlay(_currentSong, newstate);
  if (play != Organya::getInstance()->isPlaying())
  {
    if (play) _start_org_track(_currentSong, false);
    else _lastSongPos = Organya::getInstance()->stop();
  }
}

void SoundManager::setNewmusic(int newstate)
{
  settings->new_music = newstate;
  Organya::getInstance()->stop();
  _reloadTrackList();
  _start_org_track(_currentSong, false);
}

void SoundManager::fadeMusic() { Organya::getInstance()->fade(); }
void SoundManager::runFade()  { Organya::getInstance()->runFade(); }
void SoundManager::pause()    { Organya::getInstance()->pause(); }
void SoundManager::resume()   { Organya::getInstance()->resume(); }
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
  if (_music_playlists.size() <= settings->new_music)
    settings->new_music = 0;

  std::string path = ResourceManager::getInstance()->getPath(_music_playlists.at(settings->new_music), false);
  _music_names.clear();
  _music_names.push_back("");
  _music_loop.clear();
  _music_loop.push_back(false);

  std::ifstream fl(widen(path), std::ifstream::in | std::ifstream::binary);
  if (fl.is_open())
  {
    nlohmann::json tracklist = nlohmann::json::parse(fl, nullptr, false);
    if (!tracklist.is_discarded())
    {
      for (auto it = tracklist.begin(); it != tracklist.end(); ++it)
      {
        auto it_loop = it.value().find("loop");
        _music_loop.push_back(it_loop != it.value().end() ? it_loop->get<bool>() : true);
        _music_names.push_back(it.value().at("name"));
      }
    }
  }
}

} // namespace Sound
} // namespace NXE