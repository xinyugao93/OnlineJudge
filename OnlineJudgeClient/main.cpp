#include <QApplication>
#include "loginwindow.h"
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 创建并显示登录窗口
    // LoginWindow loginWindow;
    
    // // 如果登录窗口被关闭或取消，直接退出程序
    // if (loginWindow.exec() != QDialog::Accepted) {
    //     return 0;  // 直接退出程序
    // }
    
    // 只有在登录成功时才创建并显示主窗口
    MainWindow w;
    // 不需要调用 w.show()，因为在登录成功后会自动显示
    
    return a.exec();
}
