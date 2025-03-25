#include "logger.h"
#include <QDir>
#include <QDebug>

Logger* Logger::instance = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent)
    , currentLevel(Debug)
{
    // 默认日志文件路径
    QString logDir = "logs";
    QDir().mkpath(logDir);
    
    QString logPath = QString("%1/server_%2.log")
        .arg(logDir)
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));
    
    setLogFile(logPath);
}

Logger::~Logger()
{
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}

Logger* Logger::getInstance()
{
    if (!instance) {
        instance = new Logger();
    }
    return instance;
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&mutex);
    
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
    
    logFile.setFileName(filePath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "无法打开日志文件:" << filePath;
        return;
    }
    
    logStream.setDevice(&logFile);
}

void Logger::setLogLevel(LogLevel level)
{
    currentLevel = level;
}

void Logger::log(LogLevel level, const QString &message)
{
    if (!shouldLog(level)) {
        return;
    }
    
    QMutexLocker locker(&mutex);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logEntry = QString("[%1] [%2] %3\n")
        .arg(timestamp)
        .arg(levelToString(level))
        .arg(message);
    
    logStream << logEntry;
    logStream.flush();
    
    // 同时输出到控制台
    if (level >= Warning) {
        qDebug().noquote() << logEntry.trimmed();
    }
}

QString Logger::levelToString(LogLevel level)
{
    switch (level) {
        case Debug: return "DEBUG";
        case Info: return "INFO";
        case Warning: return "WARNING";
        case Error: return "ERROR";
        case Fatal: return "FATAL";
        default: return "UNKNOWN";
    }
}

bool Logger::shouldLog(LogLevel level)
{
    return level >= currentLevel;
} 