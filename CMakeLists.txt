cmake_minimum_required(VERSION 3.16)
project(KDbg)
set(BUILD_FOR_KDE_VERSION 5 CACHE STRING "Target this version of the KDE Frameworks (5 or 6)")
set_property(CACHE BUILD_FOR_KDE_VERSION PROPERTY STRINGS 5 6)
set(_QT5_MIN_VERSION "5.15.0")
set(_QT6_MIN_VERSION "6.6.2")  # matches Ubuntu oracular (24.10)
set(_KF5_MIN_VERSION "5.101.0")
set(_KF6_MIN_VERSION "6.6.0")  # matches Ubuntu oracular (24.10)

set(KDBG_VERSION 3.2.0)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kdbg/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdbg/version.h)

if(BUILD_FOR_KDE_VERSION EQUAL 6)
    set(KF_MIN_VERSION ${_KF6_MIN_VERSION})
    set(QT_MIN_VERSION ${_QT6_MIN_VERSION})
elseif(BUILD_FOR_KDE_VERSION EQUAL 5)
    set(KF_MIN_VERSION ${_KF5_MIN_VERSION})
    set(QT_MIN_VERSION ${_QT5_MIN_VERSION})
else()
    message(FATAL_ERROR "BUILD_FOR_KDE_VERSION must be \"5\" or \"6\" but not \"${BUILD_FOR_KDE_VERSION}\".")
endif()

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt${BUILD_FOR_KDE_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Widgets
)
if(BUILD_FOR_KDE_VERSION EQUAL 6)
    find_package(Qt${BUILD_FOR_KDE_VERSION}Core5Compat CONFIG REQUIRED)
endif()

find_package(KF${BUILD_FOR_KDE_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS
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
