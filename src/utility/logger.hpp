#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>

namespace csat
{

// Enumerates available log levels.
enum class LogLevel : std::uint8_t
{
    DEBUG   = 10,
    INFO    = 20,
    WARNING = 30,
    ERROR   = 40,
    SILENT  = UINT8_MAX
};

// Global logging level
#ifdef ENABLE_DEBUG_LOGGING
static LogLevel const CompileLogLevel = LogLevel::DEBUG;
#else
static LogLevel const CompileLogLevel = LogLevel::INFO;
#endif

// Basic logging to std::cout.
inline void LOG_OUT()
{
    std::cout << std::endl;
}

// Basic logging to std::cout.
template<class T, class... Args>
inline void LOG_OUT(T const& t, Args const&... args)
{
    std::cout << t;
    LOG_OUT(args...);
}

// Basic logging to std::cerr.
inline void LOG_ERR()
{
    std::cerr << std::endl;
}

// Basic logging to std::cerr.
template<class T, class... Args>
inline void LOG_ERR(T const& t, Args const&... args)
{
    std::cerr << t;
    LOG_ERR(args...);
}

class Logger
{
  public:
    std::string name;

    Logger() noexcept
        : name("Logger"){};
    explicit Logger(std::string const& _name) noexcept
        : name(_name)
    {
    }
    explicit Logger(std::string&& _name) noexcept
        : name(std::move(_name))
    {
    }

    ~Logger() = default;

    template<class T, class... Args>
    void debug(T const& t, Args const&... args) const
    {
        if constexpr (LogLevel::DEBUG >= CompileLogLevel)
        {
            LOG_OUT("<", name, ">(", _getCurrentTime(), ") DEBUG: ", t, args...);
        }
    }

    template<class T, class... Args>
    void info(T const& t, Args const&... args) const
    {
        if constexpr (LogLevel::INFO >= CompileLogLevel)
        {
            LOG_OUT("<", name, ">(", _getCurrentTime(), ") INFO: ", t, args...);
        }
    }

    template<class T, class... Args>
    void warning(T const& t, Args const&... args) const
    {
        if constexpr (LogLevel::WARNING >= CompileLogLevel)
        {
            LOG_OUT("<", name, ">(", _getCurrentTime(), ") WARNING: ", t, args...);
        }
    }

    template<class T, class... Args>
    void error(T const& t, Args const&... args) const
    {
        if constexpr (LogLevel::ERROR >= CompileLogLevel)
        {
            LOG_OUT("<", name, ">(", _getCurrentTime(), ") ERROR: ", t, args...);
        }
    }

  private:
    static std::string _getCurrentTime()
    {
        auto cur_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // asctime result is always 25 char, with last char '\n'
        return std::string(std::asctime(std::localtime(&cur_time))).substr(0, 24);
    }
};

}  // namespace csat
