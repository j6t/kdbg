include(ECMInstallIcons)

# compress SVG files
file(GLOB svgs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_CURRENT_SOURCE_DIR}/sc-*.svg")
set(svgzs)
foreach(svg ${svgs})
	set(svgz "${CMAKE_CURRENT_BINARY_DIR}/${svg}z")
	add_custom_command(
		OUTPUT "${svgz}"
		COMMAND gzip < "${CMAKE_CURRENT_SOURCE_DIR}/${svg}" > "${svgz}"
		DEPENDS "${svg}"
	)
	set(svgzs ${svgzs} "${svgz}")
endforeach(svg)
add_custom_target(generatesvgz ALL DEPENDS ${svgzs})

# line items and other pictograms
set(PICTOGRAMS
	brkena.png
	brkdis.png
	brktmp.png
	brkcond.png
	brkorph.png
	pcinner.png
	pcup.png
	watchena.png
	watchdis.png
	pointer.xpm)

install(FILES ${PICTOGRAMS} DESTINATION "${DATA_INSTALL_DIR}/kdbg/pics")

ecm_install_icons(ICONS
    22-actions-pulse.mng
    DESTINATION "${DATA_INSTALL_DIR}/kdbg/icons"
    THEME hicolor
)
ecm_install_icons(ICONS
    16-apps-kdbg.png
    32-apps-kdbg.png
    48-apps-kdbg.png
    64-apps-kdbg.png
    128-apps-kdbg.png
    ${CMAKE_CURRENT_BINARY_DIR}/sc-apps-kdbg.svgz
    DESTINATION "${DATA_INSTALL_DIR}/icons"
    THEME hicolor
)
ecm_install_icons(ICONS
    16-actions-brkpt.png
    16-actions-debug-run.png
    16-actions-document-open-executable.png
    22-actions-brkpt.png
    22-actions-debug-run.png
    22-actions-document-open-executable.png
    32-actions-brkpt.png
    32-actions-debug-run.png
    32-actions-document-open-executable.png
    48-actions-brkpt.png
    48-actions-debug-run.png
    48-actions-document-open-executable.png
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-brkpt.svgz
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-debug-run.svgz
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-document-open-executable.svgz
    DESTINATION "${DATA_INSTALL_DIR}/kdbg/icons"
    THEME oxygen
)
