
GMOFILES = cs.gmo da.gmo de.gmo \
	es.gmo fr.gmo hr.gmo hu.gmo it.gmo ja.gmo \
	no.gmo no_NY.gmo pl.gmo pt.gmo ro.gmo ru.gmo \
	sk.gmo sr.gmo sv.gmo tr.gmo zh_CN.GB2312.gmo
POFILES = $(GMOFILES:.gmo=.po) 

localedir = $(kde_locale)

MAINTAINERCLEANFILES = $(GMOFILES)

GMSGFMT = @GMSGFMT@
MSGFMT = @MSGFMT@
XGETTEXT = @XGETTEXT@
EXTRA_DIST = $(PACKAGE).pot $(POFILES) $(GMOFILES) kde1-gmo.tar


all: all-@USE_NLS@
install-data-local: install-@USE_NLS@

SUFFIXES = .po .gmo

.po.mo:
	$(MSGFMT) -o $@ $<

if BUILDFORKDE2
.po.gmo:
	file=`echo $* | sed 's,.*/,,'`.gmo \
	&& rm -f $$file && $(GMSGFMT) -o $$file $< 
all-yes: $(GMOFILES)
else
all-yes:
	cd $(srcdir) && tar xf kde1-gmo.tar
endif

all-no:

install-yes: all
	@catalogs='$(GMOFILES)'; \
	for cat in $$catalogs; do \
	destdir="$(DESTDIR)$(localedir)"; \
	lang=`echo $$cat | sed 's/\.gmo//'`; \
	name=$(PACKAGE).mo \
	dir=$$destdir/$$lang/LC_MESSAGES; \
	$(mkinstalldirs) $$dir; \
	$(INSTALL_DATA) $(srcdir)/$$cat $$dir/$$name; \
	  echo "installing $$cat as $$dir/$$name"; \
	done

install-no:

uninstall-local:
	@catalogs='$(GMOFILES)'; \
	for cat in $$catalogs; do \
	destdir="$(DESTDIR)$(localedir)"; \
	lang=`echo $$cat | sed 's/\.gmo//'`; \
	name=$(PACKAGE).mo; \
	dir=$$destdir/$$lang/LC_MESSAGES; \
	rm -f $$cat $$dir/$$name; \
	echo "removing $$dir/$$name" ; \
	done

merge:
	@catalogs='$(POFILES)'; \
	for cat in $$catalogs; do \
		name=$(PACKAGE).pot ; \
		echo $$cat $$name; \
		msgmerge $$cat $$name > $$cat.new ; \
		if cmp $$cat $$cat.new >/dev/null; then \
			rm $$cat.new;  \
		else  \
			mv $$cat.new $$cat ; \
		fi; \
	done

