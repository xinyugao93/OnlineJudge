QT -= gui
QT += network sql

CONFIG += c++11 console
CONFIG -= app_bundle

# 添加 Qt 头文件路径
INCLUDEPATH += $$[QT_INSTALL_HEADERS]
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCore
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtNetwork
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtSql

SOURCES += \
    main.cpp \
    server.cpp \
    logger.cpp

HEADERS += \
    server.h \
    logger.h

target.path = /usr/local/bin
!isEmpty(target.path): INSTALLS += target 