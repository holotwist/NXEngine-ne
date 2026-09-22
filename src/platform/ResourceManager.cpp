#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#if defined(__unix__) || defined(__APPLE__) || defined(__VITA__) || defined(__SWITCH__)
#include <sys/stat.h>
#elif defined(__HAIKU__)
#include <posix/sys/stat.h> // ugh
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

#if defined(__VITA__)
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#endif

#include "ResourceManager.h"
#include "glob.h"
#include "misc.h"
#include "settings.h"

#include <json.hpp>

bool ResourceManager::fileExists(const std::string &filename)
{
#if defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__) || defined(__VITA__) || defined(__SWITCH__)
  struct stat st;

  if (stat(filename.c_str(), &st) == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
#elif defined(_WIN32) || defined(WIN32) // Windows
  DWORD attrs = GetFileAttributes(widen(filename).c_str());

  // Assume path exists
  if (attrs != INVALID_FILE_ATTRIBUTES)
  {
    return true;
  }
  else
  {
    return false;
  }
#else
#error Platform not supported
#endif
  return false;
}

#include <raylib.h>

std::string ResourceManager::getBasePath()
{
  const char *dir = GetApplicationDirectory();
  if (dir && dir[0] != '\0')
    return std::string(dir);
  return std::string("./");
}

std::string ResourceManager::getUserPrefPath()
{
#if defined(PLATFORM_ANDROID)
  const char *internalPath = GetApplicationDirectory();
  if (internalPath) return std::string(internalPath) + "/";
  return std::string("./");
#else
  const char *home = getenv("XDG_DATA_HOME");
  if (home && home[0] != '\0')
  {
    std::string pref = std::string(home) + "/nxengine-ne/";
    mkdir(pref.c_str(), 0755);
    return pref;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0')
  {
    std::string pref = std::string(home) + "/.local/share/nxengine-ne/";
    mkdir((std::string(home) + "/.local").c_str(), 0755);
    mkdir((std::string(home) + "/.local/share").c_str(), 0755);
    mkdir(pref.c_str(), 0755);
    return pref;
  }
  return std::string("./");
#endif
}

ResourceManager::ResourceManager()
{
  findLanguages();
  findMods();
}

ResourceManager::~ResourceManager() {}

ResourceManager *ResourceManager::getInstance()
{
  return Singleton<ResourceManager>::get();
}

void ResourceManager::shutdown() {}

std::string ResourceManager::getPath(const std::string &filename, bool localized)
{
  std::vector<std::string> _paths;

  std::string userResBase = getUserPrefPath();

  if (!userResBase.empty())
  {
    if (!_mod.empty())
    {
      if (localized) _paths.push_back(userResBase + "data/mods/" + _mod + "/lang/" + std::string(settings->language) + "/" + filename);
      _paths.push_back(userResBase + "data/mods/" + _mod + "/" + filename);
    }
    if (localized) _paths.push_back(userResBase + "data/lang/" + std::string(settings->language) + "/" + filename);
    _paths.push_back(userResBase + "data/" + filename);
  }

  #if defined(DATADIR)
    std::string _data(DATADIR);
  #else
    std::string _data = getBasePath();

    #if defined(HAVE_UNIX_LIKE) and !defined(PORTABLE)
      _data += "../share/nxengine/data/";
    #else
      _data += "data/";
    #endif
  #endif

  if (!_mod.empty())
  {
    if (localized) _paths.push_back(_data + "mods/" + _mod + "/lang/" + std::string(settings->language) + "/" + filename);
    _paths.push_back(_data + "mods/" + _mod + "/" + filename);
  }

  std::string _base = getBasePath();
  if (localized) {
    _paths.push_back(_base + "resources/lang/" + std::string(settings->language) + "/" + filename);
    _paths.push_back(_data + "lang/" + std::string(settings->language) + "/" + filename);
  }
  _paths.push_back(_base + "resources/" + filename);
  _paths.push_back(_data + filename);

  for (auto &_tryPath: _paths)
  {
    if (fileExists(_tryPath))
      return _tryPath;
  }

  return _paths.back();
}

std::string ResourceManager::getPrefPath(const std::string &filename)
{
  return std::string(getUserPrefPath()) + std::string(filename);
}

std::string ResourceManager::getPathForDir(const std::string &dir)
{
  return getPath(dir, false);
}

inline std::vector<std::string> glob(const std::string &pat)
{
  Glob search(pat);
  std::vector<std::string> ret;
  while (search)
  {
    ret.push_back(search.GetFileName());
    search.Next();
  }
  return ret;
}

void ResourceManager::findLanguages()
{
  _languages.clear();
  _languageInfos.clear();

  auto registerLang = [this](const std::string &id, const std::string &folder) {
    if (std::find(_languages.begin(), _languages.end(), id) != _languages.end()) return;

    LanguageInfo info;
    info.id = id;
    info.name = id;

    std::ifstream mf(widen(folder + "/meta.json"), std::ios::binary);
    if (mf.is_open()) {
      nlohmann::json meta = nlohmann::json::parse(mf, nullptr, false);
      if (!meta.is_discarded()) {
        info.name = meta.value("name", id);
        info.author = meta.value("author", "");
        info.rtl = meta.value("rtl", false);
      }
    }

    _languages.push_back(id);
    _languageInfos.push_back(info);
  };

  // Check both resources/lang and data/lang
  std::vector<std::string> searchDirs = {
    getBasePath() + "resources/lang/",
    getPathForDir("lang/")
  };

  for (const auto &sdir : searchDirs) {
    std::vector<std::string> found = glob(sdir + "*");
    for (auto &l : found) {
      if (fileExists(l + "/meta.json") || fileExists(l + "/system.json")) {
        std::string id = l.substr(l.find_last_of('/') + 1);
        registerLang(id, l);
      }
    }
  }

  // Ensure default english fallback exists
  if (std::find(_languages.begin(), _languages.end(), "english") == _languages.end()) {
    _languages.insert(_languages.begin(), "english");
    _languageInfos.insert(_languageInfos.begin(), {"english", "English", "Studio Pixel / Aeon Genesis", false});
  }
}

std::string ResourceManager::getLanguageDisplayName(const std::string &id)
{
  for (const auto &info : _languageInfos) {
    if (info.id == id) return info.name;
  }
  return id;
}

std::vector<std::string> &ResourceManager::languages()
{
  return _languages;
}

void ResourceManager::findMods()
{
  std::vector<std::string> mods=glob(getPathForDir("mods/")+"*");
  for (auto &l: mods)
  {
//    std::cout << l << std::endl;
    std::ifstream ifs(widen(l+"/mod.json"), std::ifstream::in | std::ifstream::binary);
    if (ifs.is_open())
    {
      nlohmann::json modfile = nlohmann::json::parse(ifs);
      _mods.insert(
        {
          l.substr(l.find_last_of('/')+1),
          Mod{l.substr(l.find_last_of('/')+1), modfile["name"].get<std::string>(), modfile["skip-intro"].get<bool>()}
        }
      );

      ifs.close();
    }
  }
}

Mod& ResourceManager::mod(std::string& name)
{
  return _mods[name];
}

void ResourceManager::setMod(std::string name)
{
  _mod = name;
}

Mod& ResourceManager::mod()
{
  return _mods[_mod];
}

bool ResourceManager::isMod()
{
  return !_mod.empty();
}

std::map<std::string, Mod> &ResourceManager::mods()
{
  return _mods;
}
