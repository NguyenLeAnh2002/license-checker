TEMPLATE = lib
CONFIG += staticlib c++17

TARGET = license_detection

SOURCES += \
    LicenseResult.cpp \
    LicenseDetector.cpp \
    SLAPIDetector.cpp \
    WMIDetector.cpp \
    RegistryDetector.cpp \
    DetectionLogger.cpp

HEADERS += \
    LicenseResult.h \
    LicenseStatusEnum.h \
    LicenseDetector.h \
    SLAPIDetector.h \
    WMIDetector.h \
    RegistryDetector.h \
    DetectionLogger.h

win32 {
    LIBS += -ladvapi32 -lkernel32 -lwbemuuid -lole32 -loleaut32 -lcrypt32
}

INCLUDEPATH += $$PWD
