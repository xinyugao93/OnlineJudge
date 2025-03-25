#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHttpMultiPart>
#include "loginwindow.h"
#include <QApplication>
#include <QDateTimeEdit>
#include <QJsonArray>
#include <QInputDialog>
#include <QFormLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new QNetworkAccessManager(this))
    , currentUserId(-1)
{
    // 先隐藏主窗口
    hide();
    
    // 显示登录窗口
    showLoginWindow();
}

MainWindow::~MainWindow()
{
}

// 添加用户管理对话框类
class UserManageDialog : public QDialog
{
public:
    UserManageDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("用户管理");
        setMinimumWidth(800);
        setMinimumHeight(600);
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        // 工具栏
        QHBoxLayout *toolbarLayout = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("添加用户", this);
        QPushButton *refreshBtn = new QPushButton("刷新", this);
        toolbarLayout->addWidget(addBtn);
        toolbarLayout->addWidget(refreshBtn);
        toolbarLayout->addStretch();
        
        // 用户列表
        userTable = new QTableWidget(this);
        userTable->setColumnCount(6);
        userTable->setHorizontalHeaderLabels(QStringList() 
            << "ID" << "用户名" << "角色" << "创建时间" << "状态" << "操作");
        
        layout->addLayout(toolbarLayout);
        layout->addWidget(userTable);
        
        // 连接信号
        connect(addBtn, &QPushButton::clicked, this, &UserManageDialog::addUser);
        connect(refreshBtn, &QPushButton::clicked, this, &UserManageDialog::refreshUserList);
        
        // 初始加载用户列表
        refreshUserList();
    }

private slots:
    void addUser()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("添加用户");
        
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        
        QFormLayout *form = new QFormLayout();
        QLineEdit *usernameEdit = new QLineEdit(&dialog);
        QLineEdit *passwordEdit = new QLineEdit(&dialog);
        QComboBox *roleCombo = new QComboBox(&dialog);
        
        passwordEdit->setEchoMode(QLineEdit::Password);
        roleCombo->addItems(QStringList() << "student" << "teacher");
        
        form->addRow("用户名:", usernameEdit);
        form->addRow("密码:", passwordEdit);
        form->addRow("角色:", roleCombo);
        
        QDialogButtonBox *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, &dialog);
            
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
        layout->addLayout(form);
        layout->addWidget(buttons);
        
        if (dialog.exec() == QDialog::Accepted) {
            QJsonObject data;
            data["username"] = usernameEdit->text();
            data["password"] = passwordEdit->text();
            data["role"] = roleCombo->currentText();
            
            sendUserRequest("add", data);
        }
    }
    
    void editUser(int userId, const QString &username)
    {
        QDialog dialog(this);
        dialog.setWindowTitle("编辑用户");
        
        QVBoxLayout *layout = new QVBoxLayout(&dialog);
        QFormLayout *form = new QFormLayout();
        
        QLineEdit *passwordEdit = new QLineEdit(&dialog);
        QComboBox *roleCombo = new QComboBox(&dialog);
        QComboBox *statusCombo = new QComboBox(&dialog);
        
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setPlaceholderText("不修改请留空");
        roleCombo->addItems(QStringList() << "student" << "teacher");
        statusCombo->addItems(QStringList() << "active" << "disabled");
        
        form->addRow("用户名:", new QLabel(username, &dialog));
        form->addRow("新密码:", passwordEdit);
        form->addRow("角色:", roleCombo);
        form->addRow("状态:", statusCombo);
        
        QDialogButtonBox *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, &dialog);
            
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        
        layout->addLayout(form);
        layout->addWidget(buttons);
        
        if (dialog.exec() == QDialog::Accepted) {
            QJsonObject data;
            data["userId"] = userId;
            if (!passwordEdit->text().isEmpty()) {
                data["password"] = passwordEdit->text();
            }
            data["role"] = roleCombo->currentText();
            data["status"] = statusCombo->currentText();
            
            sendUserRequest("edit", data);
        }
    }
    
    void deleteUser(int userId)
    {
        if (QMessageBox::question(this, "确认删除", 
            "确定要删除该用户吗？此操作不可恢复。") == QMessageBox::Yes) {
            QJsonObject data;
            data["userId"] = userId;
            sendUserRequest("delete", data);
        }
    }
    
    void refreshUserList()
    {
        sendUserRequest("list", QJsonObject());
    }
    
    void handleUserResponse(const QJsonObject &response)
    {
        if (response["success"].toBool()) {
            if (response.contains("users")) {
                updateUserTable(response["users"].toArray());
            } else {
                refreshUserList();
            }
        } else {
            QMessageBox::warning(this, "操作失败", 
                response["message"].toString("未知错误"));
        }
    }
    
    void updateUserTable(const QJsonArray &users)
    {
        userTable->setRowCount(users.size());
        
        for (int i = 0; i < users.size(); ++i) {
            QJsonObject user = users[i].toObject();
            
            userTable->setItem(i, 0, new QTableWidgetItem(QString::number(user["id"].toInt())));
            userTable->setItem(i, 1, new QTableWidgetItem(user["username"].toString()));
            userTable->setItem(i, 2, new QTableWidgetItem(user["role"].toString()));
            userTable->setItem(i, 3, new QTableWidgetItem(user["created_at"].toString()));
            userTable->setItem(i, 4, new QTableWidgetItem(user["status"].toString()));
            
            // 操作按钮
            QWidget *btnWidget = new QWidget(userTable);
            QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
            btnLayout->setContentsMargins(2, 2, 2, 2);
            
            QPushButton *editBtn = new QPushButton("编辑", btnWidget);
            QPushButton *deleteBtn = new QPushButton("删除", btnWidget);
            
            btnLayout->addWidget(editBtn);
            btnLayout->addWidget(deleteBtn);
            
            userTable->setCellWidget(i, 5, btnWidget);
            
            // 连接按钮事件
            int userId = user["id"].toInt();
            QString username = user["username"].toString();
            
            connect(editBtn, &QPushButton::clicked, [=]() {
                editUser(userId, username);
            });
            
            connect(deleteBtn, &QPushButton::clicked, [=]() {
                deleteUser(userId);
            });
        }
        
        userTable->resizeColumnsToContents();
    }
    
    void sendUserRequest(const QString &action, const QJsonObject &data)
    {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QUrl url(QString("http://localhost:8080/api/users/%1").arg(action));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonDocument doc(data);
        QByteArray jsonData = doc.toJson();
        
        QNetworkReply *reply = manager->post(request, jsonData);
        
        connect(reply, &QNetworkReply::finished, [=]() {
            reply->deleteLater();
            manager->deleteLater();
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
                if (!response.isNull() && response.isObject()) {
                    handleUserResponse(response.object());
                }
            } else {
                QMessageBox::critical(this, "错误", 
                    QString("网络错误：%1").arg(reply->errorString()));
            }
        });
    }
    
private:
    QTableWidget *userTable;
};

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // 顶部工具栏
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    courseSelector = new QComboBox(this);
    courseSelector->addItem("C++程序设计");
    courseSelector->addItem("数据结构");
    courseSelector->addItem("算法设计");
    courseSelector->hide();  // 默认隐藏，等待角色确定后再显示
    
    // 发布作业按钮
    publishBtn = new QPushButton("发布作业", this);
    publishBtn->hide();  // 默认隐藏
    
    // 作业列表
    homeworkList = new QTableWidget(this);
    homeworkList->setColumnCount(5);
    homeworkList->setHorizontalHeaderLabels(QStringList() 
        << "作业名称" << "发布教师" << "截止日期" << "状态" << "操作");
    
    toolbarLayout->addWidget(courseSelector);
    toolbarLayout->addWidget(publishBtn);  // 添加发布作业按钮
    toolbarLayout->addStretch();
    
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(homeworkList);
    
    setCentralWidget(centralWidget);
    resize(1024, 768);
    setWindowTitle("在线作业批改系统");
}

void MainWindow::initConnections()
{
    connect(publishBtn, &QPushButton::clicked, this, &MainWindow::onPublishHomework);
    connect(courseSelector, &QComboBox::currentTextChanged, this, &MainWindow::onSelectCourse);
}

void MainWindow::onSubmitHomework()
{
    // TODO: 实现作业提交逻辑
    QMessageBox::information(this, "提示", "作业提交成功！");
}

void MainWindow::onViewGrade()
{
    // TODO: 实现查看成绩逻辑
}

void MainWindow::onSelectCourse()
{
    // TODO: 实现课程选择逻辑
}

void MainWindow::sendRequest(const QString &endpoint, const QJsonObject &data)
{
    QUrl url(QString("http://localhost:8080/api/%1").arg(endpoint));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(data);
    QByteArray jsonData = doc.toJson();
    
    QNetworkReply *reply = networkManager->post(request, jsonData);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void MainWindow::handleNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();  // 确保reply对象被正确释放

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::critical(this, "错误", 
            QString("网络请求失败: %1").arg(reply->errorString()));
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    if (doc.isNull()) {
        QMessageBox::warning(this, "警告", "服务器返回的数据格式无效");
        return;
    }

    QJsonObject response = doc.object();
    
    // 检查响应状态
    if (!response["success"].toBool()) {
        QString errorMessage = response["message"].toString("未知错误");
        QMessageBox::warning(this, "操作失败", errorMessage);
        return;
    }

    // 根据请求的不同endpoint处理不同的响应数据
    QString endpoint = reply->url().path();
    if (endpoint.endsWith("/submit")) {
        QMessageBox::information(this, "成功", "作业提交成功！");
        refreshHomeworkList();  // 刷新作业列表
    } else if (endpoint.endsWith("/publish")) {
        QMessageBox::information(this, "成功", "作业发布成功！");
        refreshHomeworkList();  // 刷新作业列表
    } else if (endpoint.endsWith("/homeworks")) {
        updateHomeworkList(response["homeworks"].toArray());
    }
}

void MainWindow::showLoginWindow()
{
    LoginWindow *loginWindow = new LoginWindow(this);
    connect(loginWindow, &LoginWindow::loginSuccess, [this](int userId, const QString &role) {
        onLoginSuccess(userId, role);
    });
    
    if (loginWindow->exec() != QDialog::Accepted) {
        loginWindow->deleteLater();
        QApplication::quit();
        return;
    }
    
    loginWindow->deleteLater();
}

void MainWindow::onLoginSuccess(int userId, const QString &role)
{
    currentUserId = userId;  // 确保正确保存用户ID
    currentRole = role;
    currentUsername = LoginWindow::getUsername();
    
    setupUI();
    initConnections();
    
    // 根据服务器返回的角色设置界面和标题
    QString roleText;
    if (role == "teacher") {
        setupTeacherUI();
        roleText = QString("教师端 - %1").arg(currentUsername);
    } else if (role == "student") {
        setupStudentUI();
        roleText = QString("学生端 - %1").arg(currentUsername);
    } else if (role == "admin") {
        setupAdminUI();
        roleText = QString("管理员端 - %1").arg(currentUsername);
    } else {
        QMessageBox::critical(this, "错误", "未知的用户角色：" + role);
        close();
        QApplication::quit();
        return;
    }
    
    setWindowTitle(QString("在线作业批改系统 - %1").arg(roleText));
    show();
    
    // 登录成功后立即刷新作业列表
    refreshHomeworkList();
}

void MainWindow::setupTeacherUI()
{
    // 教师特有的界面元素
    courseSelector->show();
    publishBtn->show();    // 显示发布作业按钮
}

void MainWindow::setupStudentUI()
{
    courseSelector->show();
    publishBtn->hide();    // 隐藏发布作业按钮
}

void MainWindow::setupAdminUI()
{
    // 管理员特有的界面元素
    publishBtn->hide();    // 隐藏发布作业按钮
    courseSelector->hide();
    
    // 添加管理员工具栏
    QHBoxLayout *adminToolbar = new QHBoxLayout();
    QPushButton *userManageBtn = new QPushButton("用户管理", this);
    QPushButton *systemSettingsBtn = new QPushButton("系统设置", this);
    
    adminToolbar->addWidget(userManageBtn);
    adminToolbar->addWidget(systemSettingsBtn);
    adminToolbar->addStretch();
    
    // 将管理员工具栏添加到主布局
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(centralWidget()->layout());
    if (mainLayout) {
        mainLayout->insertLayout(0, adminToolbar);
    }
    
    connect(userManageBtn, &QPushButton::clicked, [this]() {
        UserManageDialog dialog(this);
        dialog.exec();
    });
}

// 发布作业对话框
class PublishHomeworkDialog : public QDialog
{
public:
    PublishHomeworkDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("发布作业");
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        // 作业标题
        QHBoxLayout *titleLayout = new QHBoxLayout();
        QLabel *titleLabel = new QLabel("作业标题:", this);
        titleEdit = new QLineEdit(this);
        titleLayout->addWidget(titleLabel);
        titleLayout->addWidget(titleEdit);
        
        // 作业描述
        QLabel *descLabel = new QLabel("作业描述:", this);
        descEdit = new QTextEdit(this);
        
        // 截止日期
        QHBoxLayout *deadlineLayout = new QHBoxLayout();
        QLabel *deadlineLabel = new QLabel("截止日期:", this);
        deadlineEdit = new QDateTimeEdit(this);
        deadlineEdit->setDateTime(QDateTime::currentDateTime().addDays(7));
        deadlineEdit->setCalendarPopup(true);
        deadlineLayout->addWidget(deadlineLabel);
        deadlineLayout->addWidget(deadlineEdit);
        
        // 按钮
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("确定", this);
        QPushButton *cancelBtn = new QPushButton("取消", this);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        
        layout->addLayout(titleLayout);
        layout->addWidget(descLabel);
        layout->addWidget(descEdit);
        layout->addLayout(deadlineLayout);
        layout->addLayout(btnLayout);
        
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        
        setMinimumWidth(400);
    }
    
    QString getTitle() const { return titleEdit->text(); }
    QString getDescription() const { return descEdit->toPlainText(); }
    QDateTime getDeadline() const { return deadlineEdit->dateTime(); }
    
private:
    QLineEdit *titleEdit;
    QTextEdit *descEdit;
    QDateTimeEdit *deadlineEdit;
};

void MainWindow::onPublishHomework()
{
    PublishHomeworkDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    
    QJsonObject data;
    data["title"] = dialog.getTitle();
    data["description"] = dialog.getDescription();
    data["deadline"] = dialog.getDeadline().toString(Qt::ISODate);
    data["courseId"] = courseSelector->currentData().toInt();
    data["teacherId"] = currentUserId;
    data["teacherName"] = currentUsername;  // 添加教师用户名
    
    sendRequest("publish", data);
}

void MainWindow::refreshHomeworkList()
{
    QJsonObject data;
    data["courseId"] = courseSelector->currentData().toInt();
    sendRequest("homeworks", data);
}

// 修改作业详情对话框类
class HomeworkDetailDialog : public QDialog
{
public:
    HomeworkDetailDialog(const QJsonObject &homework, QWidget *parent = nullptr) 
        : QDialog(parent)
    {
        setWindowTitle("作业详情");
        
        QVBoxLayout *layout = new QVBoxLayout(this);
        
        // 创建选项卡控件
        QTabWidget *tabWidget = new QTabWidget(this);
        
        // 作业信息选项卡
        QWidget *infoTab = new QWidget(this);
        QVBoxLayout *infoLayout = new QVBoxLayout(infoTab);
        
        // 作业标题
        QLabel *titleLabel = new QLabel(QString("作业标题：%1").arg(homework["title"].toString()), this);
        titleLabel->setStyleSheet("font-weight: bold;");
        
        // 发布教师
        QLabel *teacherLabel = new QLabel(QString("发布教师：%1").arg(homework["teacherName"].toString()), this);
        
        // 截止日期
        QLabel *deadlineLabel = new QLabel(QString("截止日期：%1").arg(homework["deadline"].toString()), this);
        
        // 作业描述
        QLabel *descLabel = new QLabel("作业描述：", this);
        QTextEdit *descEdit = new QTextEdit(this);
        descEdit->setText(homework["description"].toString());
        descEdit->setReadOnly(true);
        
        infoLayout->addWidget(titleLabel);
        infoLayout->addWidget(teacherLabel);
        infoLayout->addWidget(deadlineLabel);
        infoLayout->addWidget(descLabel);
        infoLayout->addWidget(descEdit);
        
        // 提交记录选项卡
        QWidget *submissionsTab = new QWidget(this);
        QVBoxLayout *submissionsLayout = new QVBoxLayout(submissionsTab);
        
        // 创建提交记录表格
        QTableWidget *submissionsTable = new QTableWidget(this);
        submissionsTable->setColumnCount(5);
        submissionsTable->setHorizontalHeaderLabels(QStringList() 
            << "学生" << "提交时间" << "状态" << "分数" << "操作");
        
        // 直接从作业对象中获取提交记录
        QJsonArray submissions = homework["submissions"].toArray();
        submissionsTable->setRowCount(submissions.size());
        
        for (int i = 0; i < submissions.size(); ++i) {
            QJsonObject submission = submissions[i].toObject();
            
            submissionsTable->setItem(i, 0, 
                new QTableWidgetItem(submission["studentName"].toString()));
            submissionsTable->setItem(i, 1, 
                new QTableWidgetItem(submission["submitTime"].toString()));
            submissionsTable->setItem(i, 2, 
                new QTableWidgetItem(submission["status"].toString()));
            
            // 添加分数显示
            QString score = submission["score"].isNull() ? "未评分" : 
                QString::number(submission["score"].toInt());
            submissionsTable->setItem(i, 3, new QTableWidgetItem(score));
            
            // 创建操作按钮布局
            QWidget *btnWidget = new QWidget(this);
            QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
            btnLayout->setContentsMargins(2, 2, 2, 2);
            
            QPushButton *viewBtn = new QPushButton("查看答案", this);
            QPushButton *gradeBtn = new QPushButton("评分", this);
            
            btnLayout->addWidget(viewBtn);
            btnLayout->addWidget(gradeBtn);
            
            submissionsTable->setCellWidget(i, 4, btnWidget);
            
            // 查看答案按钮点击事件
            connect(viewBtn, &QPushButton::clicked, [=]() {
                QDialog *answerDialog = new QDialog(this);
                answerDialog->setWindowTitle("学生答案");
                QVBoxLayout *answerLayout = new QVBoxLayout(answerDialog);
                
                QTextEdit *answerEdit = new QTextEdit(answerDialog);
                answerEdit->setText(submission["answer"].toString());
                answerEdit->setReadOnly(true);
                
                answerLayout->addWidget(answerEdit);
                
                QPushButton *closeBtn = new QPushButton("关闭", answerDialog);
                connect(closeBtn, &QPushButton::clicked, answerDialog, &QDialog::accept);
                answerLayout->addWidget(closeBtn);
                
                answerDialog->setMinimumSize(500, 400);
                answerDialog->exec();
                answerDialog->deleteLater();
            });
            
            // 评分按钮点击事件
            connect(gradeBtn, &QPushButton::clicked, [=]() {
                bool ok;
                int score = QInputDialog::getInt(this, "评分",
                    "请输入分数 (0-100):", 
                    submission["score"].toInt(0),  // 默认值
                    0, 100, 1, &ok);
                
                if (ok) {
                    // 发送评分请求到服务器
                    QNetworkAccessManager *gradeManager = new QNetworkAccessManager(this);
                    QUrl gradeUrl("http://localhost:8080/api/grade");
                    QNetworkRequest gradeRequest(gradeUrl);
                    gradeRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                    
                    QJsonObject gradeData;
                    gradeData["submissionId"] = submission["id"].toInt();
                    gradeData["score"] = score;
                    
                    QJsonDocument doc(gradeData);
                    QByteArray jsonData = doc.toJson();
                    
                    QNetworkReply *gradeReply = gradeManager->post(gradeRequest, jsonData);
                    connect(gradeReply, &QNetworkReply::finished, [=]() {
                        gradeReply->deleteLater();
                        gradeManager->deleteLater();
                        
                        if (gradeReply->error() == QNetworkReply::NoError) {
                            QJsonDocument response = QJsonDocument::fromJson(gradeReply->readAll());
                            if (!response.isNull() && response.isObject()) {
                                QJsonObject obj = response.object();
                                if (obj["success"].toBool()) {
                                    // 更新分数显示
                                    submissionsTable->item(i, 3)->setText(QString::number(score));
                                    QMessageBox::information(this, "成功", "评分已保存");
                                } else {
                                    QMessageBox::warning(this, "失败", 
                                        obj["message"].toString("评分保存失败"));
                                }
                            }
                        } else {
                            QMessageBox::critical(this, "错误", 
                                QString("网络错误：%1").arg(gradeReply->errorString()));
                        }
                    });
                }
            });
        }
        
        submissionsTable->resizeColumnsToContents();
        submissionsLayout->addWidget(submissionsTable);
        
        // 添加选项卡
        tabWidget->addTab(infoTab, "作业信息");
        tabWidget->addTab(submissionsTab, "提交记录");
        
        layout->addWidget(tabWidget);
        
        // 确定按钮
        QPushButton *okBtn = new QPushButton("确定", this);
        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(okBtn);
        
        setMinimumWidth(600);
        setMinimumHeight(500);
    }
};

void MainWindow::updateHomeworkList(const QJsonArray &homeworks)
{
    homeworkList->setRowCount(0);
    
    for (int i = 0; i < homeworks.size(); ++i) {
        QJsonObject homework = homeworks[i].toObject();
        
        homeworkList->insertRow(i);
        homeworkList->setItem(i, 0, new QTableWidgetItem(homework["title"].toString()));
        homeworkList->setItem(i, 1, new QTableWidgetItem(homework["teacherName"].toString()));
        homeworkList->setItem(i, 2, new QTableWidgetItem(homework["deadline"].toString()));
        
        // 检查提交状态
        QString status = "未提交";
        if (homework.contains("submissions")) {
            if ("teacher" == currentRole) {
                status = "已提交";
            }
            else 
            {
                QJsonArray submissions = homework["submissions"].toArray();
                for (const QJsonValue &subVal : submissions) {
                    QJsonObject submission = subVal.toObject();
                    if (submission["studentId"].toInt() == currentUserId) {
                        status = "已提交";
                        if (!submission["score"].isNull()) {
                            status = QString("已批改 (%1分)").arg(submission["score"].toInt());
                        }
                        break;
                    }
                }
            }
        }
        homeworkList->setItem(i, 3, new QTableWidgetItem(status));
        
        // 添加操作按钮
        QPushButton *actionBtn = new QPushButton(
            currentRole == "student" ? 
            (status == "未提交" ? "提交" : "重新提交") : 
            "查看", 
            this
        );
        homeworkList->setCellWidget(i, 4, actionBtn);
        
        // 连接按钮点击事件
        connect(actionBtn, &QPushButton::clicked, [this, homework]() {
            if (currentRole == "student") {
                showSubmitDialog(homework);
            } else {
                HomeworkDetailDialog dialog(homework, this);
                dialog.exec();
            }
        });
    }
    
    homeworkList->resizeColumnsToContents();
}

// 添加提交作业对话框
void MainWindow::showSubmitDialog(const QJsonObject &homework)
{
    QDialog *dialog = new QDialog(this);  // 改为在堆上创建对话框
    dialog->setWindowTitle("提交作业");
    dialog->setMinimumWidth(600);
    dialog->setMinimumHeight(400);
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    
    // 作业信息
    QLabel *titleLabel = new QLabel(QString("作业：%1").arg(homework["title"].toString()), dialog);
    QLabel *teacherLabel = new QLabel(QString("教师：%1").arg(homework["teacherName"].toString()), dialog);
    QLabel *deadlineLabel = new QLabel(QString("截止日期：%1").arg(homework["deadline"].toString()), dialog);
    
    // 答题区域
    QLabel *answerLabel = new QLabel("答案：", dialog);
    QTextEdit *answerEdit = new QTextEdit(dialog);
    answerEdit->setPlaceholderText("在此输入你的答案...");
    
    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *submitBtn = new QPushButton("提交", dialog);
    QPushButton *cancelBtn = new QPushButton("取消", dialog);
    
    btnLayout->addWidget(submitBtn);
    btnLayout->addWidget(cancelBtn);
    
    layout->addWidget(titleLabel);
    layout->addWidget(teacherLabel);
    layout->addWidget(deadlineLabel);
    layout->addWidget(answerLabel);
    layout->addWidget(answerEdit);
    layout->addLayout(btnLayout);
    
    // 修改 lambda 表达式中的引用捕获
    connect(submitBtn, &QPushButton::clicked, [=]() {  // 改为值捕获
        QString answer = answerEdit->toPlainText().trimmed();
        if (answer.isEmpty()) {
            QMessageBox::warning(dialog, "警告", "请输入答案");
            return;
        }
        
        // 准备提交数据
        QJsonObject data;
        data["homeworkId"] = homework["id"].toInt();
        data["studentId"] = currentUserId;
        data["studentName"] = currentUsername;
        data["answer"] = answer;
        
        // 发送提交请求
        QNetworkAccessManager *manager = new QNetworkAccessManager(dialog);
        QUrl url("http://localhost:8080/api/submit");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        
        QJsonDocument doc(data);
        QByteArray jsonData = doc.toJson();
        
        QNetworkReply *reply = manager->post(request, jsonData);
        
        connect(reply, &QNetworkReply::finished, [=]() {  // 改为值捕获
            reply->deleteLater();
            manager->deleteLater();
            
            if (reply->error() == QNetworkReply::NoError) {
                QJsonDocument response = QJsonDocument::fromJson(reply->readAll());
                if (!response.isNull() && response.isObject()) {
                    QJsonObject obj = response.object();
                    if (obj["success"].toBool()) {
                        QMessageBox::information(dialog, "成功", "作业提交成功！");
                        dialog->accept();
                        refreshHomeworkList();
                    } else {
                        QMessageBox::warning(dialog, "失败", 
                            obj["message"].toString("提交失败"));
                    }
                }
            } else {
                QMessageBox::critical(dialog, "错误", 
                    QString("网络错误：%1").arg(reply->errorString()));
            }
        });
    });
    
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);  // 添加这行
    
    dialog->exec();
}

// ... 其他实现代码 ...
