#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QCloseEvent>

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    static QString getUsername() { return username; }
    
signals:
    void loginSuccess(int userId, const QString &role);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onLoginButtonClicked();
    void handleNetworkReply(QNetworkReply *reply);

private:
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QPushButton *loginButton;
    QLabel *statusLabel;
    QNetworkAccessManager *networkManager;
    static QString username;

    void setupUI();
    void initConnections();
};

#endif // LOGINWINDOW_H 