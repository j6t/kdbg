add_subdirectory(doc)
add_subdirectory(pics)
add_subdirectory(typetables)

include_directories(${KDE4_INCLUDES} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )
include(CheckFunctionExists)

CHECK_INCLUDE_FILES(pty.h HAVE_PTY_H)
CHECK_INCLUDE_FILES(libutil.h HAVE_LIBUTIL_H)
CHECK_INCLUDE_FILES(util.h HAVE_UTIL_H)
CHECK_LIBRARY_EXISTS(util openpty "" HAVE_LIB_UTIL)
if (HAVE_LIB_UTIL)
    set(CMAKE_REQUIRED_LIBRARIES util)
endif (HAVE_LIB_UTIL)
CHECK_FUNCTION_EXISTS(openpty HAVE_FUNC_OPENPTY)

message("-- Looking for a suitable 'ps' invocation")
FIND_PROGRAM(PROG_PS ps)
IF (PROG_PS)
    set(PS_ARGS -eo pid,ppid,uid,vsz,etime,time,args)
    execute_process(COMMAND ${PROG_PS} ${PS_ARGS}
		RESULT_VARIABLE PS_FAILED
		OUTPUT_QUIET ERROR_QUIET)
    IF (NOT PS_FAILED)
	execute_process(
		COMMAND ${PROG_PS} ${PS_ARGS}
		COMMAND sed -e "s/  */ /g" -e 1q
		OUTPUT_VARIABLE PS_HEADER)
	string(STRIP "${PS_HEADER}" PS_HEADER)
	IF (PS_HEADER STREQUAL "PID PPID UID VSZ ELAPSED TIME COMMAND")
	    # enclose arguments in double-quotes
	    set(PS_COMMAND \"${PROG_PS}\")
	    set(PS_MSG ${PROG_PS})
	    foreach (I ${PS_ARGS})
		set(PS_COMMAND ${PS_COMMAND},\"${I}\")
		set(PS_MSG "${PS_MSG} ${I}")
	    endforeach (I)
	    message("-- Found 'ps' command: ${PS_MSG}")
	ENDIF (PS_HEADER STREQUAL "PID PPID UID VSZ ELAPSED TIME COMMAND")
    ENDIF (NOT PS_FAILED)
ENDIF (PROG_PS)
IF (NOT PS_COMMAND)
    message("-- Looking for a suitable 'ps' invocation - not found")
ENDIF (NOT PS_COMMAND)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)

set(kdbg_SRCS
    pgmargs.cpp
    procattach.cpp
    debugger.cpp
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

set(kdbg_UI
    brkptbase.ui
    brkptcondition.ui
    pgmargsbase.ui
    procattachbase.ui
)

kde4_add_ui_files(kdbg_SRCS ${kdbg_UI})

kde4_add_app_icon(kdbg_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/pics/hi*-app-kdbg.png")
kde4_add_app_icon(kdbg_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/pics/lo*-app-kdbg.png")
kde4_add_executable(kdbg ${kdbg_SRCS})

IF (HAVE_LIB_UTIL)
    set(LIB_UTIL util)
ENDIF (HAVE_LIB_UTIL)

target_link_libraries(kdbg ${KDE4_KDECORE_LIBS} ${KDE4_KIO_LIBS} ${LIB_UTIL})

install(TARGETS kdbg ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES kdbg.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES kdbgrc DESTINATION ${CONFIG_INSTALL_DIR})
install(FILES kdbgui.rc DESTINATION ${DATA_INSTALL_DIR}/kdbg)
