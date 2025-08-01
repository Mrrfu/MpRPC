#pragma once

#include <string>
#include <atomic>
#include "lockqueue.h"

// 日志系统

enum LogLevel
{
    INFO,
    ERROR
};
class Logger
{
public:
    static Logger &GetInstance();
    ~Logger();
    void Log(LogLevel level, const std::string &msg);
    void Stop();

    Logger(const Logger &) = delete;
    Logger(const Logger &&) = delete;
    // Logger &operator=(const Logger &) = delete;

private:
    Logger();
    LockQueue<std::pair<LogLevel, std::string>> m_lockQueue;

private:
    std::string getCurrentTime();
    const char *logLevelToString(LogLevel level);
};

#define LOG_INFO(logmsgformat, ...)                     \
    do                                                  \
    {                                                   \
        Logger &logger = Logger::GetInstance();         \
        char c[1024] = {0};                             \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.Log(INFO, c);                            \
    } while (0);

#define LOG_ERR(logmsgformat, ...)                      \
    do                                                  \
    {                                                   \
        Logger &logger = Logger::GetInstance();         \
        char c[1024] = {0};                             \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.Log(ERROR, c);                           \
    } while (0)