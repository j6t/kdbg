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

install(FILES $(PICTOGRAMS) DESTINATION /share/apps/kdbg/pics)
install(FILES DESTINATION /share/apps/kdbg/icons)

kde3_install_icons(hicolor)
