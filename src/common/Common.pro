TEMPLATE = lib
CONFIG += staticlib c++17
QT -= gui

TARGET = common

SOURCES += \
    NamedPipeServer.cpp \
    NamedPipeClient.cpp \
    SharedLicenseDataWriter.cpp

HEADERS += \
    NamedPipeServer.h \
    NamedPipeClient.h \
    SharedLicenseDataWriter.h

win32 {
    LIBS += -ladvapi32 -lkernel32 -lws2_32
}

# Include paths for detection library
INCLUDEPATH += $$PWD \
               $$PWD/../license-detection

DEPENDPATH += $$PWD/../license-detection

# Link to license detection (order matters!)
unix|win32-g++ {
    LIBS += -L$$OUT_PWD/../license-detection -llicense_detection
}
win32-msvc {
    LIBS += $$OUT_PWD/../license-detection/license_detection.lib
}
