cmake_minimum_required(VERSION 3.24)

project(LicenseDetectionLibrary)

# License detection sources
set(DETECTION_SOURCES
    LicenseStatusEnum.h
    LicenseResult.h
    LicenseResult.cpp
    LicenseDetector.h
    LicenseDetector.cpp
    SLAPIDetector.h
    SLAPIDetector.cpp
    WMIDetector.h
    WMIDetector.cpp
    RegistryDetector.h
    RegistryDetector.cpp
    DetectionLogger.h
    DetectionLogger.cpp
)

# Create static library
add_library(license_detection_lib STATIC ${DETECTION_SOURCES})

# Link Windows libraries
if(MSVC)
    target_link_libraries(license_detection_lib
        advapi32.lib
        kernel32.lib
        wbemuuid.lib
        ole32.lib
        oleaut32.lib
    )
endif()

# Include directories
target_include_directories(license_detection_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler settings
set_target_properties(license_detection_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(license_detection_lib PRIVATE /W4)
else()
    target_compile_options(license_detection_lib PRIVATE -Wall -Wextra)
endif()
