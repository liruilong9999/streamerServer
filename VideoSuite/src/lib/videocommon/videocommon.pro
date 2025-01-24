QT += core gui widgets concurrent network

TEMPLATE = lib
DEFINES += VIDEOCOMMON_LIBRARY
include($$PWD/../../../videosuite.pri)
CONFIG += c++11

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    httpfileserver.cpp \
    playerthread.cpp \
    playerwindow.cpp \
    seekh264videofileservermediasubsession.cpp \
    seekrtspserver.cpp \
    videocapturemanager.cpp \
    videocommon.cpp

HEADERS += \
    circularqueue.h \
    httpfileserver.h \
    playerthread.h \
    playerwindow.h \
    seekh264videofileservermediasubsession.h \
    seekrtspserver.h \
    videocapturemanager.h \
    videocommon_global.h \
    videocommon.h

CONFIG(release, debug|release){
#live555依赖
    LIBS += -L$$BIN_DIST_DIR/lib/ -lUsageEnvironment
    LIBS += -L$$BIN_DIST_DIR/lib/ -lliveMedia
    LIBS += -L$$BIN_DIST_DIR/lib/ -lgroupsock
    LIBS += -L$$BIN_DIST_DIR/lib/ -lBasicUsageEnvironment
}else
{
    LIBS += -L$$BIN_DIST_DIR/lib/ -lUsageEnvironmentd
    LIBS += -L$$BIN_DIST_DIR/lib/ -lliveMediad
    LIBS += -L$$BIN_DIST_DIR/lib/ -lgroupsockd
    LIBS += -L$$BIN_DIST_DIR/lib/ -lBasicUsageEnvironmentd
}

#ffmpeg依赖
    LIBS += -L$$BIN_DIST_DIR/lib/ -lavcodec
    LIBS += -L$$BIN_DIST_DIR/lib/ -lavdevice
    LIBS += -L$$BIN_DIST_DIR/lib/ -lavfilter
    LIBS += -L$$BIN_DIST_DIR/lib/ -lavformat
    LIBS += -L$$BIN_DIST_DIR/lib/ -lavutil
    LIBS += -L$$BIN_DIST_DIR/lib/ -lpostproc
    LIBS += -L$$BIN_DIST_DIR/lib/ -lswresample
    LIBS += -L$$BIN_DIST_DIR/lib/ -lswscale
    LIBS += -L$$BIN_DIST_DIR/lib/ -lWS2_32

INCLUDEPATH += $$BIN_DIST_DIR/include

INCLUDEPATH +=  \
        $$PWD/../groupsock/include \
        $$PWD/../liveMedia/include \
        $$PWD/../UsageEnvironment/include \
        $$PWD/../BasicUsageEnvironment/include \

CONFIG(debug,debug|release) : TARGET = videocommond
CONFIG(release,debug|release) : TARGET = videocommon

headers.files = $$HEADERS
headers.path = $$BIN_DIST_DIR/include/videocommon

target.path = $$BIN_DIST_DIR/lib

INSTALLS += target headers
FORMS += \
    playerwindow.ui

win32-msvc*{
    QMAKE_CXXFLAGS += /source-charset:utf-8 /execution-charset:utf-8
}

DLLDESTDIR = $$BIN_DIST_DIR
