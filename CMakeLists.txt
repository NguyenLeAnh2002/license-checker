# OUTDATED: requires Qt5 and points at src/common, src/agent, src/ui, none
# of which exist in this repository anymore (replaced by src/ui-native, a
# single native app with no Qt dependency). This will fail immediately.
# Current build: LicenseChecker.sln via build-msbuild.bat - see BUILD_VS2022.md.
cmake_minimum_required(VERSION 3.24)

project(LicenseChecker)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler settings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 COMPONENTS
    Core
    Gui
    Widgets
    Network
    REQUIRED
)

# Add subdirectories
add_subdirectory(src/common)
add_subdirectory(src/license-detection)
add_subdirectory(src/agent)
add_subdirectory(src/ui)
