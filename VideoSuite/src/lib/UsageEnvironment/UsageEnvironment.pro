#QT       += core
TEMPLATE = lib
DEFINES += UsageEnvironment_LIBRARY

CONFIG += c++11 staticlib

include($$PWD/../../../videosuite.pri)
SOURCES += \
	HashTable.cpp \
	UsageEnvironment.cpp \
	strDup.cpp

HEADERS += \ \
    include/Boolean.hh \
    include/HashTable.hh \
    include/UsageEnvironment.hh \
    include/UsageEnvironment_version.hh \
    include/strDup.hh


FORMS += \


INCLUDEPATH +=  \
	$$PWD/../groupsock/include \
	$$PWD/../liveMedia/include \
	$$PWD/../UsageEnvironment/include \
	$$PWD/../BasicUsageEnvironment/include \

CONFIG(debug,debug|release) : TARGET = UsageEnvironmentd
CONFIG(release,debug|release) : TARGET = UsageEnvironment

target.path = $$BIN_DIST_DIR/lib

INSTALLS += target 
#DESTDIR = $$BIN_DIST_DIR


