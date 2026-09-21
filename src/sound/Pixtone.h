/* Based on */
/* SIMPLE CAVE STORY MUSIC PLAYER (Organya) */
/* Written by Joel Yliluoma -- http://iki.fi/bisqwit/ */
/* https://bisqwit.iki.fi/jutut/kuvat/programming_examples/doukutsu-org/orgplay.cc */

#ifndef _PIXTONE_H
#define _PIXTONE_H

#include "Singleton.h"
#include "basics.h"
#include "SoundManager.h"
#include <string>
#include <vector>
#include <cstdint>

#define PXT_NO_CHANNELS 4
#define PXENV_NUM_VERTICES 3
#define MAX_PXT_VOICES 32

namespace NXE
{
namespace Sound
{

enum
{
  MOD_SINE,
  MOD_TRI,
  MOD_SAWUP,
  MOD_SAWDOWN,
  MOD_SQUARE,
  MOD_NOISE,

  PXT_NO_MODELS
};

struct stPXEnvelope
{
  int32_t initial;
  struct
  {
    int32_t time, val;
  } p[PXENV_NUM_VERTICES];
  int32_t evaluate(int32_t i) const;
};

struct stPXChannel
{
  bool enabled;
  uint32_t nsamples;
  struct stPXWave
  {
    const int8_t *wave;
    double pitch;
    int32_t level, offset;
  };

  stPXWave carrier;
  stPXWave frequency;
  stPXWave amplitude;
  stPXEnvelope envelope;
  int8_t *buffer;
  void synth();
};

struct stPXSound
{
  stPXChannel channels[PXT_NO_CHANNELS];

  int8_t *final_buffer = nullptr;
  uint32_t final_size;
  bool load(const std::string &fname);
  bool render();
  int32_t allocBuf();
  void freeBuf();
};

struct PxtVoice
{
  int slot = -1;
  float pos = 0.0f;
  float step = 1.0f;
  int loop = 0;
  bool active = false;
};

class Pixtone
{
public:
  static Pixtone *getInstance();
  bool init();
  void shutdown();

  void play(int32_t slot, int32_t loop = 0);
  void playResampled(int32_t slot, uint32_t percent);
  void stop(int32_t slot);
  void mixActiveChannels(int16_t *stream, uint32_t frameCount);

protected:
  friend class Singleton<Pixtone>;

  Pixtone();
  ~Pixtone();
  Pixtone(const Pixtone &) = delete;
  Pixtone &operator=(const Pixtone &) = delete;

private:
  void _prepareToPlay(stPXSound *snd, int32_t slot);

  bool _inited = false;
  std::vector<int16_t> _sounds[256];
  PxtVoice _voices[MAX_PXT_VOICES];
  const uint32_t NUM_SOUNDS = 0x75;
};

} // namespace Sound
} // namespace NXE
#endif
