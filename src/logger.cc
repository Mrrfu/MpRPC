#include "logger.h"
#include <iostream>
#include <fstream>

Logger &Logger::GetInstance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
    std::thread writeLogTask([this]()
                             {
        while(true)
        {
            try
            {
            auto log_pair = m_lockQueue.pop();
            LogLevel level = log_pair.first;
            std::string msg = log_pair.second;

            //获取当前的日期，然后取日志信息，写入相应的日志文件中（追加模式）
            time_t now = time(nullptr);
            tm *nowtm = localtime(&now);
            char file_name[128];
            sprintf(file_name,"%d-%d-%d-log.txt",nowtm->tm_year+1900,nowtm->tm_mon+1,nowtm->tm_mday);

            std::ofstream log_file(file_name,std::ios::out|std::ios::app);
            if(!log_file.is_open())
            {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                // 不建议直接 exit
                continue;
            }
          
    
            //获取日志级别字符串
            std::string level_str = logLevelToString(level);
            //获取当前时间字符串
            std::string current_time = getCurrentTime();

            msg =current_time+level_str+msg;
           
            log_file<<msg<<std::endl;

            // if(log_file.is_open()&&m_lockQueue.empty())
            // {
            //     log_file.close();
            // }
            // log_file会自动关闭，因为这是一个局部变量
            }catch(const std::exception& e)
            {
                //被通知关闭
                break;
            }
        } });

    // 设置分离线程（守护线程）
    writeLogTask.detach();
}

Logger::~Logger()
{
    Stop();
}

void Logger::Log(LogLevel level, const std::string &msg)
{
    m_lockQueue.push({level, msg});
}

std::string Logger::getCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char buf[100];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    return buf;
}

const char *Logger::logLevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::INFO:
        return " [INFO] ";
    case LogLevel::ERROR:
        return " [ERROR] ";
    default:
        return " [UNKNOWN] ";
    }
}

void Logger::Stop()
{
    m_lockQueue.shutdown();
}