#include "server.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "logger.h"
#include <QFile>


Server::Server(QObject *parent)
    : QObject(parent)
    , tcpServer(new QTcpServer(this))
{
    dbFilePath = "users.json";
    homeworkDbPath = "homeworks.json";  // 新增作业数据文件路径
    initDatabase();
    initHomeworkDatabase();  // 新增作业数据库初始化
}

Server::~Server()
{
}

bool Server::start(quint16 port)
{
    if (!tcpServer->listen(QHostAddress::Any, port)) {
        LOG_ERROR(QString("服务器启动失败：%1").arg(tcpServer->errorString()));
        return false;
    }
    
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::handleNewConnection);
    LOG_INFO(QString("服务器启动成功，监听端口：%1").arg(port));
    return true;
}

bool Server::initDatabase()
{
    QFile file(dbFilePath);
    
    // 如果文件不存在，创建一个新的
    if (!file.exists()) {
        LOG_INFO("用户数据文件不存在，创建新文件");
        QJsonObject initialData;
        initialData["users"] = QJsonArray();
        
        if (!saveDatabase(initialData)) {
            LOG_ERROR("创建用户数据文件失败");
            return false;
        }
        
        // 添加测试用户
        return initTestUsers();
    }
    
    return true;
}

QJsonObject Server::loadDatabase()
{
    QFile file(dbFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("无法打开数据文件：%1").arg(file.errorString()));
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        LOG_ERROR("数据文件格式无效");
        return QJsonObject();
    }
    
    return doc.object();
}

bool Server::saveDatabase(const QJsonObject &data)
{
    QFile file(dbFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("无法写入数据文件：%1").arg(file.errorString()));
        return false;
    }
    
    QJsonDocument doc(data);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
}

bool Server::initTestUsers()
{
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    
    // 如果已经有用户了，不需要初始化
    if (!users.isEmpty()) {
        return true;
    }
    
    // 添加测试用户
    QVector<QPair<QString, QString>> testUsers = {
        {"admin", "admin"},
        {"teacher1", "teacher"},
        {"student1", "student"},
        {"student2", "student"}
    };
    
    for (const auto &user : testUsers) {
        QJsonObject userObj;
        userObj["username"] = user.first;
        userObj["password"] = "123456";
        userObj["role"] = user.second;
        users.append(userObj);
        
        LOG_INFO(QString("添加测试用户：%1 (角色: %2)")
            .arg(user.first)
            .arg(user.second));
    }
    
    db["users"] = users;
    return saveDatabase(db);
}

void Server::handleNewConnection()
{
    QTcpSocket *socket = tcpServer->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &Server::handleReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &Server::handleDisconnected);
    LOG_INFO(QString("新客户端连接：%1").arg(socket->peerAddress().toString()));
}

void Server::handleReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    QByteArray &buffer = buffers[socket];
    buffer.append(socket->readAll());
    
    // 检查是否收到完整的 HTTP 请求
    if (!buffer.contains("\r\n\r\n")) {
        return; // 等待更多数据
    }
    
    // 解析 HTTP 请求
    QString httpRequest = QString::fromUtf8(buffer);
    QStringList requestLines = httpRequest.split("\r\n");
    
    if (requestLines.isEmpty()) {
        LOG_ERROR("收到空的 HTTP 请求");
        socket->disconnectFromHost();
        return;
    }
    
    // 解析请求行
    QStringList requestLine = requestLines[0].split(" ");
    if (requestLine.size() < 3) {
        LOG_ERROR("无效的 HTTP 请求行");
        socket->disconnectFromHost();
        return;
    }
    
    QString method = requestLine[0];
    QString path = requestLine[1];
    
    // 查找请求体
    int bodyStart = buffer.indexOf("\r\n\r\n") + 4;
    QByteArray body = buffer.mid(bodyStart);
    
    // 解析 Content-Length
    int contentLength = 0;
    for (const QString &line : requestLines) {
        if (line.startsWith("Content-Length: ")) {
            contentLength = line.mid(16).toInt();
            break;
        }
    }
    
    // 检查是否收到完整的请求体
    if (body.length() < contentLength) {
        return; // 等待更多数据
    }
    
    LOG_INFO(QString("收到 HTTP %1 请求: %2").arg(method).arg(path));
    
    // 处理请求
    if (method == "POST") {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isNull() || !doc.isObject()) {
            sendHttpError(socket, 400, "无效的 JSON 数据");
            return;
        }
        
        QJsonObject request = doc.object();
        processRequest(socket, request, path);
    } else {
        sendHttpError(socket, 405, "方法不允许");
    }
    
    // 清除已处理的数据
    buffer.clear();
}

void Server::processRequest(QTcpSocket *socket, const QJsonObject &request, const QString &path)
{
    if (path == "/api/login") {
        handleLogin(socket, request);
    }
    else if (path == "/api/submit") {
        handleSubmission(socket, request);
    }
    else if (path == "/api/publish") {
        handlePublishHomework(socket, request);
    }
    else if (path == "/api/homeworks") {
        handleHomeworkList(socket, request);
    }
    else if (path == "/api/grade") {
        handleGrade(socket, request);
    }
    else if (path == "/api/users/list") {
        handleUserList(socket, request);
    }
    else if (path == "/api/users/add") {
        handleUserAdd(socket, request);
    }
    else if (path == "/api/users/edit") {
        handleUserEdit(socket, request);
    }
    else if (path == "/api/users/delete") {
        handleUserDelete(socket, request);
    }
    else {
        sendHttpError(socket, 404, "未找到请求的资源");
    }
}

void Server::handleSubmission(QTcpSocket *socket, const QJsonObject &data)
{
    int homeworkId = data["homeworkId"].toInt();
    int studentId = data["studentId"].toInt();
    QString studentName = data["studentName"].toString();
    QString answer = data["answer"].toString();
    
    // 从作业数据库中获取作业信息
    QJsonObject homeworkDb = loadHomeworkDatabase();
    QJsonArray homeworks = homeworkDb["homeworks"].toArray();
    bool found = false;
    
    // 查找对应的作业
    for (int i = 0; i < homeworks.size(); ++i) {
        QJsonObject homework = homeworks[i].toObject();
        if (homework["id"].toInt() == homeworkId) {
            QJsonArray submissions = homework["submissions"].toArray();
            
            // 查找该学生是否已有提交记录
            int submissionIndex = -1;
            for (int j = 0; j < submissions.size(); ++j) {
                QJsonObject submission = submissions[j].toObject();
                if (submission["studentId"].toInt() == studentId) {
                    submissionIndex = j;
                    break;
                }
            }
            
            // 创建新的提交记录
            QJsonObject newSubmission;
            newSubmission["id"] = submissionIndex >= 0 ? 
                submissions[submissionIndex].toObject()["id"].toInt() : 
                submissions.size() + 1;
            newSubmission["studentId"] = studentId;
            newSubmission["studentName"] = studentName;
            newSubmission["answer"] = answer;
            newSubmission["submitTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            newSubmission["status"] = "已提交";
            newSubmission["score"] = QJsonValue::Null;
            
            // 更新或添加提交记录
            if (submissionIndex >= 0) {
                submissions.replace(submissionIndex, newSubmission);
                LOG_INFO(QString("学生 %1 更新了作业提交").arg(studentName));
            } else {
                submissions.append(newSubmission);
                LOG_INFO(QString("学生 %1 首次提交作业").arg(studentName));
            }
            
            homework["submissions"] = submissions;
            homeworks[i] = homework;
            found = true;
            break;
        }
    }
    
    if (found) {
        homeworkDb["homeworks"] = homeworks;
        if (saveHomeworkDatabase(homeworkDb)) {
            sendHttpResponse(socket, {
                {"success", true},
                {"message", "作业提交成功"}
            });
        } else {
            LOG_ERROR(QString("保存学生 %1 的提交记录失败").arg(studentName));
            sendHttpError(socket, 500, "保存提交记录失败");
        }
    } else {
        LOG_ERROR(QString("未找到作业：%1").arg(homeworkId));
        sendHttpError(socket, 404, "未找到对应的作业");
    }
}

void Server::handleHomeworkList(QTcpSocket *socket, const QJsonObject &data)
{
    QJsonObject homeworkDb = loadHomeworkDatabase();
    QJsonArray homeworks = homeworkDb["homeworks"].toArray();
    
    // 可以根据需要添加过滤条件，如课程ID
    int courseId = data["courseId"].toInt(-1);
    
    QJsonArray filteredHomeworks;
    for (const QJsonValue &val : homeworks) {
        QJsonObject homework = val.toObject();
        if (courseId == -1 || homework["courseId"].toInt() == courseId) {
            // 确保 submissions 字段存在
            if (!homework.contains("submissions")) {
                homework["submissions"] = QJsonArray();
            }
            filteredHomeworks.append(homework);
        }
    }
    
    sendHttpResponse(socket, {
        {"success", true},
        {"homeworks", filteredHomeworks}
    });
}

void Server::handleLogin(QTcpSocket *socket, const QJsonObject &data)
{
    QString username = data["username"].toString();
    QString password = data["password"].toString();
    
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    
    for (const QJsonValue &userVal : users) {
        QJsonObject user = userVal.toObject();
        if (user["username"].toString() == username) {
            if (user["password"].toString() == password) {
                if (user["status"].toString() != "disabled") {
                    LOG_INFO(QString("用户 %1 登录成功").arg(username));
                    sendHttpResponse(socket, {
                        {"success", true},
                        {"userId", user["id"].toInt()},  // 返回正确的用户ID
                        {"role", user["role"].toString()}
                    });
                    return;
                } else {
                    sendHttpError(socket, 403, "账号已被禁用");
                    return;
                }
            }
            sendHttpError(socket, 401, "密码错误");
            return;
        }
    }
    
    sendHttpError(socket, 404, "用户不存在");
}

void Server::handlePublishHomework(QTcpSocket *socket, const QJsonObject &data)
{
    // 直接使用客户端传来的教师信息
    int teacherId = data["teacherId"].toInt();
    QString teacherName = data["teacherName"].toString();
    
    // 从请求中获取作业信息
    QString title = data["title"].toString();
    QString description = data["description"].toString();
    QString deadline = data["deadline"].toString();
    int courseId = data["courseId"].toInt();
    
    // 将作业信息保存到作业数据库
    QJsonObject homeworkDb = loadHomeworkDatabase();
    QJsonArray homeworks = homeworkDb["homeworks"].toArray();
    
    QJsonObject homework;
    homework["id"] = homeworks.size() + 1;
    homework["title"] = title;
    homework["description"] = description;
    homework["deadline"] = deadline;
    homework["courseId"] = courseId;
    homework["teacherId"] = teacherId;
    homework["teacherName"] = teacherName;
    homework["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    homeworks.append(homework);
    homeworkDb["homeworks"] = homeworks;
    
    if (saveHomeworkDatabase(homeworkDb)) {
        LOG_INFO(QString("教师 %1 发布新作业：%2").arg(teacherName).arg(title));
        sendHttpResponse(socket, {
            {"success", true},
            {"message", "作业发布成功"},
            {"homework", homework}
        });
    } else {
        LOG_ERROR(QString("教师 %1 发布作业失败：%2").arg(teacherName).arg(title));
        sendHttpError(socket, 500, "保存作业信息失败");
    }
}

void Server::handleGrade(QTcpSocket *socket, const QJsonObject &data)
{
    int submissionId = data["submissionId"].toInt();
    int score = data["score"].toInt();
    
    // 从作业数据库中获取提交记录
    QJsonObject homeworkDb = loadHomeworkDatabase();
    QJsonArray homeworks = homeworkDb["homeworks"].toArray();
    bool found = false;
    
    // 遍历所有作业
    for (int i = 0; i < homeworks.size(); ++i) {
        QJsonObject homework = homeworks[i].toObject();
        QJsonArray submissions = homework["submissions"].toArray();
        
        // 遍历所有提交记录
        for (int j = 0; j < submissions.size(); ++j) {
            QJsonObject submission = submissions[j].toObject();
            if (submission["id"].toInt() == submissionId) {
                // 找到对应的提交记录，更新分数
                submission["score"] = score;
                submissions[j] = submission;
                homework["submissions"] = submissions;
                homeworks[i] = homework;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    
    if (found) {
        // 保存更新后的数据
        homeworkDb["homeworks"] = homeworks;
        if (saveHomeworkDatabase(homeworkDb)) {
            LOG_INFO(QString("提交记录 %1 评分成功：%2分").arg(submissionId).arg(score));
            sendHttpResponse(socket, {
                {"success", true},
                {"message", "评分已保存"}
            });
        } else {
            LOG_ERROR(QString("提交记录 %1 评分保存失败").arg(submissionId));
            sendHttpError(socket, 500, "评分保存失败");
        }
    } else {
        LOG_ERROR(QString("未找到提交记录：%1").arg(submissionId));
        sendHttpError(socket, 404, "未找到对应的提交记录");
    }
}

void Server::handleUserList(QTcpSocket *socket, const QJsonObject &data)
{
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    
    // 移除敏感信息（如密码）
    for (int i = 0; i < users.size(); ++i) {
        QJsonObject user = users[i].toObject();
        user.remove("password");
        users[i] = user;
    }
    
    sendHttpResponse(socket, {
        {"success", true},
        {"users", users}
    });
}

void Server::handleUserAdd(QTcpSocket *socket, const QJsonObject &data)
{
    QString username = data["username"].toString();
    QString password = data["password"].toString();
    QString role = data["role"].toString();
    
    if (username.isEmpty() || password.isEmpty() || role.isEmpty()) {
        sendHttpError(socket, 400, "缺少必要的用户信息");
        return;
    }
    
    // 检查用户名是否已存在
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    
    // 找到当前最大的用户ID
    int maxId = 0;
    for (const QJsonValue &val : users) {
        QJsonObject user = val.toObject();
        if (user["username"].toString() == username) {
            sendHttpError(socket, 400, "用户名已存在");
            return;
        }
        maxId = qMax(maxId, user["id"].toInt());
    }
    
    // 创建新用户，ID为最大ID加1
    QJsonObject newUser;
    newUser["id"] = maxId + 1;  // 确保ID唯一且递增
    newUser["username"] = username;
    newUser["password"] = password;
    newUser["role"] = role;
    newUser["status"] = "active";
    newUser["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    users.append(newUser);
    db["users"] = users;
    
    if (saveDatabase(db)) {
        LOG_INFO(QString("新用户创建成功：%1 (ID: %2)").arg(username).arg(maxId + 1));
        sendHttpResponse(socket, {
            {"success", true},
            {"message", "用户创建成功"}
        });
    } else {
        LOG_ERROR(QString("创建用户失败：%1").arg(username));
        sendHttpError(socket, 500, "保存用户信息失败");
    }
}

void Server::handleUserEdit(QTcpSocket *socket, const QJsonObject &data)
{
    int userId = data["userId"].toInt();
    
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    bool found = false;
    
    for (int i = 0; i < users.size(); ++i) {
        QJsonObject user = users[i].toObject();
        if (user["id"].toInt() == userId) {
            // 更新用户信息
            if (data.contains("password")) {
                user["password"] = data["password"].toString();
            }
            if (data.contains("role")) {
                user["role"] = data["role"].toString();
            }
            if (data.contains("status")) {
                user["status"] = data["status"].toString();
            }
            
            users[i] = user;
            found = true;
            break;
        }
    }
    
    if (found) {
        db["users"] = users;
        if (saveDatabase(db)) {
            LOG_INFO(QString("用户 %1 更新成功").arg(userId));
            sendHttpResponse(socket, {
                {"success", true},
                {"message", "用户信息更新成功"}
            });
        } else {
            LOG_ERROR(QString("更新用户 %1 失败").arg(userId));
            sendHttpError(socket, 500, "保存用户信息失败");
        }
    } else {
        LOG_ERROR(QString("未找到用户：%1").arg(userId));
        sendHttpError(socket, 404, "未找到指定用户");
    }
}

void Server::handleUserDelete(QTcpSocket *socket, const QJsonObject &data)
{
    int userId = data["userId"].toInt();
    
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    bool found = false;
    
    for (int i = 0; i < users.size(); ++i) {
        if (users[i].toObject()["id"].toInt() == userId) {
            users.removeAt(i);
            found = true;
            break;
        }
    }
    
    if (found) {
        db["users"] = users;
        if (saveDatabase(db)) {
            LOG_INFO(QString("用户 %1 删除成功").arg(userId));
            sendHttpResponse(socket, {
                {"success", true},
                {"message", "用户删除成功"}
            });
        } else {
            LOG_ERROR(QString("删除用户 %1 失败").arg(userId));
            sendHttpError(socket, 500, "删除用户失败");
        }
    } else {
        LOG_ERROR(QString("未找到用户：%1").arg(userId));
        sendHttpError(socket, 404, "未找到指定用户");
    }
}

void Server::sendHttpResponse(QTcpSocket *socket, const QJsonObject &response)
{
    QJsonDocument doc(response);
    QByteArray jsonData = doc.toJson();
    
    QByteArray httpResponse = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: " + QByteArray::number(jsonData.length()) + "\r\n"
                             "Access-Control-Allow-Origin: *\r\n"
                             "\r\n";
    httpResponse.append(jsonData);
    
    socket->write(httpResponse);
    socket->flush();
}

void Server::sendHttpError(QTcpSocket *socket, int statusCode, const QString &message)
{
    QJsonObject errorResponse;
    errorResponse["success"] = false;
    errorResponse["error"] = message;
    
    QJsonDocument doc(errorResponse);
    QByteArray jsonData = doc.toJson();
    
    QByteArray httpResponse = QString("HTTP/1.1 %1 %2\r\n"
                                    "Content-Type: application/json\r\n"
                                    "Content-Length: %3\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "\r\n")
                                    .arg(statusCode)
                                    .arg(getStatusText(statusCode))
                                    .arg(jsonData.length())
                                    .toUtf8();
    httpResponse.append(jsonData);
    
    socket->write(httpResponse);
    socket->flush();
    
    LOG_WARNING(QString("发送 HTTP 错误 %1: %2").arg(statusCode).arg(message));
}

QString Server::getStatusText(int statusCode)
{
    switch (statusCode) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

void Server::handleDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        buffers.remove(socket);
        socket->deleteLater();
        qDebug() << "客户端断开连接";
    }
}

bool Server::verifyUser(const QString &username, const QString &password)
{
    QJsonObject db = loadDatabase();
    QJsonArray users = db["users"].toArray();
    
    for (const QJsonValue &userVal : users) {
        QJsonObject user = userVal.toObject();
        if (user["username"].toString() == username) {
            return user["password"].toString() == password;
        }
    }
    
    return false;
}

bool Server::initHomeworkDatabase()
{
    QFile file(homeworkDbPath);
    if (!file.exists()) {
        LOG_INFO("作业数据文件不存在，创建新文件");
        QJsonObject initialData;
        initialData["homeworks"] = QJsonArray();
        
        if (!saveHomeworkDatabase(initialData)) {
            LOG_ERROR("创建作业数据文件失败");
            return false;
        }
    }
    return true;
}

QJsonObject Server::loadHomeworkDatabase()
{
    QFile file(homeworkDbPath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("无法打开作业数据文件：%1").arg(file.errorString()));
        return QJsonObject();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        LOG_ERROR("作业数据文件格式无效");
        return QJsonObject();
    }
    
    return doc.object();
}

bool Server::saveHomeworkDatabase(const QJsonObject &data)
{
    QFile file(homeworkDbPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(QString("无法写入作业数据文件：%1").arg(file.errorString()));
        return false;
    }
    
    QJsonDocument doc(data);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return true;
} 
