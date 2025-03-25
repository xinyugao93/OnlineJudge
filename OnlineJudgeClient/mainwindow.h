#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSubmitHomework();
    void onViewGrade();
    void onSelectCourse();
    void handleNetworkReply(QNetworkReply *reply);
    void onLoginSuccess(int userId, const QString &role);
    void onPublishHomework();

private:
    QTableWidget *homeworkList;
    QComboBox *courseSelector;
    QPushButton *publishBtn;
    QNetworkAccessManager *networkManager;
    int currentUserId;
    QString currentRole;
    QString currentUsername;
    
    void setupUI();
    void setupTeacherUI();
    void setupStudentUI();
    void setupAdminUI();
    void initConnections();
    void refreshHomeworkList();
    void sendRequest(const QString &endpoint, const QJsonObject &data);
    void showLoginWindow();
    void updateHomeworkList(const QJsonArray &homeworks);
    void showSubmitDialog(const QJsonObject &homework);
};
#endif // MAINWINDOW_H
