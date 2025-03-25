#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server();

    bool start(quint16 port = 8080);
    bool initDatabase();

private slots:
    void handleNewConnection();
    void handleReadyRead();
    void handleDisconnected();

private:
    QTcpServer *tcpServer;
    QMap<QTcpSocket*, QByteArray> buffers;
    QString dbFilePath;
    QString homeworkDbPath;  // 新增

    // API处理函数
    void handleSubmission(QTcpSocket *socket, const QJsonObject &data);
    void handleHomeworkList(QTcpSocket *socket, const QJsonObject &data);
    void handleLogin(QTcpSocket *socket, const QJsonObject &data);
    void handlePublishHomework(QTcpSocket *socket, const QJsonObject &data);
    void handleGrade(QTcpSocket *socket, const QJsonObject &data);  // 新增评分处理函数
    void handleUserList(QTcpSocket *socket, const QJsonObject &data);
    void handleUserAdd(QTcpSocket *socket, const QJsonObject &data);
    void handleUserEdit(QTcpSocket *socket, const QJsonObject &data);
    void handleUserDelete(QTcpSocket *socket, const QJsonObject &data);

    // HTTP请求处理
    void processRequest(QTcpSocket *socket, const QJsonObject &request, const QString &path);
    void sendHttpResponse(QTcpSocket *socket, const QJsonObject &response);
    void sendHttpError(QTcpSocket *socket, int statusCode, const QString &message);
    QString getStatusText(int statusCode);

    // 辅助函数
    bool verifyUser(const QString &username, const QString &password);

    // JSON文件操作
    QJsonObject loadDatabase();
    bool saveDatabase(const QJsonObject &data);
    bool initTestUsers();

    // 作业数据库操作
    bool initHomeworkDatabase();
    QJsonObject loadHomeworkDatabase();
    bool saveHomeworkDatabase(const QJsonObject &data);
};

#endif // SERVER_H
