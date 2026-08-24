cmake_minimum_required(VERSION 3.24)

project(CommonLibrary)

# Common library sources
set(COMMON_SOURCES
    NamedPipeServer.h
    NamedPipeServer.cpp
    NamedPipeClient.h
    NamedPipeClient.cpp
    SharedLicenseDataWriter.h
    SharedLicenseDataWriter.cpp
)

# Create static library
add_library(common_lib STATIC ${COMMON_SOURCES})

# Link to license detection and Windows libraries
target_link_libraries(common_lib
    license_detection_lib
)

if(MSVC)
    target_link_libraries(common_lib
        advapi32.lib
        kernel32.lib
    )
endif()

# Include directories
target_include_directories(common_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/license-detection
)

# Compiler settings
set_target_properties(common_lib PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(common_lib PRIVATE /W4)
else()
    target_compile_options(common_lib PRIVATE -Wall -Wextra)
endif()
