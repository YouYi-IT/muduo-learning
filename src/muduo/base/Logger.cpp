#include <muduo/base/Logger.h>
#include <muduo/base/Timestamp.h>

#include <iostream>

namespace muduo::base
{

// 获取日志唯一实例对象
Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int level)
{
    log_level_ = level;
}

// 写日志
void Logger::log(std::string msg)
{
    switch (log_level_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    std::cout << Timestamp().now().toString() << ":" << msg << std::endl;
}

}  // namespace muduo::base