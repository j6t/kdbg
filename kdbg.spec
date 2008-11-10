%define name kdbg
%define version 2.0.0
%define release 1.kde2
%define prefix /usr

%define builddir $RPM_BUILD_DIR/%{name}-%{version}

Summary: KDbg - KDE Debugging GUI around gdb
Name: %{name}
Version: %{version}
Release: %{release}
Prefix: %{prefix}
Group: X11/KDE/Development
Copyright: GPL
Distribution: RedHat 7.0
Vendor: Johannes Sixt <j6t@kdbg.org>
Packager: Ullrich von Bassewitz <uz@musoftware.de>
Source: %{name}-%{version}.tar.gz
URL: http://www.kdbg.org/
Requires: kdelibs >= 2.0
BuildRoot: /tmp/build-%{name}-%{version}

%description
KDbg is a graphical user interface to gdb, the GNU debugger. It provides
an intuitive interface for setting breakpoints, inspecting variables, and
stepping through code.

%prep
rm -rf $RPM_BUILD_ROOT
rm -rf %{builddir}

%setup
touch `find . -type f`

%build
if [ -z "$KDEDIR" ]; then
        export KDEDIR=%{prefix}
fi
CXXFLAGS="$RPM_OPT_FLAGS" CFLAGS="$RPM_OPT_FLAGS" ./configure \
	--prefix=$KDEDIR
make


%install
if [ -z "$KDEDIR" ]; then
	export KDEDIR=%{prefix}
fi
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install-strip

cd $RPM_BUILD_ROOT
find . -type d | sed '1,2d;s,^\.,\%attr(-\,root\,root) \%dir ,' > \
	$RPM_BUILD_DIR/file.list.%{name}
find . -type f | sed -e 's,^\.,\%attr(-\,root\,root) ,' \
	-e '/\/config\//s|^|%config|' >> \
	$RPM_BUILD_DIR/file.list.%{name}
find . -type l | sed 's,^\.,\%attr(-\,root\,root) ,' >> \
	$RPM_BUILD_DIR/file.list.%{name}
echo "%docdir $KDEDIR/share/doc/HTML" >> $RPM_BUILD_DIR/file.list.%{name}


%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{builddir}
rm -f $RPM_BUILD_DIR/file.list.%{name}

%files -f ../file.list.%{name}
