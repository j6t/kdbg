
# add here all files
data_DATA = index.html

# this is the directory, where all datas are installed
# you have given the datas in data_DATA
datadir = $(kde_htmldir)/ru/kdbg

EXTRA_DIST = $(data_DATA)

install-data-local:
	cd $(DESTDIR)$(datadir) && ln -s ../../en/kdbg/types.html .

uninstall-local:
	-rm -f $(DESTDIR)$(datadir)/types.html
