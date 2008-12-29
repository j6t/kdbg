# line items and other pictograms
set(PICTOGRAMS
	brkena.xpm
	brkdis.xpm
	brktmp.xpm
	brkcond.xpm
	brkorph.xpm
	pcinner.xpm
	pcup.xpm
	regs.xpm
	watch.xpm
	watchcoded.xpm
	watchena.xpm
	watchdis.xpm
	pointer.xpm)

install(FILES $(PICTOGRAMS) DESTINATION ${DATA_INSTALL_DIR}/kdbg/pics)
install(FILES DESTINATION ${DATA_INSTALL_DIR}/kdbg/icons)

kde4_install_icons(${ICON_INSTALL_DIR})
