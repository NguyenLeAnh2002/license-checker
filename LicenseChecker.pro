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
