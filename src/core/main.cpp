
#include "nx.h"

#include <cstdarg>
#include <cstdlib>
#if !defined(_WIN32)
#include <unistd.h>
#else
#include <direct.h>
#include <io.h>
#endif
//#include "main.h"
#include "game.h"
#include "Renderer.h"
#include "input.h"
#include "map.h"
#include "profile.h"
#include "settings.h"
#include "statusbar.h"
#include "trig.h"
#include "tsc.h"

#include <raylib.h>
#include "ResourceManager.h"
#include "caret.h"
#include "misc.h"
#include "console.h"
#include "screeneffect.h"
#include "SoundManager.h"
#include "Logger.h"

using namespace NXE::Graphics;
using namespace NXE::Utils;
using namespace NXE::Sound;

static void fatal(const char *str)
{
  LOG_CRITICAL("fatal: '{}'", str);
}

static inline void run_tick()
{
  input_poll();

  if (justpushed(F9KEY))
  {
    Renderer::getInstance()->saveScreenshot();
  }

  // Render game frame to virtual target
  Renderer::getInstance()->beginFrame();
  game.tick();

  if (settings->show_fps)
  {
    char fpstext[32];
    snprintf(fpstext, sizeof(fpstext), "%d fps", GetFPS());
    int x = (Renderer::getInstance()->screenWidth - 4) - Renderer::getInstance()->font.getWidth(fpstext);
    Renderer::getInstance()->font.draw(x, 4, fpstext, 0x00FF00, true);
  }

  Renderer::getInstance()->endFrame();
  Renderer::getInstance()->flip();

  SoundManager::getInstance()->runFade();
}

void gameloop(void)
{
  game.switchstage.mapno = -1;

  while (game.running && !WindowShouldClose() && game.switchstage.mapno < 0)
  {
    run_tick();
  }

  if (WindowShouldClose())
    game.running = false;
}

void InitNewGame(bool with_intro)
{
  LOG_DEBUG("= Beginning new game =");

  memset(game.flags, 0, sizeof(game.flags));
  memset(game.skipflags, 0, sizeof(game.skipflags));
  textbox.StageSelect.ClearSlots();

  game.quaketime = game.megaquaketime = 0;
  game.showmapnametime                = 0;
  game.debug.god                      = 0;
  game.running                        = true;
  game.frozen                         = false;

  // fully re-init the player object
  Objects::DestroyAll(true);
  game.createplayer();

  player->maxHealth = 3;
  player->hp        = player->maxHealth;

  game.switchstage.mapno        = STAGE_START_POINT;
  game.switchstage.playerx      = 10;
  game.switchstage.playery      = 8;
  game.switchstage.eventonentry = (with_intro) ? 200 : 91;

  fade.set_full(FADE_OUT);
}

int main(int argc, char *argv[])
{
  bool error            = false;
  bool freshstart;

#if defined(UNIX_LIKE)
  // On platforms where SDL may use Wayland (Linux and BSD), setting the icon from a surface doesn't work and
  // the request will be ignored. Instead apps submit their app ID using the xdg-shell Wayland protocol and
  // then the desktop looks up the icon based on this.
  // SDL (as of 2.0.14) only exposes setting the app ID through the (missleadingly named) environment variable
  // used below, so we call `setenv` here and hope it picks up the value during initialization of the Wayland
  // backend. As its name implies this will also set window class on X11, but this will not cause any issues,
  // and its recomended that that matches the app ID anyways. On other platforms, the env will be silently
  // ignored.
  setenv("SDL_VIDEO_X11_WMCLASS", "org.nxengine.nxengine_evo", 0);
#endif

#if defined(__SWITCH__)
  if (romfsInit() != 0)
  {
    std::cerr << "romfsInit() failed" << std::endl;
    return 1;
  }
#endif

  (void)ResourceManager::getInstance();
  Logger::init(ResourceManager::getInstance()->getPrefPath("debug.log"));

  // Initialize controls
  input_init();

  // load settings, or at least get the defaults,
  // so we know the initial screen resolution.
  settings_load();

  if (!Renderer::getInstance()->init(settings->resolution))
  {
    fatal("Failed to initialize graphics.");
    return 1;
  }
  Renderer::getInstance()->setFullscreen(settings->fullscreen);

  //	if (check_data_exists())
  //	{
  //		return 1;
  //	}

  Renderer::getInstance()->showLoadingScreen();

  if (!SoundManager::getInstance()->init())
  {
    fatal("Failed to initialize sound.");
    return 1;
  }

  if (trig_init())
  {
    fatal("Failed trig module init.");
    return 1;
  }

  if (textbox.Init())
  {
    fatal("Failed to initialize textboxes.");
    return 1;
  }
  if (Carets::init())
  {
    fatal("Failed to initialize carets.");
    return 1;
  }

  if (game.init())
    return 1;
  if (!game.tsc->Init())
  {
    fatal("Failed to initialize script engine.");
    return 1;
  }
  game.setmode(GM_NORMAL);
  // set null stage just to have something to do while we go to intro
  game.switchstage.mapno = 0;

  char *profile_name = GetProfileName(settings->last_save_slot);
  if (settings->skip_intro && file_exists(profile_name))
    game.switchstage.mapno = LOAD_GAME;
  else
    game.setmode(GM_INTRO);

  free(profile_name);

  // for debug
  if (game.paused)
  {
    game.switchstage.mapno        = 0;
    game.switchstage.eventonentry = 0;
  }

  game.running = true;
  freshstart   = true;

  LOG_INFO("Entering main loop...");

  while (game.running)
  {
    // SSS/SPS persists across stage transitions until explicitly
    // stopped, or you die & reload. It seems a bit risky to me,
    // but that's the spec.
    if (game.switchstage.mapno >= MAPNO_SPECIALS)
    {
      NXE::Sound::SoundManager::getInstance()->stopLoopSfx();
      //			StopLoopSounds();
    }

    // enter next stage, whatever it may be
    if (game.switchstage.mapno == LOAD_GAME || game.switchstage.mapno == LOAD_GAME_FROM_MENU)
    {
      if (game.switchstage.mapno == LOAD_GAME_FROM_MENU)
        freshstart = true;

      LOG_DEBUG("= Loading game =");
      if (game_load(settings->last_save_slot))
      {
        fatal("savefile error");
        goto ingame_error;
      }
      fade.set_full(FADE_IN);
    }
    else if (game.switchstage.mapno == TITLE_SCREEN)
    {
      LOG_DEBUG("= Title screen =");
      game.curmap = TITLE_SCREEN;
    }
    else
    {
      if (game.switchstage.mapno == NEW_GAME || game.switchstage.mapno == NEW_GAME_FROM_MENU)
      {
        bool show_intro = (game.switchstage.mapno == NEW_GAME_FROM_MENU || ResourceManager::getInstance()->isMod());
        InitNewGame(show_intro);
      }

      // slide weapon bar on first intro to Start Point
      if (game.switchstage.mapno == STAGE_START_POINT && game.switchstage.eventonentry == 91)
      {
        freshstart = true;
      }

      // switch maps
      if (load_stage(game.switchstage.mapno))
        goto ingame_error;

      player->x = (game.switchstage.playerx * TILE_W) * CSFI;
      player->y = (game.switchstage.playery * TILE_H) * CSFI;
    }

    // start the level
    if (game.initlevel())
      return 1;

    if (freshstart)
      weapon_introslide();

    gameloop();
    game.stageboss.OnMapExit();
    freshstart = false;
  }

shutdown:;
  game.tsc->Close();
  game.close();
  Carets::close();

  input_close();
  textbox.Deinit();
  NXE::Sound::SoundManager::getInstance()->shutdown();
  Renderer::getInstance()->close();
  return error;

ingame_error:;
  LOG_CRITICAL("");
  LOG_CRITICAL(" ************************************************");
  LOG_CRITICAL(" * An in-game error occurred. Game shutting down.");
  LOG_CRITICAL(" ************************************************");
  error = true;
  goto shutdown;
}
