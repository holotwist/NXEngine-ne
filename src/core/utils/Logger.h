#ifndef _LOGGER_H
#define _LOGGER_H

#include <string>
#include <sstream>
#include <iostream>

namespace NXE
{
namespace Utils
{
namespace Logger
{

enum Level
{
  LEVEL_TRACE = 0,
  LEVEL_DEBUG,
  LEVEL_INFO,
  LEVEL_WARN,
  LEVEL_ERROR,
  LEVEL_CRITICAL
};

void init(const std::string &filename);
void log_write(Level level, const char *file, int line, const std::string &msg);

inline void format_to_stream(std::ostringstream &oss, const char *fmt)
{
  oss << fmt;
}

template <typename T, typename... Args>
void format_to_stream(std::ostringstream &oss, const char *fmt, const T &first, const Args &...rest)
{
  while (*fmt)
  {
    if (*fmt == '{')
    {
      const char *end = fmt + 1;
      while (*end && *end != '}') end++;
      if (*end == '}')
      {
        oss << first;
        format_to_stream(oss, end + 1, rest...);
        return;
      }
    }
    oss << *fmt++;
  }
}

template <typename... Args>
std::string format(const char *fmt, const Args &...args)
{
  std::ostringstream oss;
  format_to_stream(oss, fmt, args...);
  return oss.str();
}

template <typename... Args>
std::string format(const std::string &fmt, const Args &...args)
{
  return format(fmt.c_str(), args...);
}

} // namespace Logger
} // namespace Utils
} // namespace NXE

#define LOG_TRACE(...)    NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_TRACE,    __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))
#define LOG_DEBUG(...)    NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_DEBUG,    __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))
#define LOG_INFO(...)     NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_INFO,     __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))
#define LOG_WARN(...)     NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_WARN,     __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))
#define LOG_ERROR(...)    NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_ERROR,    __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))
#define LOG_CRITICAL(...) NXE::Utils::Logger::log_write(NXE::Utils::Logger::LEVEL_CRITICAL, __FILE__, __LINE__, NXE::Utils::Logger::format(__VA_ARGS__))

#endif