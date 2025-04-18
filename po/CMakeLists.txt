FIND_PROGRAM(GETTEXT_MSGFMT_EXECUTABLE msgfmt)

IF(NOT GETTEXT_MSGFMT_EXECUTABLE)
	MESSAGE(
"------
		NOTE: msgfmt not found. Translations will *not* be installed
------")
ELSE(NOT GETTEXT_MSGFMT_EXECUTABLE)

	SET(catalogname kdbg)

	FILE(GLOB PO_FILES *.po)
	SET(GMO_FILES)

	FOREACH(_poFile ${PO_FILES})
		GET_FILENAME_COMPONENT(_lang ${_poFile} NAME_WE)
		SET(_gmoFile ${CMAKE_CURRENT_BINARY_DIR}/${_lang}.gmo)

		ADD_CUSTOM_COMMAND(OUTPUT ${_gmoFile}
			COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${_gmoFile} ${_poFile}
			DEPENDS ${_poFile})
		INSTALL(FILES ${_gmoFile}
			DESTINATION ${KDE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES/
			RENAME ${catalogname}.mo)
		LIST(APPEND GMO_FILES ${_gmoFile})

	ENDFOREACH(_poFile ${PO_FILES})

	ADD_CUSTOM_TARGET(translations ALL DEPENDS ${GMO_FILES})

ENDIF(NOT GETTEXT_MSGFMT_EXECUTABLE)
