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

install(FILES ${PICTOGRAMS} DESTINATION "${KDE_INSTALL_DATADIR}/kdbg/pics")

ecm_install_icons(ICONS
    16-apps-kdbg.png
    32-apps-kdbg.png
    48-apps-kdbg.png
    64-apps-kdbg.png
    128-apps-kdbg.png
    ${CMAKE_CURRENT_BINARY_DIR}/sc-apps-kdbg.svgz
    DESTINATION "${KDE_INSTALL_ICONDIR}"
    THEME hicolor
)
ecm_install_icons(ICONS
    16-actions-brkpt.png
    16-actions-debug-run.png
    16-actions-executable-open.png
    22-actions-brkpt.png
    22-actions-debug-run.png
    22-actions-executable-open.png
    22-actions-pulse.mng
    32-actions-brkpt.png
    32-actions-debug-run.png
    32-actions-executable-open.png
    48-actions-brkpt.png
    48-actions-debug-run.png
    48-actions-executable-open.png
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-brkpt.svgz
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-debug-run.svgz
    ${CMAKE_CURRENT_BINARY_DIR}/sc-actions-executable-open.svgz
    DESTINATION "${KDE_INSTALL_DATADIR}/kdbg/icons"
    THEME hicolor
)
