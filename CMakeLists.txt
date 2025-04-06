cmake_minimum_required(VERSION 3.16)
project(KDbg)
set(QT_MIN_VERSION "5.15.0")
set(KF_MIN_VERSION "5.89.0")

set(KDBG_VERSION 3.1.0)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kdbg/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdbg/version.h)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Widgets
)
if(QT_MAJOR_VERSION EQUAL 6)
    find_package(Qt${QT_MAJOR_VERSION}Core5Compat CONFIG REQUIRED)
endif()

find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS
    I18n
    Config
    CoreAddons
    IconThemes
    XmlGui
    WindowSystem
)

add_subdirectory(kdbg)
add_subdirectory(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
