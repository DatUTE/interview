#include "logger.h"

#include <ctime>

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

std::string Logger::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &tt);
#else
    localtime_r(&tt, &local_time);
#endif

    std::stringstream ss;
    ss << std::put_time(&local_time, "%F %T");
    return ss.str();
}

const char* Logger::levelToString(LogLevel level)
{
    switch (level)
    {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
    }

    return "UNKNOWN";
}

const char* Logger::colorForLevel(LogLevel level)
{
    switch (level)
    {
        case LogLevel::INFO:    return "\033[32m";
        case LogLevel::WARNING: return "\033[33m";
        case LogLevel::ERROR:   return "\033[31m";
    }

    return "\033[0m";
}