add_subdirectory(doc)
add_subdirectory(pics)
add_subdirectory(typetables)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE3_INCLUDE_DIR} ${QT_INCLUDE_DIR} )

CHECK_INCLUDE_FILES(pty.h HAVE_PTY_H)
CHECK_INCLUDE_FILES(libutil.h HAVE_LIBUTIL_H)
CHECK_INCLUDE_FILES(util.h HAVE_UTIL_H)
CHECK_LIBRARY_EXISTS(util openpty "" HAVE_LIB_UTIL)
CHECK_FUNCTION_EXISTS(openpty HAVE_FUNC_OPENPTY)

set(PS_COMMAND \"/usr/bin/ps\",\"-eo\",\"pid,ppid,uid,vsz,etime,time,args\")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(kdbg_SRCS
    pgmargs.cpp
    procattach.cpp
    debugger.cpp
    programconfig.cpp
    dbgdriver.cpp
    gdbdriver.cpp
    xsldbgdriver.cpp
    brkpt.cpp
    exprwnd.cpp
    regwnd.cpp
    memwindow.cpp
    threadlist.cpp
    sourcewnd.cpp
    winstack.cpp
    ttywnd.cpp
    typetable.cpp
    prefdebugger.cpp
    prefmisc.cpp
    pgmsettings.cpp
    watchwindow.cpp
    dbgmainwnd.cpp
    main.cpp
)

kde3_automoc(${kdbg_SRCS})

set(kdbg_UI
    pgmargsbase.ui
    procattachbase.ui
)

kde3_add_ui_files(kdbg_SRCS ${kdbg_UI})

kde3_add_executable(kdbg ${kdbg_SRCS})

IF (HAVE_LIB_UTIL)
    set(LIB_UTIL util)
ENDIF (HAVE_LIB_UTIL)

target_link_libraries(kdbg ${QT_AND_KDECORE_LIBS} kio ${LIB_UTIL})

install(TARGETS kdbg DESTINATION bin)
install(FILES kdbg.desktop DESTINATION /share/applications/kde)
install(FILES kdbgrc DESTINATION /share/config)
install(FILES kdbgui.rc DESTINATION /share/apps/kdbg)
