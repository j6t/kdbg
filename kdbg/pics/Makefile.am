## $Id$

# toolbar icons
TOOLBAR = \
	brkpt.xpm \
	pgmfinish.xpm \
	pgmnext.xpm \
	pgmnexti.xpm \
	pgmrun.xpm \
	pgmstep.xpm \
	pgmstepi.xpm \
	execopen.xpm \
	search.xpm

# line items and other pictograms
PICTOGRAMS = \
	brkena.xpm \
	brkdis.xpm \
	brktmp.xpm \
	brkcond.xpm \
	pcinner.xpm \
	pcup.xpm \
	regs.xpm \
	watch.xpm \
	watchcoded.xpm \
	watchena.xpm \
	watchdis.xpm \
	pointer.xpm


KDE2_ICONS = \
	hi32-app-kdbg.png hi16-app-kdbg.png \
	lo32-app-kdbg.png lo16-app-kdbg.png

if BUILDFORKDE2
kde2_icon_install = yes
else
icon_DATA = kdbg.xpm
icondir = $(kde_icondir)
miniicon_DATA = mini-kdbg.xpm
miniicondir = $(kde_icondir)/mini
kde2_icon_install = no
endif
pics_DATA = $(TOOLBAR) $(PICTOGRAMS)
picsdir = $(kde_datadir)/kdbg/pics


EXTRA_DIST = $(pics_DATA) kdbg.xpm mini-kdbg.xpm $(KDE2_ICONS)

install-data-local:
	@if test "x$(kde2_icon_install)" = "xyes"; then \
	set -x; \
	d="$(DESTDIR)$(kde_icondir)"; \
	$(mkinstalldirs) $$d/hicolor/32x32/apps; \
	$(INSTALL_DATA) hi32-app-kdbg.png $$d/hicolor/32x32/apps/kdbg.png; \
	$(mkinstalldirs) $$d/hicolor/16x16/apps; \
	$(INSTALL_DATA) hi16-app-kdbg.png $$d/hicolor/16x16/apps/kdbg.png; \
	$(mkinstalldirs) $$d/locolor/32x32/apps; \
	$(INSTALL_DATA) lo32-app-kdbg.png $$d/locolor/32x32/apps/kdbg.png; \
	$(mkinstalldirs) $$d/locolor/16x16/apps; \
	$(INSTALL_DATA) lo16-app-kdbg.png $$d/locolor/16x16/apps/kdbg.png; \
	fi

uninstall-local:
	-rm -f $(DESTDIR)$(kde_icondir)/hicolor/32x32/apps/kdbg.png
	-rm -f $(DESTDIR)$(kde_icondir)/hicolor/16x16/apps/kdbg.png
	-rm -f $(DESTDIR)$(kde_icondir)/locolor/32x32/apps/kdbg.png
	-rm -f $(DESTDIR)$(kde_icondir)/locolor/16x16/apps/kdbg.png
