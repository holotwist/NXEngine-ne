#include "Logger.h"
#include <cstdio>
#include <ctime>
#include <mutex>

namespace NXE
{
namespace Utils
{
namespace Logger
{

static FILE *s_log_file = nullptr;
static std::mutex s_log_mutex;

void init(const std::string &filename)
{
  std::lock_guard<std::mutex> lock(s_log_mutex);
  if (s_log_file)
  {
    fclose(s_log_file);
    s_log_file = nullptr;
  }
  if (!filename.empty())
  {
    s_log_file = fopen(filename.c_str(), "w");
  }
}

void log_write(Level level, const char *file, int line, const std::string &msg)
{
  std::lock_guard<std::mutex> lock(s_log_mutex);

  static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"
  };

  const char *tag = (level >= LEVEL_TRACE && level <= LEVEL_CRITICAL) ? level_names[level] : "INFO";

  time_t now = time(nullptr);
  struct tm tm_info;
#if defined(_WIN32)
  localtime_s(&tm_info, &now);
#else
  localtime_r(&now, &tm_info);
#endif
  char time_buf[32];
  strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_info);

  const char *base_file = file;
  for (const char *p = file; *p; p++)
  {
    if (*p == '/' || *p == '\\')
      base_file = p + 1;
  }

  char header[128];
  snprintf(header, sizeof(header), "[%s] [%s] [%s:%d]: ", time_buf, tag, base_file, line);

  FILE *out = (level >= LEVEL_WARN) ? stderr : stdout;
  fprintf(out, "%s%s\n", header, msg.c_str());
  fflush(out);

  if (s_log_file)
  {
    fprintf(s_log_file, "%s%s\n", header, msg.c_str());
    fflush(s_log_file);
  }
}

} // namespace Logger
} // namespace Utils
} // namespace NXE