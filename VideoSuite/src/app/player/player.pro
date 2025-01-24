QT += core gui widgets

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = player
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        main.cpp

include($$PWD/../../../videosuite.pri)

INCLUDEPATH += $$BIN_DIST_DIR/include
INCLUDEPATH += $$DEPENCE_DIR


CONFIG(release, debug|release){
        LIBS += -L$$BIN_DIST_DIR/lib/ -lvideocommon
    }else
    {
        LIBS += -L$$BIN_DIST_DIR/lib/ -lvideocommond
    }

target.path = $$BIN_DIST_DIR

INSTALLS += target

