
# add here all files
data_DATA = \
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

# this is the directory, where all datas are installed
# you have given the datas in data_DATA
datadir = $(kde_htmldir)/ru/kdbg

EXTRA_DIST = $(data_DATA)

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)
	cd $(DESTDIR)$(datadir) && ln -s -f ../../en/kdbg/types.html .

uninstall-local:
	-rm -f $(DESTDIR)$(datadir)/types.html
