#-------------------------------------------------
#
# Project created by QtCreator 2018-03-28T09:43:02
# 宝学平台人脸数据库服务器
#-------------------------------------------------

QT       += core gui script network sql xml concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = FaceDbServer
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

#用于公安三所检测
DEFINES += jiance

INCLUDEPATH         += $$PWD

INCLUDEPATH         += $$PWD/log
INCLUDEPATH         += $$PWD/tcp
INCLUDEPATH         += $$PWD/upgrade
INCLUDEPATH         += $$PWD/cpu
INCLUDEPATH         += $$PWD/udp

include             ($$PWD/log/log.pri)
include             ($$PWD/tcp/tcp.pri)
include             ($$PWD/upgrade/upgrade.pri)
include             ($$PWD/cpu/cpu.pri)
include             ($$PWD/udp/udp.pri)

SOURCES += main.cpp\
        mainform.cpp \
    globalconfig.cpp \

HEADERS  += mainform.h \
    globalconfig.h \

FORMS    += mainform.ui


DESTDIR=bin
MOC_DIR=temp/moc
RCC_DIR=temp/rcc
UI_DIR=temp/ui
OBJECTS_DIR=temp/obj
