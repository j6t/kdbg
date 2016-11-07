include(ECMInstallIcons)

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
    16-apps-kdbg.png
    22-actions-pulse000000.png
    22-actions-pulse000001.png
    22-actions-pulse000002.png
    22-actions-pulse000003.png
    22-actions-pulse000004.png
    22-actions-pulse000005.png
    22-actions-pulse000006.png
    22-actions-pulse000007.png
    22-actions-pulse000008.png
    22-actions-pulse000009.png
    22-actions-pulse000010.png
    22-actions-pulse000011.png
    22-actions-pulse000012.png
    22-actions-pulse000013.png
    22-actions-pulse000014.png
    22-actions-pulse000015.png
    22-actions-pulse000016.png
    22-actions-pulse000017.png
    22-actions-pulse000018.png
    22-actions-pulse000019.png
    22-actions-pulse000020.png
    22-actions-pulse000021.png
    22-actions-pulse000022.png
    22-actions-pulse000023.png
    22-actions-pulse000024.png
    22-actions-pulse000025.png
    22-actions-pulse000026.png
    22-actions-pulse000027.png
    22-actions-pulse000028.png
    32-apps-kdbg.png
    48-apps-kdbg.png
    22-actions-pulse.mng
    DESTINATION "${DATA_INSTALL_DIR}/kdbg/icons"
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
    DESTINATION "${DATA_INSTALL_DIR}/kdbg/icons"
    THEME oxygen
)
