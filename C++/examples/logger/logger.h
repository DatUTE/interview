#pragma once

#include <iostream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <string>
#include <utility>

enum class LogLevel
{
    INFO,
    WARNING,
    ERROR
};

class Logger
{
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template<typename... Args>
    void log(LogLevel level,
             const char* file,
             int line,
             Args&&... args)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
    
        std::ostringstream oss;
        oss << "[" << getCurrentTime() << "] ";
        oss << "[" << levelToString(level) << "] ";
        oss << "[" << file << ":" << line << "] ";
    
        (oss << ... << std::forward<Args>(args));
    
        std::cout << oss.str() << '\n';
    }

private:
    Logger() = default;

    std::string getCurrentTime();
    const char* levelToString(LogLevel level);

    std::mutex m_mutex;
};


#define LOG_INFO(...) \
    Logger::instance().log(LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_WARN(...) \
    Logger::instance().log(LogLevel::WARNING, __FILE__, __LINE__, __VA_ARGS__)

#define LOG_ERROR(...) \
    Logger::instance().log(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)