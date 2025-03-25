#include "loginwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QMessageBox>
#include <QCloseEvent>

QString LoginWindow::username;

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
    , networkManager(new QNetworkAccessManager(this))
{
    setupUI();
    initConnections();
    setWindowTitle("登录");
}

void LoginWindow::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout();
    
    usernameEdit = new QLineEdit(this);
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    
    formLayout->addRow("用户名:", usernameEdit);
    formLayout->addRow("密码:", passwordEdit);
    
    // 创建登录按钮
    loginButton = new QPushButton("登录", this);
    
    // 状态标签
    statusLabel = new QLabel(this);
    statusLabel->setStyleSheet("color: red;");
    
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(loginButton);
    mainLayout->addWidget(statusLabel);
    
    setFixedSize(300, 200);
}

void LoginWindow::initConnections()
{
    connect(loginButton, &QPushButton::clicked, this, &LoginWindow::onLoginButtonClicked);
    
    // 允许按回车键登录
    connect(usernameEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);
    connect(passwordEdit, &QLineEdit::returnPressed, loginButton, &QPushButton::click);
}

void LoginWindow::onLoginButtonClicked()
{
    username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        statusLabel->setText("用户名和密码不能为空！");
        return;
    }
    
    // 禁用登录按钮防止重复提交
    loginButton->setEnabled(false);
    statusLabel->setText("正在登录...");
    
    // 准备登录请求
    QUrl url("http://localhost:8080/api/login");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 构建请求数据
    QJsonObject jsonObj;
    jsonObj["username"] = username;
    jsonObj["password"] = password;
    
    QJsonDocument doc(jsonObj);
    QByteArray jsonData = doc.toJson();
    
    // 发送登录请求
    QNetworkReply *reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void LoginWindow::handleNetworkReply(QNetworkReply *reply)
{
    loginButton->setEnabled(true);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (reply->error() == QNetworkReply::InternalServerError) {
            statusLabel->setText("用户名或密码错误");
        } else {
            statusLabel->setText("网络错误：" + reply->errorString());
        }
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    if (doc.isNull()) {
        statusLabel->setText("服务器返回的数据格式无效");
        return;
    }

    QJsonObject response = doc.object();
    
    if (response["success"].toBool()) {
        // 登录成功，获取用户ID和角色
        int userId = response["userId"].toInt();
        QString role = response["role"].toString();
        
        // 发送登录成功信号，包含用户ID和角色信息
        emit loginSuccess(userId, role);
        accept();
    } else {
        // 登录失败
        QString errorMessage = response["error"].toString();
        statusLabel->setText(errorMessage);
        passwordEdit->clear();
        passwordEdit->setFocus();
    }
}

void LoginWindow::closeEvent(QCloseEvent *event)
{
    // 当用户点击关闭按钮时，设置对话框结果为Rejected
    reject();
    event->accept();
} 
