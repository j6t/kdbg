cmake_minimum_required(VERSION 3.5)
project(KDbg)
set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.2.0")

set(KDBG_VERSION 3.1.0)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kdbg/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdbg/version.h)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    Config
    IconThemes
    XmlGui
    WindowSystem
)

include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(FeatureSummary)

add_subdirectory(kdbg)
add_subdirectory(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
