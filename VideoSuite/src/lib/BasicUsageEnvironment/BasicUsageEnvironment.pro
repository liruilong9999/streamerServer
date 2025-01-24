QT       += core 
TEMPLATE = lib
DEFINES += BasicTaskScheduler_LIBRARY

CONFIG += c++11 staticlib

include($$PWD/../../../videosuite.pri)
SOURCES += \
	BasicHashTable.cpp \
	BasicTaskScheduler.cpp \
	BasicTaskScheduler0.cpp \
	BasicUsageEnvironment.cpp \
	BasicUsageEnvironment0.cpp \
	DelayQueue.cpp

HEADERS += \ \
    include/BasicHashTable.hh \
    include/BasicUsageEnvironment.hh \
    include/BasicUsageEnvironment0.hh \
    include/BasicUsageEnvironment_version.hh \
    include/DelayQueue.hh \
    include/HandlerSet.hh


FORMS += \


INCLUDEPATH +=  \
	$$PWD/../groupsock/include \
	$$PWD/../liveMedia/include \
	$$PWD/../UsageEnvironment/include \
	$$PWD/../BasicUsageEnvironment/include \

CONFIG(debug,debug|release) : TARGET = BasicUsageEnvironmentd
CONFIG(release,debug|release) : TARGET = BasicUsageEnvironment

target.path = $$BIN_DIST_DIR/lib

INSTALLS += target 
#DESTDIR = $$BIN_DIST_DIR


