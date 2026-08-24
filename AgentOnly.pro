TEMPLATE = app
CONFIG += c++17 console
TARGET = LicenseCheckerAgent

QT -= gui core

# ============ Core Detection Sources ============
SOURCES += \
    src/license-detection/LicenseResult.cpp \
    src/license-detection/LicenseDetector.cpp \
    src/license-detection/SLAPIDetector.cpp \
    src/license-detection/WMIDetector.cpp \
    src/license-detection/RegistryDetector.cpp \
    src/license-detection/DetectionLogger.cpp

HEADERS += \
    src/license-detection/LicenseResult.h \
    src/license-detection/LicenseStatusEnum.h \
    src/license-detection/LicenseDetector.h \
    src/license-detection/SLAPIDetector.h \
    src/license-detection/WMIDetector.h \
    src/license-detection/RegistryDetector.h \
    src/license-detection/DetectionLogger.h

# ============ Common IPC ============
SOURCES += \
    src/common/NamedPipeServer.cpp \
    src/common/SharedLicenseDataWriter.cpp

HEADERS += \
    src/common/NamedPipeServer.h \
    src/common/SharedLicenseDataWriter.h

# ============ Agent Service ============
SOURCES += \
    src/agent/ServiceMain.cpp \
    src/agent/LicenseDetectionWorker.cpp \
    src/agent/ServiceInstaller.cpp \
    src/agent/NotificationFormatter.cpp

HEADERS += \
    src/agent/LicenseDetectionWorker.h \
    src/agent/NotificationFormatter.h

# ============ Windows Libraries ============
win32 {
    LIBS += \
        -ladvapi32 \
        -lkernel32 \
        -lwsock32 \
        -lwbemuuid \
        -lole32 \
        -loleaut32 \
        -lcrypt32 \
        -lwtsapi32
}

# ============ Include Paths ============
INCLUDEPATH += \
    src/license-detection \
    src/common \
    src/agent

# ============ Compiler Settings ============
win32-g++ {
    QMAKE_CXXFLAGS += -std=c++17
}

# ============ Output Directory ============
CONFIG(debug, debug|release) {
    DESTDIR = build/debug
    OBJECTS_DIR = build/debug/.obj
    MOC_DIR = build/debug/.moc
} else {
    DESTDIR = build/release
    OBJECTS_DIR = build/release/.obj
    MOC_DIR = build/release/.moc
}

# ============ Application Info ============
VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
