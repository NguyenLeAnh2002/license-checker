# OUTDATED: points at src/common, src/agent, src/ui, none of which exist in
# this repository anymore (replaced by src/ui-native, a single native app
# with no Qt dependency). qmake will fail immediately on this file.
# Current build: LicenseChecker.sln via build-msbuild.bat - see BUILD_VS2022.md.
TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS = \
    src/license-detection/LicenseDetection.pro \
    src/common/Common.pro \
    src/agent/Agent.pro \
    src/ui/UI.pro

# Build dependencies
src_common_Common_pro.depends = src_license_detection_LicenseDetection_pro
src_agent_Agent_pro.depends = src_license_detection_LicenseDetection_pro src_common_Common_pro
src_ui_UI_pro.depends = src_license_detection_LicenseDetection_pro src_common_Common_pro
