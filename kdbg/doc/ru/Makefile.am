html_DATA = \
	argspwdenv.html \
	breakptlist.html \
	globaloptions.html \
	howdoi.html \
	localvars.html \
	memory.html \
	pgmoutput.html \
	pgmsettings.html \
	registers.html \
	sourcecode.html \
	stack.html \
	threads.html \
	tips.html \
	watches.html \
	index.html

htmldir = $(kde_htmldir)/ru/kdbg

EXTRA_DIST = $(html_DATA)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(htmldir)
	cd $(DESTDIR)$(htmldir) && ln -s -f ../../en/kdbg/types.html .

uninstall-local:
	-rm -f $(DESTDIR)$(htmldir)/types.html
