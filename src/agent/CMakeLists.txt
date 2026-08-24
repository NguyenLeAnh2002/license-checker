cmake_minimum_required(VERSION 3.24)

project(LicenseCheckerAgent)

# Agent sources
set(AGENT_SOURCES
    LicenseDetectionWorker.h
    LicenseDetectionWorker.cpp
    ServerReporter.h
    ServerReporter.cpp
    ServiceMain.cpp
    ServiceInstaller.cpp
    NotificationFormatter.h
    NotificationFormatter.cpp
)

# Create executable for the Windows Service
add_executable(LicenseCheckerAgent ${AGENT_SOURCES})

# Link libraries
target_link_libraries(LicenseCheckerAgent
    license_detection_lib
    common_lib
)

if(MSVC)
    target_link_libraries(LicenseCheckerAgent
        advapi32.lib
        kernel32.lib
        winsvc.lib
        wtsapi32.lib
        winhttp.lib
    )
endif()

# Include directories
target_include_directories(LicenseCheckerAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common
    ${CMAKE_SOURCE_DIR}/src/license-detection
)

# Compiler settings
set_target_properties(LicenseCheckerAgent PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(LicenseCheckerAgent PRIVATE /W4)
else()
    target_compile_options(LicenseCheckerAgent PRIVATE -Wall -Wextra)
endif()
