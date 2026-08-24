TEMPLATE = app
CONFIG += c++17 console
QT -= gui

TARGET = LicenseCheckerAgent

SOURCES += \
    ServiceMain.cpp \
    LicenseDetectionWorker.cpp \
    ServiceInstaller.cpp \
    NotificationFormatter.cpp

HEADERS += \
    LicenseDetectionWorker.h \
    NotificationFormatter.h

win32 {
    LIBS += -ladvapi32 -lkernel32 -lwsock32 -lwtsapi32
}

# Include paths (order matters!)
INCLUDEPATH += $$PWD \
               $$PWD/../license-detection \
               $$PWD/../common

DEPENDPATH += $$PWD/../license-detection \
              $$PWD/../common

# Link to libraries
unix|win32-g++ {
    LIBS += -L$$OUT_PWD/../license-detection -llicense_detection
    LIBS += -L$$OUT_PWD/../common -lcommon
}
win32-msvc {
    LIBS += $$OUT_PWD/../license-detection/license_detection.lib
    LIBS += $$OUT_PWD/../common/common.lib
}
