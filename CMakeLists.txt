cmake_minimum_required(VERSION 2.6)

find_package(KDE3 REQUIRED)

add_definitions(${QT_DEFINITIONS} ${KDE3_DEFINITIONS})

link_directories(${KDE3_LIB_DIR})

add_subdirectory(kdbg)
add_subdirectory(po)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE3_INCLUDE_DIR} ${QT_INCLUDE_DIR} )
