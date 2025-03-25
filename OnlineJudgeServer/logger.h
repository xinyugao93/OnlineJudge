#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

class Logger : public QObject
{
    Q_OBJECT
public:
    enum LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    static Logger* getInstance();
    void log(LogLevel level, const QString &message);
    void setLogFile(const QString &filePath);
    void setLogLevel(LogLevel level);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    
    static Logger* instance;
    QFile logFile;
    QTextStream logStream;
    LogLevel currentLevel;
    QMutex mutex;
    
    QString levelToString(LogLevel level);
    bool shouldLog(LogLevel level);
};

// 定义宏以方便使用
#define LOG_DEBUG(msg) Logger::getInstance()->log(Logger::Debug, msg)
#define LOG_INFO(msg) Logger::getInstance()->log(Logger::Info, msg)
#define LOG_WARNING(msg) Logger::getInstance()->log(Logger::Warning, msg)
#define LOG_ERROR(msg) Logger::getInstance()->log(Logger::Error, msg)
#define LOG_FATAL(msg) Logger::getInstance()->log(Logger::Fatal, msg)

#endif // LOGGER_H 