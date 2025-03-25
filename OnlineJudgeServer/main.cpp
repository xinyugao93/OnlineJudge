#include <QCoreApplication>
#include "server.h"
#include "logger.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    // 初始化日志系统
    Logger::getInstance()->setLogLevel(Logger::Info);
    LOG_INFO("服务器程序启动");
    
    Server server;
    if (!server.start()) {
        LOG_FATAL("服务器启动失败！");
        return -1;
    }
    
    return a.exec();
} 