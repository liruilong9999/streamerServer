#QT       += core
TEMPLATE = lib
DEFINES += groupsock_LIBRARY

CONFIG += c++11 staticlib

include($$PWD/../../../videosuite.pri)
SOURCES += \
	GroupEId.cpp \
	Groupsock.cpp \
	GroupsockHelper.cpp \
	IOHandlers.cpp \
	NetAddress.cpp \
	NetInterface.cpp \
	groupsock.cpp \
	inet.c

HEADERS += \ \
    include/GroupEId.hh \
    include/Groupsock.hh \
    include/GroupsockHelper.hh \
    include/IOHandlers.hh \
    include/NetAddress.hh \
    include/NetCommon.h \
    include/NetInterface.hh \
    include/TunnelEncaps.hh \
    include/groupsock_version.hh


FORMS += \


INCLUDEPATH +=  \
	$$PWD/../groupsock/include \
	$$PWD/../liveMedia/include \
	$$PWD/../UsageEnvironment/include \
	$$PWD/../BasicUsageEnvironment/include \

CONFIG(debug,debug|release) : TARGET = groupsockd
CONFIG(release,debug|release) : TARGET = groupsock

target.path = $$BIN_DIST_DIR/lib

INSTALLS += target 
#DESTDIR = $$BIN_DIST_DIR


