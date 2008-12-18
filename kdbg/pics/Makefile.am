# toolbar icons

kdbgiconsdir = $(kde_datadir)/kdbg/icons
kdbgicons_ICON = \
	pulse \
	brkpt \
	pgmfinish \
	pgmnext \
	pgmnexti \
	pgmrun \
	pgmstep \
	pgmstepi \
	execopen

# line items and other pictograms
PICTOGRAMS = \
	brkena.xpm \
	brkdis.xpm \
	brktmp.xpm \
	brkcond.xpm \
	brkorph.xpm \
	pcinner.xpm \
	pcup.xpm \
	regs.xpm \
	watch.xpm \
	watchcoded.xpm \
	watchena.xpm \
	watchdis.xpm \
	pointer.xpm


pics_DATA = $(PICTOGRAMS)
picsdir = $(kde_datadir)/kdbg/pics

# application icon
KDE_ICON = kdbg

animation = $(shell cd "$(srcdir)" && echo hi22-action-pulse0000*.png)

# this requires ImageMagick
hi22-action-pulse.png: hi22-action-pulse.pngseq
	list="$(animation)" ; \
	montage +frame +shadow +label -background black \
		-geometry 22x22 \
		-tile 1x`echo $$list | wc -w` \
		$$list hi22-action-pulse.png

hi22-action-pulse.pngseq: hi22-action-pulse.xml
	@echo "Using cinelerra render $< into a PNG sequence $@"
	@echo "If you do not have cinelerra, just touch $@"
	exit 1

# This file can be rendered by cinelerra into a "PNG sequence".
# This produces the PNG sequence $(animation), which is pasted together
# by `make hi22-action-pulse.png'.
EXTRA_DIST = hi22-action-pulse.xml $(animation)
