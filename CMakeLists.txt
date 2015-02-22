cmake_minimum_required(VERSION 2.6)

set(KDBG_VERSION 2.5.5)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kdbg/version.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kdbg/version.h)

find_package(KDE4 REQUIRED)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

include(KDE4Defaults)
include(MacroLibrary)
#include(ManualStuff.cmake)
#include(ConfigureChecks.cmake)

add_subdirectory(kdbg)
add_subdirectory(po)
